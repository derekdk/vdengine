# VDE Test Script
# Runs unit tests with GoogleTest
# Usage: .\scripts\test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Filter <pattern>] [-Build] [-Verbose] [-ProblemsOnly]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [string]$Filter = "*",  # GoogleTest filter pattern
    
    [switch]$Build = $false,  # Build before testing
    
    [switch]$Verbose = $false,  # Verbose test output

    [switch]$ProblemsOnly = $false  # Emit only warnings/failures plus a final PASS/FAIL line
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(assert failed|test failed|\[\s*failed\s*\]|unknown file:\s*Failure|^\s*error\b|\berror:|\bfatal error\b|\bfailed to\b|\bexception thrown\b)'
$warningPattern = '(?i)(^\s*warning\b|\bwarning:|\bvalidation layer\b)'
$problemPattern = '(?i)(assert failed|test failed|\[\s*failed\s*\]|unknown file:\s*Failure|^\s*error\b|\berror:|\bfatal error\b|\bfailed to\b|\bexception thrown\b|^\s*warning\b|\bwarning:|\bvalidation layer\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

$ProblemsOnly = Resolve-ProblemsOnlyPreference -BoundParameters $PSBoundParameters -VerboseRequested $Verbose
$ShowWarningsInProblemsOnly = Resolve-ProblemsOnlyWarningPreference -BoundParameters $PSBoundParameters

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
    $buildExitCode = Invoke-ScriptWithMode -ScriptPath "$scriptDir\build.ps1" -Arguments @('-Generator', $Generator, '-Config', $Config) -VerboseOutput:$Verbose
    if ($buildExitCode -ne 0) {
        Write-Err "FAILURE: Tests did not run because the build failed."
        exit $buildExitCode
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
if ($ProblemsOnly) {
    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()

    try {
        $startProcessArgs = @{
            FilePath = $testExe
            NoNewWindow = $true
            Wait = $true
            PassThru = $true
            RedirectStandardOutput = $stdout
            RedirectStandardError = $stderr
        }
        if ($testArgs.Count -gt 0) {
            $startProcessArgs.ArgumentList = $testArgs
        }

        $testProcess = Start-Process @startProcessArgs
        $testExitCode = $testProcess.ExitCode

        $allLines = @()
        if (Test-Path $stdout) {
            $allLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            $allLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue
        }

        if ($testExitCode -ne 0) {
            $problemLines = Get-ProblemLines -Lines $allLines
            if ($problemLines.Count -eq 0) {
                $problemLines = @($allLines | Select-Object -Last 40)
            }

            Write-ProblemLines -Lines $problemLines
            Write-Err "FAILURE: Tests FAILED with exit code $testExitCode"
            exit 1
        }

        Write-WarningSummary -Lines $allLines
    }
    finally {
        if (Test-Path $stdout) {
            Remove-Item $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            Remove-Item $stderr -ErrorAction SilentlyContinue
        }
    }

    Write-Pass "All tests passed."
    exit 0
}

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
