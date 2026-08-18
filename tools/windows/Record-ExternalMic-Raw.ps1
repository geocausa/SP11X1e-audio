param(
    [int]$Seconds = 60,
    [string]$OutDir = "",
    [double]$ExpectedEndpointDb = [double]::NaN
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
    public static DateTime StartUtc;
    public static DateTime StopUtc;
    public static string EndpointId;
    public static float EndpointVolumeDb;
    public static float EndpointVolumeScalar;
    public static bool EndpointMute;
    public static uint EndpointHardwareSupport;

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
                     [MarshalAs(UnmanagedType.IUnknown)] out object activated);
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
     Guid("5CDF2C82-841E-4546-9722-0CF74078229A")]
    interface IAudioEndpointVolume
    {
        int RegisterControlChangeNotify(IntPtr notify);
        int UnregisterControlChangeNotify(IntPtr notify);
        int GetChannelCount(out uint count);
        int SetMasterVolumeLevel(float db, Guid context);
        int SetMasterVolumeLevelScalar(float scalar, Guid context);
        int GetMasterVolumeLevel(out float db);
        int GetMasterVolumeLevelScalar(out float scalar);
        int SetChannelVolumeLevel(uint channel, float db, Guid context);
        int SetChannelVolumeLevelScalar(uint channel, float scalar, Guid context);
        int GetChannelVolumeLevel(uint channel, out float db);
        int GetChannelVolumeLevelScalar(uint channel, out float scalar);
        int SetMute([MarshalAs(UnmanagedType.Bool)] bool muted, Guid context);
        int GetMute([MarshalAs(UnmanagedType.Bool)] out bool muted);
        int GetVolumeStepInfo(out uint step, out uint stepCount);
        int VolumeStepUp(Guid context);
        int VolumeStepDown(Guid context);
        int QueryHardwareSupport(out uint mask);
        int GetVolumeRange(out float minDb, out float maxDb, out float incrementDb);
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

    public static void ProbeEndpoint()
    {
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        Check(enumerator.GetDefaultAudioEndpoint(EDataFlow.eCapture,
              ERole.eMultimedia, out device), "GetDefaultAudioEndpoint");

        IntPtr idPtr;
        Check(device.GetId(out idPtr), "GetId");
        try { EndpointId = Marshal.PtrToStringUni(idPtr); }
        finally { CoTaskMemFree(idPtr); }

        Guid volumeIid = typeof(IAudioEndpointVolume).GUID;
        object volumeObject;
        Check(device.Activate(ref volumeIid, CLSCTX_ALL, IntPtr.Zero,
              out volumeObject), "Activate IAudioEndpointVolume");
        var volume = (IAudioEndpointVolume)volumeObject;
        Check(volume.GetMasterVolumeLevel(out EndpointVolumeDb),
              "GetMasterVolumeLevel");
        Check(volume.GetMasterVolumeLevelScalar(out EndpointVolumeScalar),
              "GetMasterVolumeLevelScalar");
        Check(volume.GetMute(out EndpointMute), "GetMute");
        Check(volume.QueryHardwareSupport(out EndpointHardwareSupport),
              "QueryHardwareSupport");
    }

    public static string Record(string path, int seconds)
    {
        ProbeEndpoint();
        var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        IMMDevice device;
        Check(enumerator.GetDefaultAudioEndpoint(EDataFlow.eCapture,
              ERole.eMultimedia, out device), "GetDefaultAudioEndpoint");

        Guid clientIid = typeof(IAudioClient2).GUID;
        object clientObject;
        Check(device.Activate(ref clientIid, CLSCTX_ALL, IntPtr.Zero,
              out clientObject), "Activate IAudioClient2");
        var client = (IAudioClient2)clientObject;

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
            StartUtc = DateTime.UtcNow;
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
                StopUtc = DateTime.UtcNow;
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
[WasapiRawCaptureRecorder]::ProbeEndpoint()
$endpointDb = [double][WasapiRawCaptureRecorder]::EndpointVolumeDb
Write-Host ("Capture endpoint: {0}" -f [WasapiRawCaptureRecorder]::EndpointId)
Write-Host ("Capture gain: {0:F3} dB (scalar={1:R}, mute={2}, hw=0x{3:X})" -f `
    $endpointDb, [WasapiRawCaptureRecorder]::EndpointVolumeScalar, `
    [WasapiRawCaptureRecorder]::EndpointMute, [WasapiRawCaptureRecorder]::EndpointHardwareSupport)
if (-not [double]::IsNaN($ExpectedEndpointDb) -and `
    [math]::Abs($endpointDb - $ExpectedEndpointDb) -gt 0.01) {
    throw ("capture endpoint gain mismatch: actual={0:F3} dB expected={1:F3} dB" -f `
        $endpointDb, $ExpectedEndpointDb)
}

Write-Host "Recording RAW external microphone for $Seconds seconds..."
$message = [WasapiRawCaptureRecorder]::Record($outFile, $Seconds)
$meta = [ordered]@{
    wav = $outFile
    capture_mode = "WASAPI shared + AUDCLNT_STREAMOPTIONS_RAW"
    endpoint_id = [WasapiRawCaptureRecorder]::EndpointId
    endpoint_gain_db = [double][WasapiRawCaptureRecorder]::EndpointVolumeDb
    endpoint_gain_scalar = [double][WasapiRawCaptureRecorder]::EndpointVolumeScalar
    endpoint_mute = [bool][WasapiRawCaptureRecorder]::EndpointMute
    endpoint_hardware_support = ("0x{0:X}" -f [WasapiRawCaptureRecorder]::EndpointHardwareSupport)
    start_utc = [WasapiRawCaptureRecorder]::StartUtc.ToString("o")
    stop_utc = [WasapiRawCaptureRecorder]::StopUtc.ToString("o")
    requested_seconds = $Seconds
}
$metaFile = $outFile + ".metadata.json"
$meta | ConvertTo-Json -Depth 3 | Set-Content -Path $metaFile -Encoding ASCII
Write-Host $message
Write-Host ("StartUtc={0}" -f $meta.start_utc)
Write-Host ("StopUtc={0}" -f $meta.stop_utc)
Write-Host "Saved: $outFile"
Write-Host "Metadata: $metaFile"
