# VDE Shader Lint Script
# Validate Vulkan GLSL shader files with glslangValidator.
#
# Usage:
#   .\scripts\lint-shaders.ps1
#   .\scripts\lint-shaders.ps1 -Files shaders\sprite.vert

[CmdletBinding()]
param(
    [string[]]$Files = @(),

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
VDE Shader Lint Script

Usage:
    .\scripts\lint-shaders.ps1
    .\scripts\lint-shaders.ps1 -Files shaders\sprite.vert

Description:
    Validates VDE Vulkan GLSL shaders with glslangValidator.
    With no -Files argument, all shaders under shaders/ are checked.

Parameters:
    -Files <paths>   Optional explicit shader file list (relative or absolute)
    -Help            Show this help
"@
    exit 0
}

if ($Help) {
    Show-Help
}

$RepoRoot = Get-VdeLintRepoRoot -ScriptRoot $PSScriptRoot
Set-Location $RepoRoot

$glslangValidator = Get-Command glslangValidator -ErrorAction SilentlyContinue
if (-not $glslangValidator) {
    Write-Host "SKIPPED: glslangValidator (not found in PATH)" -ForegroundColor DarkGray
    exit 0
}

if ($Files.Count -gt 0) {
    $shaderFiles = Resolve-VdeFiles -RepoRoot $RepoRoot -Files $Files -AllowedExtensions @('.vert', '.frag')
} else {
    $shaderFiles = @(Get-ChildItem -Path (Join-Path $RepoRoot 'shaders') -Include '*.vert', '*.frag' -Recurse -File -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName |
        Sort-Object -Unique)
}

if (-not $shaderFiles -or $shaderFiles.Count -eq 0) {
    Write-Host "SKIPPED: glslangValidator (no matching shader files selected)" -ForegroundColor DarkGray
    exit 0
}

Write-Host "Validating $($shaderFiles.Count) shader file(s)..." -ForegroundColor Cyan

$shaderPass = $true
foreach ($shader in $shaderFiles) {
    Write-Host "  Checking $shader" -ForegroundColor Gray
    $output = & glslangValidator -V $shader 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILURE: shader validation failed for $shader" -ForegroundColor Red
        foreach ($line in $output) {
            Write-Host "  $line"
        }
        $shaderPass = $false
    }
}

if (-not $shaderPass) {
    Write-Host "FAILURE: one or more shaders failed validation." -ForegroundColor Red
    exit 1
}

Write-Host "PASS: all selected shaders passed validation." -ForegroundColor Green
exit 0