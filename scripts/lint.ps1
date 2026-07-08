# VDE Lint Script
# Runs VDE lint stages in sequence for local development.
#
# Usage:
#   .\scripts\lint.ps1                         # Full lint across the repository
#   .\scripts\lint.ps1 -ChangedOnly            # Lint only changed files
#   .\scripts\lint.ps1 -Quick -ChangedOnly     # Fast targeted lint
#   .\scripts\lint.ps1 -Files src\BufferUtils.cpp
#   .\scripts\lint.ps1 -Fix                    # Auto-fix formatting issues
#   .\scripts\lint.ps1 -Help                   # Show this help message

[CmdletBinding()]
param(
    [switch]$Quick,

    [switch]$Fix,

    [switch]$ChangedOnly,

    [string[]]$Files = @(),

    [string]$Since = '',

    [ValidateSet('Auto', 'Ninja', 'MSBuild')]
    [string]$Generator = 'Auto',

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
VDE Lint Script - Run static analysis tools

Usage:
    .\scripts\lint.ps1                         # Run the full lint suite
    .\scripts\lint.ps1 -ChangedOnly            # Lint only changed files in the working tree
    .\scripts\lint.ps1 -Quick                  # Format check + cppcheck only
    .\scripts\lint.ps1 -Quick -ChangedOnly     # Fast targeted lint
    .\scripts\lint.ps1 -Fix                    # Run clang-format in fix mode
    .\scripts\lint.ps1 -Files src\BufferUtils.cpp
    .\scripts\lint.ps1 -Help                   # Show this help message

Options:
    -Quick        Skip shader validation and clang-tidy
    -Fix          Run clang-format in fix mode (modifies files in-place)
    -ChangedOnly  Use git working tree changes as the lint file set
    -Files        Explicit file list (relative or absolute)
    -Since        Git revision/range for changed-file linting (implies -ChangedOnly)
    -Generator    Compile database preference for clang-tidy (Auto, Ninja, MSBuild)
    -Help         Show this help

The top-level script orchestrates these stages:
    1. format.ps1            clang-format check/fix
    2. lint-shaders.ps1      GLSL shader validation
    3. lint-cppcheck.ps1     cppcheck static analysis
    4. lint-clang-tidy.ps1   clang-tidy static analysis
"@
    exit 0
}

if ($Help) {
    Show-Help
}

if (-not [string]::IsNullOrWhiteSpace($Since)) {
    $ChangedOnly = $true
}

if ($ChangedOnly -and $Files.Count -gt 0) {
    throw "Use either -ChangedOnly/-Since or -Files, not both."
}

$scriptDir = $PSScriptRoot
$projectRoot = Get-VdeLintRepoRoot -ScriptRoot $scriptDir
Set-Location $projectRoot

$scriptHost = if (Get-Command powershell -ErrorAction SilentlyContinue) { 'powershell' } else { 'pwsh' }
$bar = '=' * 50
$overallPass = $true
$results = [ordered]@{}

$allRelevantExtensions = @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx', '.vert', '.frag')
$codeExtensions = @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx')
$shaderExtensions = @('.vert', '.frag')

$selectedFiles = @()
$targetedMode = $ChangedOnly -or $Files.Count -gt 0
if ($Files.Count -gt 0) {
    $selectedFiles = Resolve-VdeFiles -RepoRoot $projectRoot -Files $Files -AllowedExtensions $allRelevantExtensions
} elseif ($ChangedOnly) {
    $selectedFiles = Get-VdeChangedFiles -RepoRoot $projectRoot -Since $Since
}

$codeFiles = @($selectedFiles | Where-Object { [System.IO.Path]::GetExtension($_) -in $codeExtensions })
$shaderFiles = @($selectedFiles | Where-Object { [System.IO.Path]::GetExtension($_) -in $shaderExtensions })

function Get-CppcheckTargetFiles {
    param([string[]]$Files)

    $cppcheckFiles = [System.Collections.Generic.List[string]]::new()
    foreach ($file in $Files) {
        $relative = Get-VdeRelativePath -RepoRoot $projectRoot -Path $file
        if ($relative -and $relative -match '^games[\\/][^\\/]+[\\/].+\.(h|hpp|hh|hxx)$') {
            $gameDir = Split-Path -Parent $file
            $siblingSources = Get-ChildItem -Path $gameDir -Filter *.cpp -File -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
            if ($siblingSources.Count -eq 0) {
                $cppcheckFiles.Add($file)
                continue
            }
            foreach ($source in $siblingSources) {
                $cppcheckFiles.Add($source)
            }
            continue
        }

        $cppcheckFiles.Add($file)
    }

    return @($cppcheckFiles | Sort-Object -Unique)
}

