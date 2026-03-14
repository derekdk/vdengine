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

# Colors for output
function Write-Success {
    param([string]$msg)
    if ($ProblemsOnly) {
        return
    }
    Write-Host $msg -ForegroundColor Green
}
function Write-Info {
    param([string]$msg)
    if ($ProblemsOnly) {
        return
    }
    Write-Host $msg -ForegroundColor Cyan
}
function Write-Warn {
    param([string]$msg)
    if ($ProblemsOnly -and $msg -notmatch '^(WARNING|FAILURE|PASS):') {
        $msg = "WARNING: $msg"
    }
    Write-Host $msg -ForegroundColor Yellow
}
function Write-Err {
    param([string]$msg)
    if ($ProblemsOnly -and $msg -notmatch '^(WARNING|FAILURE|PASS):') {
        $msg = "FAILURE: $msg"
    }
    Write-Host $msg -ForegroundColor Red
}
function Write-Pass { param([string]$msg) Write-Host "PASS: $msg" -ForegroundColor Green }

$failurePattern = '(?i)(assert failed|test failed|\[\s*failed\s*\]|unknown file: Failure|\bFailure\b|\berror\b|\bfatal\b|\bexception\b)'
$warningPattern = '(?i)\b(warn|warning|validation)\b'
$problemPattern = '(?i)(assert failed|test failed|\[\s*failed\s*\]|unknown file: Failure|\bFailure\b|\berror\b|\bfatal\b|\bexception\b|\bwarn\b|\bwarning\b|\bvalidation\b)'

function Get-ProblemLines {
    param(
        [string[]]$Lines,
        [int]$MaxLines = 40
    )

    if (-not $Lines) {
        return @()
    }

    $selected = New-Object System.Collections.Generic.List[string]
    $captureContext = 0

    foreach ($line in $Lines) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            $captureContext = 0
            continue
        }

        if ($line -match $problemPattern) {
            if (-not $selected.Contains($line)) {
                $selected.Add($line)
            }
            $captureContext = 2
            if ($selected.Count -ge $MaxLines) {
                break
            }
            continue
        }

        if ($captureContext -gt 0) {
            if (-not $selected.Contains($line)) {
                $selected.Add($line)
            }
            $captureContext--
            if ($selected.Count -ge $MaxLines) {
                break
            }
        }
    }

    return @($selected | Select-Object -First $MaxLines)
}

function Test-IsWarningLine {
    param([string]$Line)

    return ($Line -match $warningPattern) -and ($Line -notmatch $failurePattern)
}

function Write-ProblemLines {
    param([string[]]$Lines)

    foreach ($line in $Lines) {
        if (Test-IsWarningLine -Line $line) {
            Write-Warn "WARNING: $line"
        } else {
            Write-Err "FAILURE: $line"
        }
    }
}

function Invoke-BuildForProblemsOnly {
    param(
        [string]$BuildScriptPath,
        [string]$SelectedGenerator,
        [string]$SelectedConfig
    )

    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()

    try {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $BuildScriptPath -Generator $SelectedGenerator -Config $SelectedConfig > $stdout 2> $stderr
        $buildExitCode = $LASTEXITCODE

        $buildLines = @()
        if (Test-Path $stdout) {
            $buildLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            $buildLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue
        }

        $problemLines = Get-ProblemLines -Lines $buildLines
        if ($problemLines.Count -eq 0 -and $buildExitCode -ne 0) {
            $problemLines = @($buildLines | Select-Object -Last 40)
        }

        Write-ProblemLines -Lines $problemLines

        if ($buildExitCode -ne 0) {
            Write-Err "FAILURE: Build failed with exit code $buildExitCode"
            exit 1
        }
    }
    finally {
        if (Test-Path $stdout) {
            Remove-Item $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            Remove-Item $stderr -ErrorAction SilentlyContinue
        }
    }
}

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
    if ($ProblemsOnly) {
        Invoke-BuildForProblemsOnly -BuildScriptPath "$scriptDir\build.ps1" -SelectedGenerator $Generator -SelectedConfig $Config
    } else {
        Write-Info "Building before running tests..."
        & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed! Cannot run tests."
            exit 1
        }
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
        & $testExe $testArgs > $stdout 2> $stderr
        $testExitCode = $LASTEXITCODE

        $allLines = @()
        if (Test-Path $stdout) {
            $allLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            $allLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue
        }

        $problemLines = Get-ProblemLines -Lines $allLines
        if ($problemLines.Count -eq 0 -and $testExitCode -ne 0) {
            $problemLines = @($allLines | Select-Object -Last 40)
        }

        Write-ProblemLines -Lines $problemLines

        if ($testExitCode -ne 0) {
            Write-Err "FAILURE: Tests FAILED with exit code $testExitCode"
            exit 1
        }
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
