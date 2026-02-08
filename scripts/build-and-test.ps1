# VDE Build and Test Script (Legacy - use build.ps1 and test.ps1 instead)
# Usage: .\scripts\build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest] [-Generator MSBuild|Ninja]

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [switch]$NoBuild = $false,
    
    [switch]$NoTest = $false,
    
    [string]$Filter = "*",
    
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-Warn "=========================================="
Write-Warn "This script is LEGACY - Please use:"
Write-Warn "  .\scripts\build.ps1   - for building"
Write-Warn "  .\scripts\test.ps1    - for testing"
Write-Warn "  .\scripts\rebuild.ps1 - for rebuilding"
Write-Warn "=========================================="
Write-Info ""
Write-Info "VDE Build and Test Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Build if needed
if (-not $NoBuild) {
    & "$scriptDir\build.ps1" -Generator $Generator -Config $BuildConfig
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Build failed!"
        exit 1
    }
}

# Run tests
if (-not $NoTest) {
    & "$scriptDir\test.ps1" -Generator $Generator -Config $BuildConfig -Filter $Filter
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Tests failed!"
        exit 1
    }
}

Write-Success "=========================================="
Write-Success "Build and test completed successfully!"
Write-Success "=========================================="
