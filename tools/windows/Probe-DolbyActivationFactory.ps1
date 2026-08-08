param(
    [Parameter(Mandatory = $true)]
    [string]$DllPath,

    [Parameter(Mandatory = $true)]
    [string[]]$ClassName,

    [int]$ObjectScanBytes = 256,
    [int]$VtableSlots = 14,

    [string[]]$InterfaceId = @()
)

$ErrorActionPreference = 'Stop'
$DllPath = (Resolve-Path -LiteralPath $DllPath).Path

function Get-PeSizeOfImage([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 0x100) { throw 'File is too small to be a PE image.' }
    $lfanew = [BitConverter]::ToInt32($bytes, 0x3c)
    $sig = [BitConverter]::ToUInt32($bytes, $lfanew)
    if ($sig -ne 0x00004550) { throw 'Missing PE signature.' }
    return [BitConverter]::ToUInt32($bytes, $lfanew + 0x50)
}

if (-not ('DolbyActivation.Native' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace DolbyActivation {
    public static class Native {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern IntPtr LoadLibraryW(string path);

        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
        public static extern IntPtr GetProcAddress(IntPtr module, string name);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool FreeLibrary(IntPtr module);

        [DllImport("combase.dll")]
        public static extern int WindowsCreateString(
            [MarshalAs(UnmanagedType.LPWStr)] string sourceString,
            int length,
            out IntPtr hstring);

        [DllImport("combase.dll")]
        public static extern int WindowsDeleteString(IntPtr hstring);
    }

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate int DllGetActivationFactoryDelegate(IntPtr classId, out IntPtr factory);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    public delegate int ActivateInstanceDelegate(IntPtr self, out IntPtr instance);
}
'@
}

$sizeOfImage = [uint64](Get-PeSizeOfImage $DllPath)
$hash = (Get-FileHash -LiteralPath $DllPath -Algorithm SHA256).Hash
$module = [DolbyActivation.Native]::LoadLibraryW($DllPath)
if ($module -eq [IntPtr]::Zero) {
    throw "LoadLibraryW failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
}

try {
    $getFactoryPtr = [DolbyActivation.Native]::GetProcAddress($module, 'DllGetActivationFactory')
    if ($getFactoryPtr -eq [IntPtr]::Zero) {
        throw 'DllGetActivationFactory export not found.'
    }

    $getFactory = [Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(
        $getFactoryPtr,
        [type][DolbyActivation.DllGetActivationFactoryDelegate])

    $base = [uint64]$module.ToInt64()
    $end = $base + $sizeOfImage

    Write-Output ("DLL={0}" -f $DllPath)
    Write-Output ("SHA256={0}" -f $hash)
    Write-Output ("module=0x{0:X} sizeOfImage=0x{1:X}" -f $base, $sizeOfImage)

    foreach ($name in $ClassName) {
        $hs = [IntPtr]::Zero
        $factory = [IntPtr]::Zero
        $instance = [IntPtr]::Zero

        try {
            $hr = [DolbyActivation.Native]::WindowsCreateString($name, $name.Length, [ref]$hs)
            if ($hr -lt 0) { throw ("WindowsCreateString failed: 0x{0:X8}" -f ([uint32]$hr)) }

            $hrFactory = $getFactory.Invoke($hs, [ref]$factory)
            if ($hrFactory -lt 0 -or $factory -eq [IntPtr]::Zero) {
                Write-Output ("class={0} factory_hr=0x{1:X8} factory=0x{2:X}" -f $name, ([uint32]$hrFactory), [uint64]$factory.ToInt64())
                continue
            }

            $factoryVtable = [Runtime.InteropServices.Marshal]::ReadIntPtr($factory)
            $activatePtr = [Runtime.InteropServices.Marshal]::ReadIntPtr($factoryVtable, 6 * [IntPtr]::Size)
            $activate = [Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(
                $activatePtr,
                [type][DolbyActivation.ActivateInstanceDelegate])

            $hrActivate = $activate.Invoke($factory, [ref]$instance)
            Write-Output ("class={0} factory_hr=0x{1:X8} activate_hr=0x{2:X8} instance=0x{3:X}" -f $name, ([uint32]$hrFactory), ([uint32]$hrActivate), [uint64]$instance.ToInt64())

            if ($hrActivate -lt 0 -or $instance -eq [IntPtr]::Zero) { continue }

            foreach ($iidText in $InterfaceId) {
                $iid = [Guid]$iidText
                $qi = [IntPtr]::Zero
                try {
                    $hrQi = [Runtime.InteropServices.Marshal]::QueryInterface($instance, [ref]$iid, [ref]$qi)
                    if ($hrQi -lt 0 -or $qi -eq [IntPtr]::Zero) {
                        Write-Output ("  QI {0} hr=0x{1:X8} ptr=0x0" -f $iid, ([uint64]([int64]$hrQi -band 0xFFFFFFFFL)))
                        continue
                    }
                    $qiu = [uint64]$qi.ToInt64()
                    $instu = [uint64]$instance.ToInt64()
                    $delta = [int64]($qiu - $instu)
                    $qivt = [Runtime.InteropServices.Marshal]::ReadIntPtr($qi)
                    $qivtu = [uint64]$qivt.ToInt64()
                    $qirva = if ($qivtu -ge $base -and $qivtu -lt $end) { $qivtu - $base } else { [uint64]0 }
                    Write-Output ("  QI {0} hr=0x{1:X8} ptr=0x{2:X} delta={3} vtable=0x{4:X} rva=0x{5:X}" -f $iid, ([uint64]([int64]$hrQi -band 0xFFFFFFFFL)), $qiu, $delta, $qivtu, $qirva)
                }
                finally {
                    if ($qi -ne [IntPtr]::Zero) { [void][Runtime.InteropServices.Marshal]::Release($qi) }
                }
            }

            for ($off = 0; $off -lt $ObjectScanBytes; $off += [IntPtr]::Size) {
                $candidate = [Runtime.InteropServices.Marshal]::ReadIntPtr($instance, $off)
                $u = [BitConverter]::ToUInt64([BitConverter]::GetBytes($candidate.ToInt64()), 0)
                if ($u -lt $base -or $u -ge $end) { continue }

                $rva = $u - $base
                Write-Output ("  subobject +0x{0:X3} vtable=0x{1:X} rva=0x{2:X}" -f $off, $u, $rva)

                for ($slot = 0; $slot -lt $VtableSlots; $slot++) {
                    $fp = [Runtime.InteropServices.Marshal]::ReadIntPtr($candidate, $slot * [IntPtr]::Size)
                    $fu = [BitConverter]::ToUInt64([BitConverter]::GetBytes($fp.ToInt64()), 0)
                    if ($fu -ge $base -and $fu -lt $end) {
                        Write-Output ("    slot[{0,2}] 0x{1:X} rva=0x{2:X}" -f $slot, $fu, ($fu - $base))
                    }
                }
            }
        }
        finally {
            if ($instance -ne [IntPtr]::Zero) { [void][Runtime.InteropServices.Marshal]::Release($instance) }
            if ($factory -ne [IntPtr]::Zero) { [void][Runtime.InteropServices.Marshal]::Release($factory) }
            if ($hs -ne [IntPtr]::Zero) { [void][DolbyActivation.Native]::WindowsDeleteString($hs) }
        }
    }
}
finally {
    [void][DolbyActivation.Native]::FreeLibrary($module)
}



