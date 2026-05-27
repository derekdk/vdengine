# VDE Rebuild Script
# Performs a full rebuild (clean then build)
# Usage: .\scripts\rebuild.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Verbose] [-ProblemsOnly]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$Verbose = $false,

    [switch]$ProblemsOnly = $false
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b)'
$warningPattern = '(?i)(^\s*warning\b|\bwarning:|\bwarn\b)'
$problemPattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b|^\s*warning\b|\bwarning:|\bwarn\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

$ProblemsOnly = Resolve-ProblemsOnlyPreference -BoundParameters $PSBoundParameters -VerboseRequested $Verbose
$ShowWarningsInProblemsOnly = Resolve-ProblemsOnlyWarningPreference -BoundParameters $PSBoundParameters

Write-Info "=========================================="
Write-Info "VDE Rebuild Script (Clean + Build)"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# First clean
$cleanExitCode = Invoke-ScriptWithMode -ScriptPath "$scriptDir\clean.ps1" -Arguments @('-Generator', $Generator, '-Config', $Config) -VerboseOutput:$Verbose
if ($cleanExitCode -ne 0) {
    Write-Err "FAILURE: Rebuild failed during clean."
    exit $cleanExitCode
}

# Then build
$buildExitCode = Invoke-ScriptWithMode -ScriptPath "$scriptDir\build.ps1" -Arguments @('-Generator', $Generator, '-Config', $Config) -VerboseOutput:$Verbose
if ($buildExitCode -ne 0) {
    Write-Err "FAILURE: Rebuild failed during build."
    exit $buildExitCode
}

if ($ProblemsOnly) {
    Write-Pass "Rebuild succeeded ($Generator $Config)."
} else {
    Write-Success "=========================================="
    Write-Success "Rebuild completed successfully!"
    Write-Success "=========================================="
}

exit 0
