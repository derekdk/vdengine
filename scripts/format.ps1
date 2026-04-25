# Format VDE Code
# Applies clang-format to VDE C++ source files.

[CmdletBinding()]
param(
    [string[]]$Files = @(),

    [switch]$Check,

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
Format VDE Code - Apply clang-format to VDE C++ files

Usage:
    .\scripts\format.ps1                           # Format all files in-place
    .\scripts\format.ps1 -Check                    # Check formatting without modifying files
    .\scripts\format.ps1 -Files src\BufferUtils.cpp
    .\scripts\format.ps1 -Help                     # Show this help message

Description:
    Formats VDE C++ files according to the repository .clang-format configuration.
    With no -Files argument, the standard repository source roots are scanned.

Parameters:
    -Files <paths>   Optional explicit source/header file list (relative or absolute)
    -Check           Check formatting without modifying files
    -Help            Show this help message
"@
    exit 0
}

if ($Help) {
    Show-Help
}

$projectRoot = Get-VdeLintRepoRoot -ScriptRoot $PSScriptRoot
Set-Location $projectRoot

$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) {
    Write-Host "ERROR: clang-format not found in PATH" -ForegroundColor Red
    Write-Host "Please install clang-format:" -ForegroundColor Yellow
    Write-Host "  - With Visual Studio: Install C++ clang tools component" -ForegroundColor Yellow
    Write-Host "  - With LLVM: Download from https://releases.llvm.org/" -ForegroundColor Yellow
    exit 1
}

Write-Host "Using clang-format: $($clangFormat.Source)" -ForegroundColor Cyan

$allowedExtensions = @('.h', '.hpp', '.hh', '.hxx', '.cpp', '.cc', '.cxx')
if ($Files.Count -gt 0) {
    $targetFiles = Resolve-VdeFiles -RepoRoot $projectRoot -Files $Files -AllowedExtensions $allowedExtensions
} else {
    $patterns = @(
        'include\vde\**\*.h',
        'src\**\*.h',
        'src\**\*.cpp',
        'examples\**\*.h',
        'examples\**\*.cpp',
        'games\**\*.h',
        'games\**\*.cpp',
        'tests\**\*.h',
        'tests\**\*.cpp',
        'tools\**\*.h',
        'tools\**\*.cpp'
    )

    $targetFiles = @()
    foreach ($pattern in $patterns) {
        $targetFiles += Get-ChildItem -Path $pattern -Recurse -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    }
    $targetFiles = @($targetFiles | Sort-Object -Unique)
}

if (-not $targetFiles -or $targetFiles.Count -eq 0) {
    Write-Host 'No matching C++ files found to format' -ForegroundColor Yellow
    exit 0
}

Write-Host "Found $($targetFiles.Count) file(s) to process" -ForegroundColor Cyan

if ($Check) {
    Write-Host "`nChecking formatting (files will not be modified)..." -ForegroundColor Cyan
    $needsFormatting = @()

    foreach ($file in $targetFiles) {
        & clang-format --dry-run --Werror $file 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            $needsFormatting += $file
        }
    }

    if ($needsFormatting.Count -eq 0) {
        Write-Host "`n[SUCCESS] All files are properly formatted!" -ForegroundColor Green
        exit 0
    }

    Write-Host "`n[FAILED] The following files need formatting:" -ForegroundColor Red
    foreach ($file in $needsFormatting) {
        Write-Host "  - $file" -ForegroundColor Yellow
    }
    Write-Host "`nRun without -Check to format these files" -ForegroundColor Cyan
    exit 1
}

Write-Host "`nFormatting files..." -ForegroundColor Cyan

$formatted = 0
foreach ($file in $targetFiles) {
    & clang-format -i $file
    if ($LASTEXITCODE -eq 0) {
        $formatted++
        Write-Host "  Formatted: $file" -ForegroundColor Gray
    } else {
        Write-Host "  ERROR: Failed to format $file" -ForegroundColor Red
    }
}

Write-Host "`n[SUCCESS] Formatted $formatted files" -ForegroundColor Green
exit 0
