# VDE Build and Test Script (Legacy - use build.ps1 and test.ps1 instead)
# Usage: .\scripts\build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest] [-Filter <pattern>] [-Generator MSBuild|Ninja] [-Verbose] [-ProblemsOnly]

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [switch]$NoBuild = $false,
    
    [switch]$NoTest = $false,
    
    [string]$Filter = "*",
    
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [switch]$Verbose = $false,

    [switch]$ProblemsOnly = $false
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b)'
$warningPattern = '(?i)(^\s*warning\b|\bwarning:|\bwarn\b|\blegacy\b)'
$problemPattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b|^\s*warning\b|\bwarning:|\bwarn\b|\blegacy\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

$ProblemsOnly = Resolve-ProblemsOnlyPreference -BoundParameters $PSBoundParameters -VerboseRequested $Verbose
$ShowWarningsInProblemsOnly = Resolve-ProblemsOnlyWarningPreference -BoundParameters $PSBoundParameters

if (-not $ProblemsOnly) {
    Write-Warn "=========================================="
    Write-Warn "This script is LEGACY - Please use:"
    Write-Warn "  .\scripts\build.ps1   - for building"
    Write-Warn "  .\scripts\test.ps1    - for testing"
    Write-Warn "  .\scripts\rebuild.ps1 - for rebuilding"
    Write-Warn "=========================================="
    Write-Info ""
}

Write-Info "VDE Build and Test Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ranSomething = $false

# Build if needed
if (-not $NoBuild) {
    $ranSomething = $true
    $buildExitCode = Invoke-ScriptWithMode -ScriptPath "$scriptDir\build.ps1" -Arguments @('-Generator', $Generator, '-Config', $BuildConfig) -VerboseOutput:$Verbose
    if ($buildExitCode -ne 0) {
        Write-Err "FAILURE: Build-and-test failed during build."
        exit $buildExitCode
    }
}

# Run tests
if (-not $NoTest) {
    $ranSomething = $true
    $testExitCode = Invoke-ScriptWithMode -ScriptPath "$scriptDir\test.ps1" -Arguments @('-Generator', $Generator, '-Config', $BuildConfig, '-Filter', $Filter) -VerboseOutput:$Verbose
    if ($testExitCode -ne 0) {
        Write-Err "FAILURE: Build-and-test failed during tests."
        exit $testExitCode
    }
}

if ($ProblemsOnly) {
    if ($ranSomething) {
        Write-Pass "Build and tests passed ($Generator $BuildConfig)."
    } else {
        Write-Pass "No build or test steps were requested."
    }
} else {
    Write-Success "=========================================="
    if ($ranSomething) {
        Write-Success "Build and test completed successfully!"
    } else {
        Write-Success "No build or test steps were requested."
    }
    Write-Success "=========================================="
}
