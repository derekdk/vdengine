# VDE Verify Script
# Orchestrates build -> unit tests -> smoke tests -> render verify -> lint in sequence.
# Captures all output to logs/verify-latest.log (workspace-relative).
#
# AI AGENTS: Run this script for end-to-end verification.
# Read logs/verify-latest.log with read_file for full results.
# No need to redirect to temp files or handle terminal truncation.
#
# Usage:
#   .\scripts\verify.ps1                                           # Full verification with targeted lint
#   .\scripts\verify.ps1 -FullLint                                # Full verification with full-repo lint
#   .\scripts\verify.ps1 -SkipBuild                               # Tests + smoke only
#   .\scripts\verify.ps1 -SkipSmoke                               # Build + unit tests + lint only
#   .\scripts\verify.ps1 -SmokeExtended                           # Include priority 2 examples in smoke tests
#   .\scripts\verify.ps1 -Filter "Suite.*"                        # Targeted unit tests
#   .\scripts\verify.ps1 -SmokeFilter "*emoji*"                   # Targeted smoke test
#   .\scripts\verify.ps1 -SkipBuild -SkipSmoke -Filter "Suite.*"  # Fast inner loop with targeted lint
#   .\scripts\verify.ps1 -SkipRenderVerify                        # Skip render verification
#   .\scripts\verify.ps1 -SkipLint                                # Skip lint stage
#
# Key output files (always overwritten):
#   logs/verify-latest.log    -- full output of the latest run (read with read_file)
#   logs/verify-TIMESTAMP.log -- archived copy of each run

param(
    [switch]$SkipBuild,

    [switch]$SkipSmoke,

    [switch]$SkipRenderVerify,

    [switch]$SkipLint,

    [switch]$FullLint,

    # GoogleTest filter pattern (passed to test.ps1 -Filter)
    [string]$Filter = "",

    # Executable wildcard filter (passed to smoke-test.ps1 -Filter)
    [string]$SmokeFilter = "",

    [switch]$SmokeExtended,

    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot   = Split-Path -Parent $scriptDir
$logsDir    = Join-Path $repoRoot "logs"
$timestamp  = Get-Date -Format "yyyyMMdd-HHmmss"

if (-not (Test-Path $logsDir)) {
    New-Item -ItemType Directory -Path $logsDir | Out-Null
}

$latestLog  = Join-Path $logsDir "verify-latest.log"
$archiveLog = Join-Path $logsDir "verify-$timestamp.log"
$bar        = "=" * 44

# Initialize both log files fresh
Set-Content  -Path $latestLog  -Value "VDE Verify Run: $timestamp" -Encoding UTF8
Set-Content  -Path $archiveLog -Value "VDE Verify Run: $timestamp" -Encoding UTF8

# --- Log helpers -----------------------------------------------------------

function Write-Log {
    param(
        [string]$Text,
        [string]$Color = "White"
    )
    Write-Host $Text -ForegroundColor $Color
    # Strip ANSI escape codes so the log file is plain text
    $clean = $Text -replace '\x1b\[[0-9;]*[A-Za-z]', ''
    Add-Content -Path $latestLog  -Value $clean
    Add-Content -Path $archiveLog -Value $clean
}

function Write-LogDivider { param([string]$Color = "Cyan") Write-Log $bar $Color }

# --- Stage runner -----------------------------------------------------------
# Runs each stage as a child pwsh.exe process so that exit calls in the
# child scripts (build.ps1, test.ps1, smoke-test.ps1) terminate only
# the child process; verify.ps1 continues and captures the exit code.
# NOTE: Stage output is buffered until the stage finishes.  For real-time
# output, run individual scripts directly (build.ps1, test.ps1, etc.).

function Invoke-Stage {
    param(
        [string]$Label,
        [string]$ScriptName,
        [string[]]$ExtraArgs
    )

    Write-Log ""
    Write-LogDivider "Cyan"
    Write-Log "  Stage: $Label" "Cyan"
    Write-LogDivider "Cyan"
    Write-Host "  Running $Label... (output appears when stage completes)" -ForegroundColor DarkCyan

    $scriptPath = Join-Path $scriptDir $ScriptName
    $psArgs = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-NonInteractive",
        "-File", $scriptPath
    ) + $ExtraArgs

    # Run as external process; $LASTEXITCODE captures its exit code
    $stageOutput = & pwsh @psArgs 2>&1
    $stageExit   = $LASTEXITCODE
    $stagePass   = ($stageExit -eq 0)

    # Write captured output to console and log
    foreach ($line in $stageOutput) {
        $lineStr  = [string]$line
        $cleanStr = $lineStr -replace '\x1b\[[0-9;]*[A-Za-z]', ''
        Write-Host $lineStr
        Add-Content -Path $latestLog  -Value $cleanStr
        Add-Content -Path $archiveLog -Value $cleanStr
    }

    Write-Log ""
    if ($stagePass) {
        Write-Log "  STAGE RESULT: $Label PASSED" "Green"
    } else {
        Write-Log "  STAGE RESULT: $Label FAILED (exit $stageExit)" "Red"
    }
    Write-LogDivider

    return $stagePass
}

