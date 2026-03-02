# VDE VLauncher Run Script
# Builds vde_vlauncher if missing (or when -BuildIfMissing disabled, fails), then launches it detached.
# Usage: .\scripts\run-vlauncher.ps1 [-Generator Ninja|MSBuild] [-Config Debug|Release] [-BuildIfMissing] [-Rebuild]

param(
    [ValidateSet("Ninja", "MSBuild")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$BuildIfMissing = $true,

    [switch]$Rebuild = $false
)

$ErrorActionPreference = "Stop"

function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

function Ensure-VSDevShell {
    if ($env:VSINSTALLDIR) {
        return
    }

    $vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    if (-not $vsPath) {
        throw "Visual Studio installation not found."
    }

    Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -Arch amd64 | Out-Null
}

Write-Info "=========================================="
Write-Info "VDE VLauncher Runner"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir

if ($Generator -eq "Ninja") {
    $buildDir = Join-Path $vdeRoot "build_ninja"
    $exePath = Join-Path $buildDir "tools\vlauncher\vde_vlauncher.exe"
} else {
    $buildDir = Join-Path $vdeRoot "build"
    $exePath = Join-Path $buildDir "tools\vlauncher\$Config\vde_vlauncher.exe"
}

Write-Info "VDE Root: $vdeRoot"
Write-Info "Generator: $Generator"
Write-Info "Configuration: $Config"
Write-Info "Expected executable: $exePath"

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

$needsBuild = $Rebuild -or -not (Test-Path $exePath)

if ($needsBuild) {
    if (-not $BuildIfMissing) {
        Write-Err "vde_vlauncher executable missing and -BuildIfMissing is disabled."
        exit 1
    }

    Write-Info "Preparing build environment..."
    Ensure-VSDevShell

    Push-Location $buildDir
    try {
        $needsConfigure = $false
        if (-not (Test-Path "CMakeCache.txt")) {
            $needsConfigure = $true
        } else {
            $cacheContent = Get-Content "CMakeCache.txt" -Raw
            if ($Generator -eq "Ninja" -and $cacheContent -notmatch "CMAKE_GENERATOR:INTERNAL=Ninja") {
                $needsConfigure = $true
            }
            if ($Generator -eq "MSBuild" -and $cacheContent -match "CMAKE_GENERATOR:INTERNAL=Ninja") {
                $needsConfigure = $true
            }
        }

        if ($needsConfigure) {
            Write-Info "Configuring CMake..."
            if ($Generator -eq "Ninja") {
                cmake -S $vdeRoot -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=$Config
            } else {
                cmake -S $vdeRoot -B $buildDir
            }

            if ($LASTEXITCODE -ne 0) {
                Write-Err "CMake configure failed"
                exit 1
            }
        }

        Write-Info "Building target vde_vlauncher..."
        $buildArgs = @("--build", ".", "--target", "vde_vlauncher", "--parallel")
        if ($Generator -eq "MSBuild") {
            $buildArgs += @("--config", $Config)
        }
        if ($Rebuild) {
            $buildArgs += "--clean-first"
        }

        & cmake $buildArgs

        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed"
            exit 1
        }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $exePath)) {
    Write-Err "vde_vlauncher executable not found at: $exePath"
    exit 1
}

Write-Info "Launching VLauncher..."
Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath)

Write-Success "VLauncher launched: $exePath"
