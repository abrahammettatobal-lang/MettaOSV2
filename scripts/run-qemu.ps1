param(
    [string]$Iso,
    [string]$DataDisk,
    [string]$LinuxIso,
    [string[]]$AppImage,
    [switch]$Prepare,
    [switch]$NativeOnly,
    [switch]$Compat
)
$ErrorActionPreference = "Stop"
$launchArgs = @((Join-Path $PSScriptRoot "run-mde.py"))
if ($Iso)      { $launchArgs += @("--iso", $Iso) }
if ($DataDisk) { $launchArgs += @("--disk", $DataDisk) }
if ($LinuxIso) { $launchArgs += @("--linux-iso", $LinuxIso) }
if ($Prepare)  { $launchArgs += "--prepare" }
if ($NativeOnly) { $launchArgs += "--native-only" }
if ($Compat) { $launchArgs += "--compat" }
foreach ($file in $AppImage) { $launchArgs += @("--appimage", $file) }
& python @launchArgs
exit $LASTEXITCODE