$cppcheckCodeFiles = @()
if ($targetedMode) {
    $cppcheckCodeFiles = Get-CppcheckTargetFiles -Files $codeFiles
}

function Write-StageHeader {
    param([string]$Name)
    Write-Host ''
    Write-Host $bar -ForegroundColor Cyan
    Write-Host "  $Name" -ForegroundColor Cyan
    Write-Host $bar -ForegroundColor Cyan
}

function Invoke-Stage {
    param(
        [string]$Label,
        [string]$ScriptName,
        [string[]]$ExtraArgs = @()
    )

    Write-StageHeader $Label
    $scriptPath = Join-Path $scriptDir $ScriptName

    # Build a scriptblock-style call so that array-valued parameters work.
    # Switches (starting with -) are emitted as-is.  Consecutive non-switch
    # values that follow a parameter name are grouped into an @(...) array
    # literal so they bind to the [string[]] parameter correctly.
    $tokens = [System.Collections.Generic.List[string]]::new()
    $escapedPath = "'" + $scriptPath.Replace("'", "''") + "'"
    $tokens.Add("& $escapedPath")

    $pendingSwitch = $null
    $pendingValues = [System.Collections.Generic.List[string]]::new()

    function Flush-Pending {
        if ($null -eq $pendingSwitch) { return }
        if ($pendingValues.Count -eq 0) {
            $tokens.Add($pendingSwitch)
        } elseif ($pendingValues.Count -eq 1) {
            $tokens.Add("$pendingSwitch '$($pendingValues[0].Replace(""'"", ""''""  ))'")
        } else {
            $quotedVals = $pendingValues | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }
            $tokens.Add("$pendingSwitch @($($quotedVals -join ', '))")
        }
        $pendingValues.Clear()
    }

    foreach ($arg in $ExtraArgs) {
        if ($arg -match '^-') {
            Flush-Pending
            $pendingSwitch = $arg
        } else {
            $pendingValues.Add($arg)
        }
    }
    Flush-Pending
    if ($null -eq $pendingSwitch -and $ExtraArgs.Count -gt 0) {
        # No switches; all args were positional (edge case).
        $tokens.AddRange([string[]]($ExtraArgs | ForEach-Object { "'" + $_.Replace("'","''") + "'" }))
    }

    $callExpr = $tokens -join ' '
    $psArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-NonInteractive', '-Command', $callExpr)

    $stageOutput = & $scriptHost @psArgs 2>&1
    $stagePass = ($LASTEXITCODE -eq 0)

    foreach ($line in $stageOutput) {
        Write-Host $line
    }

    Write-Host ''
    if ($stagePass) {
        Write-Host "  PASSED: $Label" -ForegroundColor Green
    } else {
        Write-Host "  FAILED: $Label" -ForegroundColor Red
    }
    return $stagePass
}

function Mark-Skipped {
    param(
        [string]$Name,
        [string]$Reason
    )

    Write-Host ''
    Write-Host "  SKIPPED: $Name ($Reason)" -ForegroundColor DarkGray
    $results[$Name] = 'skipped'
}

if ($targetedMode -and $selectedFiles.Count -eq 0) {
    $emptyMsg = if ($Files.Count -gt 0) {
        'None of the specified files matched a known lintable extension or path.'
    } else {
        'No matching changed files were found for linting.'
    }
    Write-Host $emptyMsg -ForegroundColor DarkGray
}

$formatTool = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $formatTool) {
    Mark-Skipped -Name 'clang-format' -Reason 'not found in PATH'
} elseif ($targetedMode -and $codeFiles.Count -eq 0) {
    Mark-Skipped -Name 'clang-format' -Reason 'no matching source files selected'
} else {
    $formatArgs = @()
    if (-not $Fix) {
        $formatArgs += '-Check'
    }
    if ($targetedMode) {
        $formatArgs += '-Files'
        $formatArgs += $codeFiles
    }

    $formatPass = Invoke-Stage -Label 'clang-format' -ScriptName 'format.ps1' -ExtraArgs $formatArgs
    $results['clang-format'] = $formatPass
    if (-not $formatPass) { $overallPass = $false }
}

