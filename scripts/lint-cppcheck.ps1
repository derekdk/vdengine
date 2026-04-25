# VDE cppcheck Lint Script
# Run cppcheck against VDE source files.
#
# Usage:
#   .\scripts\lint-cppcheck.ps1
#   .\scripts\lint-cppcheck.ps1 -Files src\BufferUtils.cpp include\vde\Color.h

[CmdletBinding()]
param(
    [string[]]$Files = @(),

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
VDE cppcheck Lint Script

Usage:
    .\scripts\lint-cppcheck.ps1
    .\scripts\lint-cppcheck.ps1 -Files src\BufferUtils.cpp include\vde\Color.h

Description:
    Runs cppcheck with the repository's required configuration.
    With no -Files argument, the standard repository source roots are checked.

Parameters:
    -Files <paths>   Optional explicit source/header file list (relative or absolute)
    -Help            Show this help
"@
    exit 0
}

if ($Help) {
    Show-Help
}

$RepoRoot = Get-VdeLintRepoRoot -ScriptRoot $PSScriptRoot
Set-Location $RepoRoot

$cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
if (-not $cppcheck) {
    Write-Host "SKIPPED: cppcheck (not found in PATH)" -ForegroundColor DarkGray
    exit 0
}

$cppcheckSupportsProgress = $false
try {
    $cppcheckHelp = & cppcheck --help 2>&1
    $cppcheckSupportsProgress = ($cppcheckHelp | Select-String -SimpleMatch '--report-progress' -Quiet)
} catch {
    $cppcheckSupportsProgress = $false
}

$cppcheckArgs = @(
    '--enable=warning,performance,portability',
    '--std=c++20',
    '--error-exitcode=1',
    '--library=googletest',
    '--suppress=missingIncludeSystem',
    '--suppress=missingInclude',
    '--suppress=unmatchedSuppression',
    '--suppress=ctuOneDefinitionRuleViolation',
    '--inline-suppr',
    '-Iinclude',
    '-i', 'third_party'
)

if ($Files.Count -gt 0) {
    $selectedFiles = Resolve-VdeFiles -RepoRoot $RepoRoot -Files $Files -AllowedExtensions @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx')
    if (-not $selectedFiles -or $selectedFiles.Count -eq 0) {
        Write-Host "SKIPPED: cppcheck (no matching source files selected)" -ForegroundColor DarkGray
        exit 0
    }

    foreach ($file in $selectedFiles) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $file
        if ($relative) {
            $cppcheckArgs += $relative
        } else {
            $cppcheckArgs += $file
        }
    }
} else {
    $cppcheckArgs += @('src/', 'include/vde/', 'examples/', 'games/', 'tests/', 'tools/')
}

if ($cppcheckSupportsProgress) {
    Write-Host 'Progress reporting enabled; cppcheck may still pause on a file while exploring multiple macro configurations.' -ForegroundColor DarkGray
    $cppcheckArgs = @('--report-progress') + $cppcheckArgs
}

Write-Host 'Running cppcheck...' -ForegroundColor Cyan
& cppcheck @cppcheckArgs 2>&1 | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0) {
    Write-Host 'FAILURE: cppcheck reported issues.' -ForegroundColor Red
    exit 1
}

Write-Host 'PASS: cppcheck found no issues.' -ForegroundColor Green
exit 0