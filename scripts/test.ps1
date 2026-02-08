# VDE Test Script
# Runs unit tests with GoogleTest
# Usage: .\scripts\test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Filter <pattern>] [-Build] [-Verbose]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [string]$Filter = "*",  # GoogleTest filter pattern
    
    [switch]$Build = $false,  # Build before testing
    
    [switch]$Verbose = $false  # Verbose test output
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-Info "=========================================="
Write-Info "VDE Test Script"
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
Write-Info "Filter: $Filter"

# Build if requested
if ($Build) {
    Write-Info "Building before running tests..."
    & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Build failed! Cannot run tests."
        exit 1
    }
}

# Locate test executable
$testExe = $null
if ($Generator -eq "Ninja") {
    $testExe = Join-Path $buildDir "tests\vde_tests.exe"
} else {
    $testExe = Join-Path $buildDir "tests\$Config\vde_tests.exe"
}

if (-not (Test-Path $testExe)) {
    Write-Err "Test executable not found: $testExe"
    Write-Err "Run with -Build flag to build first, or run .\scripts\build.ps1"
    exit 1
}

Write-Info "Test executable: $testExe"
Write-Info ""
Write-Info "Running tests..."
Write-Info "=========================================="

# Build test arguments
$testArgs = @()

if ($Filter -ne "*") {
    $testArgs += "--gtest_filter=$Filter"
}

if ($Verbose) {
    $testArgs += "--gtest_print_time=1"
}

# Run tests
& $testExe $testArgs

if ($LASTEXITCODE -ne 0) {
    Write-Err ""
    Write-Err "=========================================="
    Write-Err "Tests FAILED with exit code $LASTEXITCODE"
    Write-Err "=========================================="
    exit 1
}

Write-Success ""
Write-Success "=========================================="
Write-Success "All tests PASSED!"
Write-Success "=========================================="