if ($Quick) {
    Mark-Skipped -Name 'glslangValidator' -Reason 'skipped by -Quick'
} else {
    $shaderTool = Get-Command glslangValidator -ErrorAction SilentlyContinue
    if (-not $shaderTool) {
        Mark-Skipped -Name 'glslangValidator' -Reason 'not found in PATH'
    } elseif ($targetedMode -and $shaderFiles.Count -eq 0) {
        Mark-Skipped -Name 'glslangValidator' -Reason 'no matching shader files selected'
    } else {
        $shaderArgs = @()
        if ($targetedMode) {
            $shaderArgs += '-Files'
            $shaderArgs += $shaderFiles
        }

        $shaderPass = Invoke-Stage -Label 'glslangValidator' -ScriptName 'lint-shaders.ps1' -ExtraArgs $shaderArgs
        $results['glslangValidator'] = $shaderPass
        if (-not $shaderPass) { $overallPass = $false }
    }
}

$cppcheckTool = Get-Command cppcheck -ErrorAction SilentlyContinue
if (-not $cppcheckTool) {
    Mark-Skipped -Name 'cppcheck' -Reason 'not found in PATH'
} elseif ($targetedMode -and $cppcheckCodeFiles.Count -eq 0) {
    Mark-Skipped -Name 'cppcheck' -Reason 'no matching source files selected'
} else {
    $cppcheckArgs = @()
    if ($targetedMode) {
        $cppcheckArgs += '-Files'
        $cppcheckArgs += $cppcheckCodeFiles
    }

    $cppcheckPass = Invoke-Stage -Label 'cppcheck' -ScriptName 'lint-cppcheck.ps1' -ExtraArgs $cppcheckArgs
    $results['cppcheck'] = $cppcheckPass
    if (-not $cppcheckPass) { $overallPass = $false }
}

if ($Quick) {
    Mark-Skipped -Name 'clang-tidy' -Reason 'skipped by -Quick'
} else {
    $clangTidyTool = Get-Command clang-tidy -ErrorAction SilentlyContinue
    $compileDb = Get-VdeCompileDatabasePath -RepoRoot $projectRoot -Generator $Generator

    if (-not $clangTidyTool) {
        Mark-Skipped -Name 'clang-tidy' -Reason 'not found in PATH'
    } elseif (-not $compileDb) {
        $compileDbReason = if ($Generator -eq 'MSBuild') {
            'no MSBuild compile_commands.json found; use a Ninja build for clang-tidy coverage unless you generated one manually'
        } else {
            'no compile_commands.json found; use .\scripts\build.ps1 -Generator Ninja for clang-tidy coverage'
        }
        Mark-Skipped -Name 'clang-tidy' -Reason $compileDbReason
    } elseif ($targetedMode -and $codeFiles.Count -eq 0) {
        Mark-Skipped -Name 'clang-tidy' -Reason 'no matching source files selected'
    } else {
        $tidyArgs = @()
        $tidyArgs += '-Generator'
        $tidyArgs += $Generator
        if ($targetedMode) {
            $tidyArgs += '-Files'
            $tidyArgs += $codeFiles
        }

        $tidyPass = Invoke-Stage -Label 'clang-tidy' -ScriptName 'lint-clang-tidy.ps1' -ExtraArgs $tidyArgs
        $results['clang-tidy'] = $tidyPass
        if (-not $tidyPass) { $overallPass = $false }
    }
}

Write-Host ''
Write-Host $bar -ForegroundColor Cyan
Write-Host '  LINT SUMMARY' -ForegroundColor Cyan
Write-Host $bar -ForegroundColor Cyan

foreach ($name in $results.Keys) {
    $val = $results[$name]
    if ($val -is [string] -and $val -eq 'skipped') {
        Write-Host "  $name : SKIPPED" -ForegroundColor DarkGray
    } elseif ($val -is [bool] -and $val) {
        Write-Host "  $name : PASSED" -ForegroundColor Green
    } else {
        Write-Host "  $name : FAILED" -ForegroundColor Red
    }
}

Write-Host ''
if ($overallPass) {
    Write-Host '  OVERALL: ALL CHECKS PASSED' -ForegroundColor Green
} else {
    Write-Host '  OVERALL: SOME CHECKS FAILED' -ForegroundColor Red
}
Write-Host $bar -ForegroundColor Cyan
Write-Host ''

if ($overallPass) { exit 0 } else { exit 1 }
