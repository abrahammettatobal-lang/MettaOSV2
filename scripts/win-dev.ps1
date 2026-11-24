# MettaOS — puente Windows → WSL para desarrollo (SA-02)
# Uso: .\scripts\win-dev.ps1 [-VerifyOnly] [-WslCommand "make"]
param(
    [switch]$VerifyOnly,
    [switch]$RunSetup,
    [string]$WslCommand = "make",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

function Write-MettaLog {
    param([string]$Message)
    Write-Host "[win-dev] $Message"
}

function Test-WslAvailable {
    try {
        $null = & wsl.exe --status 2>$null
        return $true
    } catch {
        return $false
    }
}

function Get-DefaultRepoRoot {
    if ($RepoRoot -ne "") { return (Resolve-Path -LiteralPath $RepoRoot).Path }
    $here = $PSScriptRoot
    if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Path }
    return (Resolve-Path (Join-Path $here "..")).Path
}

function Convert-WindowsPathToWsl {
    param([string]$WinPath)
    $resolved = (Resolve-Path -LiteralPath $WinPath).Path
    if ($resolved -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLower()
        $rest = $Matches[2] -replace '\\', '/'
        return "/mnt/$drive/$rest"
    }
    throw "Ruta Windows no válida: $WinPath"
}

Write-MettaLog "MettaOS win-dev (SA-02)"

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "wsl.exe no está en PATH. Instale WSL2: wsl --install"
}

$wslVersion = & wsl.exe --version 2>&1 | Out-String
Write-MettaLog "wsl --version:`n$wslVersion"

$repo = Get-DefaultRepoRoot
Write-MettaLog "Raíz del repo (Windows): $repo"

if (-not (Test-Path -LiteralPath $repo)) {
    throw "No existe la ruta del repositorio: $repo"
}

$wslRepo = Convert-WindowsPathToWsl -WinPath $repo
Write-MettaLog "Raíz del repo (WSL): $wslRepo"

$probe = & wsl.exe bash -lc "test -d '$wslRepo' && echo OK || echo MISSING"
if ($probe -notmatch 'OK') {
    throw "WSL no ve el directorio: $wslRepo (¿montaje /mnt/c activo?)"
}
Write-MettaLog "Montaje /mnt verificado."

$setupScript = Join-Path $repo "scripts\wsl-setup.sh"
$wslSetup = Convert-WindowsPathToWsl -WinPath $setupScript
if (-not (Test-Path -LiteralPath $setupScript)) {
    Write-MettaLog "ADVERTENCIA: no se encontró $setupScript"
} else {
    Write-MettaLog "wsl-setup.sh: $wslSetup"
}

if ($RunSetup) {
    if (-not (Test-Path -LiteralPath $setupScript)) {
        throw "No se encontró scripts/wsl-setup.sh"
    }
    Write-MettaLog "Instalando dependencias como root WSL: $wslSetup"
    & wsl.exe -u root bash $wslSetup
    exit $LASTEXITCODE
}

if ($VerifyOnly) {
    Write-MettaLog "Modo -VerifyOnly: no se ejecuta make ni setup."
    exit 0
}

Write-MettaLog "Ejecutando en WSL: cd '$wslRepo' && $WslCommand"
& wsl.exe bash -lc "cd '$wslRepo' && $WslCommand"
$exit = $LASTEXITCODE
if ($exit -ne 0) {
    Write-MettaLog "Comando terminó con código $exit (esperable si Makefile aún no existe — SA-03)."
}
exit $exit