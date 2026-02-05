# Format VDE Code
# Applies clang-format to all C++ source files in the project

param(
    [switch]$Check,  # Only check formatting without modifying files
    [switch]$Help
)

function Show-Help {
    Write-Host @"
Format VDE Code - Apply clang-format to all C++ files

Usage:
    .\scripts\format.ps1           # Format all files in-place
    .\scripts\format.ps1 -Check    # Check formatting without modifying files
    .\scripts\format.ps1 -Help     # Show this help message

Description:
    This script formats all C++ source files (.h, .cpp) in the VDE project
    according to the .clang-format configuration file.
    
    Files formatted:
    - include/vde/**/*.h
    - src/**/*.cpp
    - examples/**/*.cpp
    - tests/**/*.cpp

Requirements:
    - clang-format must be installed and available in PATH
    - Run from project root or scripts directory

"@
    exit 0
}

if ($Help) {
    Show-Help
}

# Navigate to project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

if (Test-Path $projectRoot) {
    Set-Location $projectRoot
}

# Check if clang-format is available
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) {
    Write-Host "ERROR: clang-format not found in PATH" -ForegroundColor Red
    Write-Host "Please install clang-format:" -ForegroundColor Yellow
    Write-Host "  - With Visual Studio: Install C++ clang tools component" -ForegroundColor Yellow
    Write-Host "  - With LLVM: Download from https://releases.llvm.org/" -ForegroundColor Yellow
    exit 1
}

Write-Host "Using clang-format: $($clangFormat.Source)" -ForegroundColor Cyan

# Find all C++ files
$patterns = @(
    "include\vde\**\*.h",
    "src\**\*.cpp",
    "examples\**\*.cpp",
    "tests\**\*.cpp"
)

$files = @()
foreach ($pattern in $patterns) {
    $found = Get-ChildItem -Path $pattern -Recurse -ErrorAction SilentlyContinue
    if ($found) {
        $files += $found
    }
}

if ($files.Count -eq 0) {
    Write-Host "No C++ files found to format" -ForegroundColor Yellow
    exit 0
}

Write-Host "Found $($files.Count) files to format" -ForegroundColor Cyan

if ($Check) {
    Write-Host "`nChecking formatting (files will not be modified)..." -ForegroundColor Cyan
    $needsFormatting = @()
    
    foreach ($file in $files) {
        $result = & clang-format --dry-run --Werror $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            $needsFormatting += $file
        }
    }
    
    if ($needsFormatting.Count -eq 0) {
        Write-Host "`n[SUCCESS] All files are properly formatted!" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "`n[FAILED] The following files need formatting:" -ForegroundColor Red
        foreach ($file in $needsFormatting) {
            Write-Host "  - $($file.FullName)" -ForegroundColor Yellow
        }
        Write-Host "`nRun without -Check to format these files" -ForegroundColor Cyan
        exit 1
    }
} else {
    Write-Host "`nFormatting files..." -ForegroundColor Cyan
    
    $formatted = 0
    foreach ($file in $files) {
        & clang-format -i $file.FullName
        if ($LASTEXITCODE -eq 0) {
            $formatted++
            Write-Host "  Formatted: $($file.FullName)" -ForegroundColor Gray
        } else {
            Write-Host "  ERROR: Failed to format $($file.FullName)" -ForegroundColor Red
        }
    }
    
    Write-Host "`n[SUCCESS] Formatted $formatted files" -ForegroundColor Green
}
