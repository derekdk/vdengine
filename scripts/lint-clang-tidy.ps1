# VDE clang-tidy Lint Script
# Run clang-tidy against VDE translation units selected from the compile database.
#
# Usage:
#   .\scripts\lint-clang-tidy.ps1
#   .\scripts\lint-clang-tidy.ps1 -Files src\BufferUtils.cpp
#   .\scripts\lint-clang-tidy.ps1 -Generator Ninja

[CmdletBinding()]
param(
    [string[]]$Files = @(),

    [ValidateSet("Auto", "Ninja", "MSBuild")]
    [string]$Generator = "Auto",

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
VDE clang-tidy Lint Script

Usage:
    .\scripts\lint-clang-tidy.ps1
    .\scripts\lint-clang-tidy.ps1 -Files src\BufferUtils.cpp
    .\scripts\lint-clang-tidy.ps1 -Generator Ninja

Description:
    Runs clang-tidy against VDE translation units discovered from compile_commands.json.
    With no -Files argument, all user translation units in the compile database are linted.
    With -Files, explicit source files and nearby translation units for changed headers are linted.

Parameters:
    -Files <paths>   Optional explicit file list (relative or absolute)
    -Generator       Prefer compile_commands.json from Ninja, MSBuild, or Auto (default)
    -Help            Show this help
"@
    exit 0
}

if ($Help) {
    Show-Help
}

$RepoRoot = Get-VdeLintRepoRoot -ScriptRoot $PSScriptRoot
Set-Location $RepoRoot

$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy) {
    Write-Host "SKIPPED: clang-tidy (not found in PATH)" -ForegroundColor DarkGray
    exit 0
}

$compileDb = Get-VdeCompileDatabasePath -RepoRoot $RepoRoot -Generator $Generator
if (-not $compileDb) {
    Write-Host "SKIPPED: clang-tidy (no compile_commands.json found)" -ForegroundColor DarkGray
    Write-Host "Generate one with: .\scripts\build.ps1 -Generator Ninja" -ForegroundColor DarkGray
    exit 0
}

$compileDbDir = Split-Path -Parent $compileDb
$compileEntries = Get-Content -Path $compileDb -Raw | ConvertFrom-Json

$translationUnits = @($compileEntries |
    ForEach-Object { ConvertTo-VdeFullPath -RepoRoot $RepoRoot -Path $_.file } |
    Where-Object { $_ } |
    Sort-Object -Unique)

$userTranslationUnits = @(foreach ($file in $translationUnits) {
    $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $file
    if (-not $relative) {
        continue
    }

    if ($relative.StartsWith("src\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("examples\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("games\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("tests\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("tools\", [System.StringComparison]::OrdinalIgnoreCase)) {
        $file
    }
})

function Add-MatchingTranslationUnits {
    param(
        [System.Collections.Generic.HashSet[string]]$TargetSet,
        [string[]]$Candidates,
        [string]$Prefix
    )

    foreach ($candidate in $Candidates) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $candidate
        if ($relative -and $relative.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            [void]$TargetSet.Add($candidate)
        }
    }
}

if ($Files.Count -gt 0) {
    $selectedFiles = Resolve-VdeFiles -RepoRoot $RepoRoot -Files $Files -AllowedExtensions @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx')
    $targetSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($file in $selectedFiles) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $file
        if (-not $relative) {
            continue
        }

        $extension = [System.IO.Path]::GetExtension($file)
        if ($extension -in @('.cpp', '.cc', '.cxx')) {
            if ($userTranslationUnits -contains $file) {
                [void]$targetSet.Add($file)
            }
            continue
        }

        if ($relative.StartsWith('include\vde\', [System.StringComparison]::OrdinalIgnoreCase) -or
            $relative.StartsWith('src\', [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-MatchingTranslationUnits -TargetSet $targetSet -Candidates $userTranslationUnits -Prefix 'src\'
            continue
        }

        if ($relative -match '^(examples|games|tools)\\([^\\]+)\\') {
            $prefix = "{0}\{1}\" -f $matches[1], $matches[2]
            Add-MatchingTranslationUnits -TargetSet $targetSet -Candidates $userTranslationUnits -Prefix $prefix
            continue
        }

        if ($relative.StartsWith('tests\', [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-MatchingTranslationUnits -TargetSet $targetSet -Candidates $userTranslationUnits -Prefix 'tests\'
        }
    }

    $targets = @($targetSet) | Sort-Object
} else {
    $targets = $userTranslationUnits | Sort-Object
}

if (-not $targets -or $targets.Count -eq 0) {
    Write-Host "SKIPPED: clang-tidy (no matching translation units selected)" -ForegroundColor DarkGray
    exit 0
}

Write-Host "Using compile database: $compileDb" -ForegroundColor Cyan
Write-Host "Running clang-tidy on $($targets.Count) translation unit(s)..." -ForegroundColor Cyan

$hasFindings = $false
foreach ($target in $targets) {
    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()

    try {
        $processArgs = @{
            FilePath               = $clangTidy.Source
            ArgumentList           = @('-p', $compileDbDir, '-quiet', $target)
            NoNewWindow            = $true
            Wait                   = $true
            PassThru               = $true
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError  = $stderrPath
        }
        $process = Start-Process @processArgs

        $output = @()
        if (Test-Path $stdoutPath) {
            $output += Get-Content -Path $stdoutPath
        }
        if (Test-Path $stderrPath) {
            $output += Get-Content -Path $stderrPath
        }

        $relevantOutput = $output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $reportedIssue = $false
        foreach ($line in $relevantOutput) {
            if ($line -match ':\d+:\d+:\s+(warning|error):') {
                $reportedIssue = $true
                break
            }
        }

        if ($process.ExitCode -ne 0 -or $reportedIssue) {
            Write-Host "FAILURE: clang-tidy issues in $target" -ForegroundColor Red
            foreach ($line in $relevantOutput) {
                Write-Host "  $line"
            }
            $hasFindings = $true
        }
    } finally {
        Remove-Item -Path $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    }
}

if ($hasFindings) {
    Write-Host "FAILURE: clang-tidy reported issues." -ForegroundColor Red
    exit 1
}

Write-Host "PASS: clang-tidy found no issues in the selected translation units." -ForegroundColor Green
exit 0