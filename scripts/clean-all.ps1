# VDE Clean All Script
# Cleans both Ninja and MSBuild build directories
# Usage: .\scripts\clean-all.ps1 [-Full]

param(
    [switch]$Full = $false  # If set, removes entire build directories
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }

Write-Info "=========================================="
Write-Info "VDE Clean All Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir

$buildDirs = @(
    @{ Name = "Ninja"; Path = Join-Path $vdeRoot "build_ninja" },
    @{ Name = "MSBuild"; Path = Join-Path $vdeRoot "build" }
)

Write-Info "VDE Root: $vdeRoot"
Write-Info "Full Clean: $Full"
Write-Info ""

$cleanedCount = 0

foreach ($buildInfo in $buildDirs) {
    $buildDir = $buildInfo.Path
    $generatorName = $buildInfo.Name
    
    if (-not (Test-Path $buildDir)) {
        Write-Warn "Build directory does not exist: $buildDir"
        Write-Info "Skipping $generatorName build"
        Write-Info ""
        continue
    }
    
    Write-Info "Cleaning $generatorName build directory: $buildDir"
    
    if ($Full) {
        Write-Warn "Performing FULL CLEAN - removing entire build directory..."
        Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Success "${generatorName}: Full clean complete - build directory removed"
        $cleanedCount++
    } else {
        Push-Location $buildDir
        try {
            if (-not (Test-Path "CMakeCache.txt")) {
                Write-Warn "No CMakeCache.txt found - build directory may not be configured"
                Write-Info "Skipping $generatorName build"
            } else {
                Write-Info "Cleaning $generatorName build artifacts..."
                
                if ($generatorName -eq "Ninja") {
                    # For Ninja, use ninja clean if available
                    if (Get-Command ninja -ErrorAction SilentlyContinue) {
                        ninja -t clean
                    } else {
                        cmake --build . --target clean
                    }
                } else {
                    # For MSBuild, clean all configurations
                    cmake --build . --config Debug --target clean
                    cmake --build . --config Release --target clean
                }
                
                if ($LASTEXITCODE -ne 0) {
                    Write-Warn "${generatorName}: Clean command returned non-zero exit code"
                } else {
                    Write-Success "${generatorName}: Clean complete"
                    $cleanedCount++
                }
            }
        }
        finally {
            Pop-Location
        }
    }
    
    Write-Info ""
}

Write-Success "=========================================="
if ($cleanedCount -gt 0) {
    Write-Success "Clean All completed! ($cleanedCount build(s) cleaned)"
} else {
    Write-Warn "No builds were cleaned"
}
Write-Success "=========================================="
