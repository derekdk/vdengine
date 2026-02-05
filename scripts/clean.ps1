# VDE Clean Script
# Cleans build artifacts
# Usage: .\scripts\clean.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Full]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "MSBuild",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [switch]$Full = $false  # If set, removes entire build directory
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }

Write-Info "=========================================="
Write-Info "VDE Clean Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir

# Select build directory based on generator
if ($Generator -eq "Ninja") {
    $buildDir = Join-Path $vdeRoot "build_ninja"
} else {
    $buildDir = Join-Path $vdeRoot "build"
}

Write-Info "VDE Root: $vdeRoot"
Write-Info "Generator: $Generator"
Write-Info "Build Directory: $buildDir"
Write-Info "Configuration: $Config"
Write-Info "Full Clean: $Full"

if (-not (Test-Path $buildDir)) {
    Write-Warn "Build directory does not exist: $buildDir"
    Write-Info "Nothing to clean"
    exit 0
}

if ($Full) {
    Write-Warn "Performing FULL CLEAN - removing entire build directory..."
    Write-Warn "This will require a full reconfigure on next build"
    
    Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Success "Full clean complete - build directory removed"
} else {
    Push-Location $buildDir
    try {
        if (-not (Test-Path "CMakeCache.txt")) {
            Write-Warn "No CMakeCache.txt found - build directory may not be configured"
            exit 0
        }
        
        Write-Info "Cleaning build artifacts..."
        
        if ($Generator -eq "Ninja") {
            # For Ninja, use ninja clean
            if (Get-Command ninja -ErrorAction SilentlyContinue) {
                ninja -t clean
            } else {
                # Fallback to cmake --build --target clean
                cmake --build . --target clean
            }
        } else {
            # For MSBuild, use cmake --build --target clean with config
            cmake --build . --config $Config --target clean
        }
        
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Clean command returned non-zero exit code"
        } else {
            Write-Success "Clean complete"
        }
    }
    finally {
        Pop-Location
    }
}

Write-Success "=========================================="
Write-Success "Clean completed!"
Write-Success "=========================================="
