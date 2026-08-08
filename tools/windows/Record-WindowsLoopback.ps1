param(
    [int]$Seconds = 60,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path (Get-Location) "windows-loopback-captures"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$outFile = Join-Path $OutDir "windows-loopback-$stamp.wav"

$cs = @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public static class WasapiLoopbackRecorder
{
    enum EDataFlow { eRender, eCapture, eAll }
    enum ERole { eConsole, eMultimedia, eCommunications }
    enum AUDCLNT_SHAREMODE { AUDCLNT_SHAREMODE_SHARED, AUDCLNT_SHAREMODE_EXCLUSIVE }

    const int CLSCTX_ALL = 23;
    const int AUDCLNT_STREAMFLAGS_LOOPBACK = 0x00020000;
    const int AUDCLNT_BUFFERFLAGS_SILENT = 0x00000002;

    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    class MMDeviceEnumerator { }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("A95664D2-9614-4F35-A746-DE8DB63617E6")]
    interface IMMDeviceEnumerator
    {
        int EnumAudioEndpoints(EDataFlow dataFlow, int dwStateMask, out IntPtr ppDevices);
        int GetDefaultAudioEndpoint(EDataFlow dataFlow, ERole role, out IMMDevice ppEndpoint);
        int GetDevice([MarshalAs(UnmanagedType.LPWStr)] string pwstrId, out IMMDevice ppDevice);
        int RegisterEndpointNotificationCallback(IntPtr pClient);
        int UnregisterEndpointNotificationCallback(IntPtr pClient);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("D666063F-1587-4E43-81F1-B948E807363F")]
    interface IMMDevice
    {
        int Activate(ref Guid iid, int dwClsCtx, IntPtr pActivationParams, out IAudioClient ppInterface);
        int OpenPropertyStore(int stgmAccess, out IntPtr ppProperties);
        int GetId(out IntPtr ppstrId);
        int GetState(out int pdwState);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2")]
    interface IAudioClient
    {
        int Initialize(AUDCLNT_SHAREMODE ShareMode, int StreamFlags, long hnsBufferDuration, long hnsPeriodicity, IntPtr pFormat, IntPtr AudioSessionGuid);
        int GetBufferSize(out uint pNumBufferFrames);
        int GetStreamLatency(out long phnsLatency);
        int GetCurrentPadding(out uint pNumPaddingFrames);
        int IsFormatSupported(AUDCLNT_SHAREMODE ShareMode, IntPtr pFormat, out IntPtr ppClosestMatch);
        int GetMixFormat(out IntPtr ppDeviceFormat);
        int GetDevicePeriod(out long phnsDefaultDevicePeriod, out long phnsMinimumDevicePeriod);
        int Start();
        int Stop();
        int Reset();
        int SetEventHandle(IntPtr eventHandle);
        int GetService(ref Guid riid, out IAudioCaptureClient ppv);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317")]
    interface IAudioCaptureClient
    {
        int GetBuffer(out IntPtr ppData, out uint pNumFramesToRead, out int pdwFlags, out long pu64DevicePosition, out long pu64QPCPosition);
        int ReleaseBuffer(uint NumFramesRead);
        int GetNextPacketSize(out uint pNumFramesInNextPacket);
    }

    [StructLayout(LayoutKind.Sequential)]
    struct WAVEFORMATEX
    {
        public ushort wFormatTag;
        public ushort nChannels;
        public uint nSamplesPerSec;
        public uint nAvgBytesPerSec;
        public ushort nBlockAlign;
        public ushort wBitsPerSample;
        public ushort cbSize;
    }

    [DllImport("ole32.dll")]
    static extern void CoTaskMemFree(IntPtr pv);

    public static void Record(string path, int seconds)
    {
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        int hr = enumerator.GetDefaultAudioEndpoint(EDataFlow.eRender, ERole.eMultimedia, out device);
        Check(hr, "GetDefaultAudioEndpoint");

        Guid audioClientGuid = typeof(IAudioClient).GUID;
        IAudioClient audioClient;
        hr = device.Activate(ref audioClientGuid, CLSCTX_ALL, IntPtr.Zero, out audioClient);
        Check(hr, "Activate IAudioClient");

        IntPtr fmtPtr;
        hr = audioClient.GetMixFormat(out fmtPtr);
        Check(hr, "GetMixFormat");
        var fmt = Marshal.PtrToStructure<WAVEFORMATEX>(fmtPtr);

        bool isFloat = fmt.wFormatTag == 3;
        bool isExtensible = fmt.wFormatTag == 0xFFFE && fmt.cbSize >= 22;
        if (isExtensible)
        {
            byte[] ext = new byte[fmt.cbSize];
            Marshal.Copy(fmtPtr + 18, ext, 0, ext.Length);
            // WAVE_FORMAT_EXTENSIBLE subformat GUID starts after validbits(2)+channelmask(4).
            byte first = ext[6];
            if (first == 0x03) isFloat = true;
        }

        int channels = fmt.nChannels;
        int srcBits = fmt.wBitsPerSample;
        int srcBytesPerFrame = fmt.nBlockAlign;
        int dstBytesPerFrame = channels * 2;
        if (!(isFloat && srcBits == 32) && !(srcBits == 16))
            throw new NotSupportedException("Unsupported WASAPI mix format: tag=" + fmt.wFormatTag + " channels=" + channels + " bits=" + srcBits);

        long bufferDuration = 10000000; // 1 second, in 100 ns units
        hr = audioClient.Initialize(AUDCLNT_SHAREMODE.AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0, fmtPtr, IntPtr.Zero);
        CoTaskMemFree(fmtPtr);
        Check(hr, "Initialize loopback");

        Guid captureGuid = typeof(IAudioCaptureClient).GUID;
        IAudioCaptureClient captureClient;
        hr = audioClient.GetService(ref captureGuid, out captureClient);
        Check(hr, "GetService IAudioCaptureClient");

        using (var ms = new MemoryStream())
        {
            hr = audioClient.Start();
            Check(hr, "Start");
            var end = DateTime.UtcNow.AddSeconds(seconds);
            try
            {
                while (DateTime.UtcNow < end)
                {
                    uint packetFrames;
                    hr = captureClient.GetNextPacketSize(out packetFrames);
                    Check(hr, "GetNextPacketSize");
                    if (packetFrames == 0)
                    {
                        Thread.Sleep(10);
                        continue;
                    }

                    while (packetFrames > 0)
                    {
                        IntPtr data;
                        uint frames;
                        int flags;
                        long pos, qpc;
                        hr = captureClient.GetBuffer(out data, out frames, out flags, out pos, out qpc);
                        Check(hr, "GetBuffer");

                        int srcBytes = checked((int)(frames * srcBytesPerFrame));
                        byte[] src = new byte[srcBytes];
                        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
                            Marshal.Copy(data, src, 0, srcBytes);

                        if (isFloat)
                        {
                            for (int i = 0; i < srcBytes; i += 4)
                            {
                                float v = BitConverter.ToSingle(src, i);
                                if (v > 1f) v = 1f;
                                if (v < -1f) v = -1f;
                                short s = (short)Math.Round(v * 32767f);
                                byte[] b = BitConverter.GetBytes(s);
                                ms.Write(b, 0, 2);
                            }
                        }
                        else
                        {
                            ms.Write(src, 0, srcBytes);
                        }

                        hr = captureClient.ReleaseBuffer(frames);
                        Check(hr, "ReleaseBuffer");
                        hr = captureClient.GetNextPacketSize(out packetFrames);
                        Check(hr, "GetNextPacketSize");
                    }
                }
            }
            finally
            {
                audioClient.Stop();
            }

            File.WriteAllBytes(path, BuildWave(ms.ToArray(), channels, (int)fmt.nSamplesPerSec, 16, dstBytesPerFrame));
        }
    }

    static byte[] BuildWave(byte[] pcm, int channels, int sampleRate, int bits, int blockAlign)
    {
        using (var outMs = new MemoryStream())
        using (var bw = new BinaryWriter(outMs))
        {
            int byteRate = sampleRate * blockAlign;
            bw.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));
            bw.Write(36 + pcm.Length);
            bw.Write(System.Text.Encoding.ASCII.GetBytes("WAVE"));
            bw.Write(System.Text.Encoding.ASCII.GetBytes("fmt "));
            bw.Write(16);
            bw.Write((ushort)1);
            bw.Write((ushort)channels);
            bw.Write(sampleRate);
            bw.Write(byteRate);
            bw.Write((ushort)blockAlign);
            bw.Write((ushort)bits);
            bw.Write(System.Text.Encoding.ASCII.GetBytes("data"));
            bw.Write(pcm.Length);
            bw.Write(pcm);
            return outMs.ToArray();
        }
    }

    static void Check(int hr, string where)
    {
        if (hr < 0) Marshal.ThrowExceptionForHR(hr);
    }
}
'@

Add-Type -TypeDefinition $cs -Language CSharp
Write-Host "Recording Windows speaker loopback for $Seconds seconds..."
Write-Host "Output: $outFile"
[WasapiLoopbackRecorder]::Record($outFile, $Seconds)
Write-Host "Saved: $outFile"
