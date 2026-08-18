param(
    [int]$Seconds = 60,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path (Get-Location) "external-mic-raw-captures"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$outFile = Join-Path $OutDir "external-mic-raw-$stamp.wav"

$cs = @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public static class WasapiRawCaptureRecorder
{
    enum EDataFlow { eRender, eCapture, eAll }
    enum ERole { eConsole, eMultimedia, eCommunications }
    enum AUDCLNT_SHAREMODE { Shared, Exclusive }

    const int CLSCTX_ALL = 23;
    const int AUDCLNT_BUFFERFLAGS_SILENT = 0x2;
    const uint AUDCLNT_STREAMOPTIONS_RAW = 0x1;

    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    class MMDeviceEnumerator { }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("A95664D2-9614-4F35-A746-DE8DB63617E6")]
    interface IMMDeviceEnumerator
    {
        int EnumAudioEndpoints(EDataFlow flow, int stateMask, out IntPtr devices);
        int GetDefaultAudioEndpoint(EDataFlow flow, ERole role, out IMMDevice device);
        int GetDevice([MarshalAs(UnmanagedType.LPWStr)] string id, out IMMDevice device);
        int RegisterEndpointNotificationCallback(IntPtr client);
        int UnregisterEndpointNotificationCallback(IntPtr client);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("D666063F-1587-4E43-81F1-B948E807363F")]
    interface IMMDevice
    {
        int Activate(ref Guid iid, int clsctx, IntPtr activationParams,
                     out IAudioClient2 client);
        int OpenPropertyStore(int mode, out IntPtr properties);
        int GetId(out IntPtr id);
        int GetState(out int state);
    }

    /*
     * Flatten IAudioClient + IAudioClient2 into one COM interface definition.
     * C# COM-interface inheritance can shift the derived-method vtable slots;
     * the flattened declaration matches audioclient.h ordering directly.
     */
    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("726778CD-F60A-4EDA-82DE-E47610CD78AA")]
    interface IAudioClient2
    {
        int Initialize(AUDCLNT_SHAREMODE mode, int flags, long duration,
                       long periodicity, IntPtr format, IntPtr sessionGuid);
        int GetBufferSize(out uint frames);
        int GetStreamLatency(out long latency);
        int GetCurrentPadding(out uint padding);
        int IsFormatSupported(AUDCLNT_SHAREMODE mode, IntPtr format,
                              out IntPtr closest);
        int GetMixFormat(out IntPtr format);
        int GetDevicePeriod(out long defaultPeriod, out long minPeriod);
        int Start();
        int Stop();
        int Reset();
        int SetEventHandle(IntPtr handle);
        int GetService(ref Guid iid, out IAudioCaptureClient capture);
        int IsOffloadCapable(int category,
            [MarshalAs(UnmanagedType.Bool)] out bool capable);
        int SetClientProperties(ref AudioClientProperties properties);
        int GetBufferSizeLimits(IntPtr format,
            [MarshalAs(UnmanagedType.Bool)] bool eventDriven,
            out long minDuration, out long maxDuration);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317")]
    interface IAudioCaptureClient
    {
        int GetBuffer(out IntPtr data, out uint frames, out int flags,
                      out long devicePosition, out long qpcPosition);
        int ReleaseBuffer(uint frames);
        int GetNextPacketSize(out uint frames);
    }

    [StructLayout(LayoutKind.Sequential)]
    struct AudioClientProperties
    {
        public uint cbSize;
        public int bIsOffload;
        public int eCategory;
        public uint Options;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct WAVEFORMATEX
    {
        public ushort tag;
        public ushort channels;
        public uint rate;
        public uint avg;
        public ushort align;
        public ushort bits;
        public ushort cb;
    }

    [DllImport("ole32.dll")]
    static extern void CoTaskMemFree(IntPtr p);

    static void Check(int hr, string where)
    {
        if (hr < 0)
            throw new COMException(where + " hr=0x" + hr.ToString("X8"), hr);
    }

    public static string Record(string path, int seconds)
    {
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        Check(enumerator.GetDefaultAudioEndpoint(EDataFlow.eCapture,
              ERole.eMultimedia, out device), "GetDefaultAudioEndpoint");

        Guid clientIid = typeof(IAudioClient2).GUID;
        IAudioClient2 client;
        Check(device.Activate(ref clientIid, CLSCTX_ALL, IntPtr.Zero,
              out client), "Activate IAudioClient2");

        var properties = new AudioClientProperties {
            cbSize = (uint)Marshal.SizeOf(typeof(AudioClientProperties)),
            bIsOffload = 0,
            eCategory = 0,
            Options = AUDCLNT_STREAMOPTIONS_RAW,
        };
        Check(client.SetClientProperties(ref properties),
              "SetClientProperties RAW");

        IntPtr formatPtr;
        Check(client.GetMixFormat(out formatPtr), "GetMixFormat");
        var format = Marshal.PtrToStructure<WAVEFORMATEX>(formatPtr);

        bool isFloat = format.tag == 3;
        bool isExtensible = format.tag == 0xFFFE && format.cb >= 22;
        if (isExtensible) {
            byte[] ext = new byte[format.cb];
            Marshal.Copy(formatPtr + 18, ext, 0, ext.Length);
            if (ext[6] == 3)
                isFloat = true;
        }

        int channels = format.channels;
        int bits = format.bits;
        int sourceAlign = format.align;
        if (!(isFloat && bits == 32) && bits != 16)
            throw new NotSupportedException("format tag=" + format.tag +
                " ch=" + channels + " bits=" + bits);

        Check(client.Initialize(AUDCLNT_SHAREMODE.Shared, 0, 10000000, 0,
              formatPtr, IntPtr.Zero), "Initialize RAW shared");
        CoTaskMemFree(formatPtr);

        Guid captureIid = typeof(IAudioCaptureClient).GUID;
        IAudioCaptureClient capture;
        Check(client.GetService(ref captureIid, out capture), "GetService");

        using (var pcm = new MemoryStream()) {
            Check(client.Start(), "Start");
            var end = DateTime.UtcNow.AddSeconds(seconds);
            try {
                while (DateTime.UtcNow < end) {
                    uint packetFrames;
                    Check(capture.GetNextPacketSize(out packetFrames),
                          "GetNextPacketSize");
                    if (packetFrames == 0) {
                        Thread.Sleep(5);
                        continue;
                    }

                    while (packetFrames > 0) {
                        IntPtr data;
                        uint frames;
                        int flags;
                        long devicePosition, qpcPosition;
                        Check(capture.GetBuffer(out data, out frames, out flags,
                              out devicePosition, out qpcPosition), "GetBuffer");
                        int bytes = checked((int)(frames * sourceAlign));
                        byte[] source = new byte[bytes];
                        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
                            Marshal.Copy(data, source, 0, bytes);

                        if (isFloat) {
                            for (int i = 0; i < bytes; i += 4) {
                                float value = BitConverter.ToSingle(source, i);
                                if (value > 1f) value = 1f;
                                if (value < -1f) value = -1f;
                                short sample = (short)Math.Round(value * 32767f);
                                pcm.Write(BitConverter.GetBytes(sample), 0, 2);
                            }
                        } else {
                            pcm.Write(source, 0, bytes);
                        }

                        Check(capture.ReleaseBuffer(frames), "ReleaseBuffer");
                        Check(capture.GetNextPacketSize(out packetFrames),
                              "GetNextPacketSize");
                    }
                }
            } finally {
                client.Stop();
            }

            WriteWave(path, pcm.ToArray(), channels, (int)format.rate);
        }

        return "RAW format tag=" + format.tag + " channels=" + channels +
               " rate=" + format.rate + " bits=" + bits;
    }

    static void WriteWave(string path, byte[] pcm, int channels, int sampleRate)
    {
        using (var output = new MemoryStream())
        using (var writer = new BinaryWriter(output)) {
            int blockAlign = channels * 2;
            int byteRate = sampleRate * blockAlign;
            writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));
            writer.Write(36 + pcm.Length);
            writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt "));
            writer.Write(16);
            writer.Write((ushort)1);
            writer.Write((ushort)channels);
            writer.Write(sampleRate);
            writer.Write(byteRate);
            writer.Write((ushort)blockAlign);
            writer.Write((ushort)16);
            writer.Write(System.Text.Encoding.ASCII.GetBytes("data"));
            writer.Write(pcm.Length);
            writer.Write(pcm);
            File.WriteAllBytes(path, output.ToArray());
        }
    }
}
'@

Add-Type -TypeDefinition $cs -Language CSharp
Write-Host "Recording RAW external microphone for $Seconds seconds..."
$message = [WasapiRawCaptureRecorder]::Record($outFile, $Seconds)
Write-Host $message
Write-Host "Saved: $outFile"
