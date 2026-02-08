# VDE Rebuild Script
# Performs a full rebuild (clean then build)
# Usage: .\scripts\rebuild.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }

Write-Info "=========================================="
Write-Info "VDE Rebuild Script (Clean + Build)"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# First clean
& "$scriptDir\clean.ps1" -Generator $Generator -Config $Config
if ($LASTEXITCODE -ne 0) {
    exit 1
}

# Then build
& "$scriptDir\build.ps1" -Generator $Generator -Config $Config
if ($LASTEXITCODE -ne 0) {
    exit 1
}
