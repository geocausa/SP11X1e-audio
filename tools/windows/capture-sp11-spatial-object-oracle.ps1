param(
    [string]$Probe = (Join-Path $PSScriptRoot 'sp11-spatial-object-oracle-arm64.exe'),
    [string]$OutputRoot = (Join-Path $env:USERPROFILE 'Documents\SP11-Spatial-Object-Oracle')
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Probe)) {
    throw "Probe not found: $Probe"
}

$procDump = (Get-Command procdump64a.exe -ErrorAction SilentlyContinue).Source
if (-not $procDump) {
    $wingetRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    $procDump = Get-ChildItem -LiteralPath $wingetRoot -Filter procdump64a.exe -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $procDump) {
    throw 'ARM64 ProcDump (procdump64a.exe) was not found.'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outDir = Join-Path $OutputRoot $stamp
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$stderrLog = Join-Path $outDir 'spatial-object-probe.stderr.txt'
$stdoutLog = Join-Path $outDir 'spatial-object-probe.stdout.txt'

Write-Host "Probe:    $Probe"
Write-Host "ProcDump: $procDump"
Write-Host "Output:   $outDir"

$probeProc = Start-Process -FilePath $Probe -PassThru -WindowStyle Hidden `
    -RedirectStandardError $stderrLog -RedirectStandardOutput $stdoutLog

$ready = $false
$deadline = (Get-Date).AddSeconds(15)
while ((Get-Date) -lt $deadline -and -not $probeProc.HasExited) {
    Start-Sleep -Milliseconds 100
    if (Test-Path -LiteralPath $stderrLog) {
        $text = Get-Content -LiteralPath $stderrLog -Raw -ErrorAction SilentlyContinue
        if ($text -match 'READY_FOR_AUDIODG_FULL_DUMP') {
            $ready = $true
            break
        }
    }
}

if (-not $ready) {
    if (-not $probeProc.HasExited) {
        Write-Warning 'Probe never reached READY; waiting for its safe volume-restoring exit.'
        $probeProc.WaitForExit()
    }
    $text = if (Test-Path $stderrLog) { Get-Content $stderrLog -Raw } else { '' }
    throw "Spatial probe did not become ready. Exit=$($probeProc.ExitCode)`n$text"
}

Write-Host 'Spatial object is active; capturing full audiodg dumps...'
$audiodg = @(Get-Process audiodg -ErrorAction Stop)
if ($audiodg.Count -eq 0) {
    throw 'No audiodg.exe process found while spatial object was active.'
}

$dumpPaths = @()
foreach ($p in $audiodg) {
    $dumpPath = Join-Path $outDir ("audiodg-spatial-object-pid{0}.dmp" -f $p.Id)
    & $procDump -accepteula -ma $p.Id $dumpPath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $dumpPath)) {
        throw "ProcDump failed for audiodg PID $($p.Id), exit=$LASTEXITCODE"
    }
    $dumpPaths += $dumpPath
}

# Do not terminate the probe: normal exit restores the user's pre-test endpoint volume.
$probeProc.WaitForExit()
$probeText = Get-Content -LiteralPath $stderrLog -Raw
if ($probeProc.ExitCode -ne 0 -or $probeText -notmatch 'SPATIAL_ORACLE_RESULT PASS') {
    throw "Probe failed after capture. Exit=$($probeProc.ExitCode)`n$probeText"
}

Write-Host "Probe PASS; previous endpoint volume restored."
Get-FileHash -Algorithm SHA256 -LiteralPath $Probe, $stderrLog, $dumpPaths |
    Format-Table -AutoSize

$manifest = [ordered]@{
    CaptureTime = (Get-Date).ToString('o')
    ProbePath = (Resolve-Path -LiteralPath $Probe).Path
    ProbeSHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Probe).Hash
    ProbeExitCode = $probeProc.ExitCode
    AudiodgDumps = @($dumpPaths | ForEach-Object {
        [ordered]@{
            Path = (Resolve-Path -LiteralPath $_).Path
            SHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash
            Bytes = (Get-Item -LiteralPath $_).Length
        }
    })
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $outDir 'manifest.json')
Write-Host "CAPTURE_RESULT PASS $outDir"
