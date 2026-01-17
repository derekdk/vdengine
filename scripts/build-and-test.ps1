# VDE Build and Test Script
# Usage: .\scripts\build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest]

param(
    [string]$BuildConfig = "Debug",
    [switch]$NoBuild = $false,
    [switch]$NoTest = $false,
    [string]$Filter = "*"
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-Info "=========================================="
Write-Info "VDE Build and Test Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir
$buildDir = Join-Path $vdeRoot "build"

Write-Info "VDE Root: $vdeRoot"
Write-Info "Build Directory: $buildDir"
Write-Info "Configuration: $BuildConfig"

# Create build directory if needed
if (-not (Test-Path $buildDir)) {
    Write-Info "Creating build directory..."
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

Push-Location $buildDir
try {
    # Configure if needed
    if (-not (Test-Path "CMakeCache.txt")) {
        Write-Info "Configuring CMake..."
        cmake .. -DCMAKE_BUILD_TYPE=$BuildConfig
        if ($LASTEXITCODE -ne 0) {
            Write-Err "CMake configuration failed!"
            exit 1
        }
    }
    
    # Build
    if (-not $NoBuild) {
        Write-Info "Building VDE..."
        cmake --build . --config $BuildConfig --parallel
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed!"
            exit 1
        }
        Write-Success "Build successful!"
    }
    
    # Run tests
    if (-not $NoTest) {
        Write-Info "Running tests..."
        $testExe = Join-Path $buildDir "tests\$BuildConfig\vde_tests.exe"
        
        if (Test-Path $testExe) {
            & $testExe --gtest_filter=$Filter
            if ($LASTEXITCODE -ne 0) {
                Write-Err "Tests failed!"
                exit 1
            }
            Write-Success "All tests passed!"
        } else {
            Write-Warn "Test executable not found at: $testExe"
            Write-Warn "Skipping tests..."
        }
    }
    
    Write-Success "=========================================="
    Write-Success "Build and test completed successfully!"
    Write-Success "=========================================="
}
finally {
    Pop-Location
}
