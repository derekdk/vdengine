# Resource Editor Smoke Test
#
# Runs vde_resource_editor in script mode with smoke_test.txt, which exercises
# all 30 commands in sequence. Verifies clean exit (exit code 0).
#
# Usage:
#   .\smoketests\resource_editor_smoke.ps1
#   .\smoketests\resource_editor_smoke.ps1 -Generator MSBuild -Config Release
#
# This script can also be run from the CI environment as a standalone test.

param(
    [ValidateSet("Ninja", "MSBuild")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info    { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn    { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err     { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-Info "=========================================="
Write-Info "VDE Resource Editor - Command Script Smoke Test"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot   = Split-Path -Parent $scriptDir

# Resolve paths
if ($Generator -eq "Ninja") {
    $exePath = Join-Path $vdeRoot "build_ninja\tools\resource_editor\vde_resource_editor.exe"
} else {
    $exePath = Join-Path $vdeRoot "build\tools\resource_editor\$Config\vde_resource_editor.exe"
}

$smokeScript = Join-Path $vdeRoot "tools\resource_editor\scripts\smoke_test.txt"

Write-Info "Executable  : $exePath"
Write-Info "Script      : $smokeScript"
Write-Info ""

# Validate prerequisites
if (-not (Test-Path $exePath)) {
    Write-Err "Executable not found: $exePath"
    Write-Err "Build the project first: .\scripts\build.ps1"
    exit 1
}

if (-not (Test-Path $smokeScript)) {
    Write-Err "Smoke script not found: $smokeScript"
    exit 1
}

$exitCode = 0

# Run from the exe's own directory so relative shader paths (shaders/mesh.vert etc.)
# resolve correctly — mirrors what runTool/setWorkingDirectoryToExecutablePath() does.
$exeDir = Split-Path -Parent $exePath

try {
    Push-Location $exeDir

    if ($Verbose) {
        & $exePath $smokeScript --log-console
    } else {
        & $exePath $smokeScript 2>&1 | Out-Null
    }

    $exitCode = $LASTEXITCODE
} finally {
    Pop-Location

    # Clean up working files produced by save/export commands in the exe dir
    Remove-Item -Path (Join-Path $exeDir "smoke_output.png") -Force -ErrorAction SilentlyContinue
    Remove-Item -Path (Join-Path $exeDir "smoke_output_export.png") -Force -ErrorAction SilentlyContinue
}

Write-Info "=========================================="
Write-Info "Result"
Write-Info "=========================================="

if ($exitCode -eq 0) {
    Write-Success "PASSED  (exit code: 0)"
    Write-Success "All resource editor commands executed successfully."
} else {
    Write-Err "FAILED  (exit code: $exitCode)"
    Write-Err "One or more commands failed. Re-run with -Verbose to see output."
    Write-Err ""
    Write-Err "Manual repro:"
    Write-Err "  cd $(Split-Path -Parent $exePath)"
    Write-Err "  $exePath $smokeScript"
}

exit $exitCode