# --- Stages -----------------------------------------------------------------

$stageResults = [ordered]@{}
$overallPass  = $true

Write-Log ""
Write-LogDivider "Cyan"
Write-Log "  VDE Verification" "Cyan"
Write-Log "  Generator : $Generator   Config : $Config" "Cyan"
Write-LogDivider "Cyan"

# Stage 1: Build
if (-not $SkipBuild) {
    $buildArgs = @("-Generator", $Generator, "-Config", $Config)
    $buildPass = Invoke-Stage "BUILD" "build.ps1" $buildArgs
    $stageResults["BUILD"] = $buildPass

    if (-not $buildPass) {
        $overallPass = $false
        Write-Log ""
        Write-LogDivider "Red"
        Write-Log "  OVERALL: FAILED (build failed; remaining stages skipped)" "Red"
        Write-LogDivider "Red"
        Write-Log ""
        Write-Log "Log:     $latestLog"
        Write-Log "Archive: $archiveLog"
        exit 1
    }
}

# Stage 2: Unit Tests
$testArgs = @("-Generator", $Generator, "-Config", $Config, "-ProblemsOnly")
if ($Filter) { $testArgs += "-Filter", $Filter }
$testPass = Invoke-Stage "UNIT TESTS" "test.ps1" $testArgs
$stageResults["UNIT TESTS"] = $testPass
if (-not $testPass) { $overallPass = $false }

# Stage 3: Smoke Tests
if (-not $SkipSmoke) {
    $smokeArgs = @("-Generator", $Generator, "-Config", $Config, "-ProblemsOnly")
    if ($SmokeFilter) { $smokeArgs += "-Filter", $SmokeFilter }
    if ($SmokeExtended) { $smokeArgs += "-Extended" }
    $smokePass = Invoke-Stage "SMOKE TESTS" "smoke-test.ps1" $smokeArgs
    $stageResults["SMOKE TESTS"] = $smokePass
    if (-not $smokePass) { $overallPass = $false }
}

# Stage 4: Render Verification
if (-not $SkipRenderVerify) {
    $renderArgs = @("-Generator", $Generator, "-Config", $Config, "-ProblemsOnly")
    if ($SmokeFilter) { $renderArgs += "-Filter", $SmokeFilter }
    if ($SmokeExtended) { $renderArgs += "-Extended" }
    $renderPass = Invoke-Stage "RENDER VERIFY" "render-verify.ps1" $renderArgs
    $stageResults["RENDER VERIFY"] = $renderPass
    if (-not $renderPass) { $overallPass = $false }
}

# Stage 5: Lint
if (-not $SkipLint) {
    $lintArgs = @()
    if (-not $FullLint) {
        $lintArgs += "-ChangedOnly"
    }

    $lintPass = Invoke-Stage "LINT" "lint.ps1" $lintArgs
    $stageResults["LINT"] = $lintPass
    if (-not $lintPass) { $overallPass = $false }
}

# --- Summary ----------------------------------------------------------------

Write-Log ""
Write-LogDivider "Cyan"
Write-Log "  VERIFICATION SUMMARY" "Cyan"
Write-LogDivider "Cyan"

foreach ($stageName in $stageResults.Keys) {
    if ($stageResults[$stageName]) {
        Write-Log "  $stageName : PASSED" "Green"
    } else {
        Write-Log "  $stageName : FAILED" "Red"
    }
}

Write-Log ""
if ($overallPass) {
    Write-Log "  OVERALL: ALL STAGES PASSED" "Green"
} else {
    Write-Log "  OVERALL: VERIFICATION FAILED" "Red"
}
Write-LogDivider

Write-Log ""
Write-Log "Log:     $latestLog"
Write-Log "Archive: $archiveLog"

if ($overallPass) { exit 0 } else { exit 1 }
