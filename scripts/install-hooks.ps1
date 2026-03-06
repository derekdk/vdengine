# VDE Git Hook Setup Script
# Configures repository-managed hooks for this local clone.
# Usage: .\scripts\install-hooks.ps1

$ErrorActionPreference = "Stop"

function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir
$hooksDir = Join-Path $vdeRoot ".githooks"

Write-Info "=========================================="
Write-Info "VDE Git Hook Setup"
Write-Info "=========================================="
Write-Info "VDE Root: $vdeRoot"
Write-Info "Hooks Directory: $hooksDir"

$requiredHooks = @("pre-commit", "pre-push")
foreach ($hookName in $requiredHooks) {
    $hookPath = Join-Path $hooksDir $hookName
    if (-not (Test-Path $hookPath)) {
        Write-Err "Required hook file is missing: $hookPath"
        exit 1
    }
}

Push-Location $vdeRoot
try {
    Write-Info "Configuring local core.hooksPath to .githooks..."
    git config --local core.hooksPath .githooks
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Failed to configure core.hooksPath"
        exit 1
    }

    $configuredPath = git config --local --get core.hooksPath
    if ($configuredPath -ne ".githooks") {
        Write-Err "Unexpected hooks path after configuration: $configuredPath"
        exit 1
    }

    if (-not (Get-Command git-lfs -ErrorAction SilentlyContinue)) {
        Write-Warn "git-lfs was not found on PATH. Pushes may fail due to the pre-push hook."
    }

    Write-Success "=========================================="
    Write-Success "Git hooks configured successfully"
    Write-Success "=========================================="
    Write-Info "Direct commits to 'main' are now blocked for this local clone."
    Write-Info "Current core.hooksPath: $configuredPath"
}
finally {
    Pop-Location
}
