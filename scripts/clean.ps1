# VDE Clean Script
# Cleans build artifacts
# Usage: .\scripts\clean.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Full] [-Verbose] [-ProblemsOnly]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [switch]$Full = $false,

    [switch]$Verbose = $false,

    [switch]$ProblemsOnly = $false
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b)'
$warningPattern = '(?i)(^\s*warning\b|\bwarning:|\bwarn\b)'
$problemPattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed\b|^\s*warning\b|\bwarning:|\bwarn\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

$ProblemsOnly = Resolve-ProblemsOnlyPreference -BoundParameters $PSBoundParameters -VerboseRequested $Verbose
$ShowWarningsInProblemsOnly = Resolve-ProblemsOnlyWarningPreference -BoundParameters $PSBoundParameters

Write-Info "=========================================="
Write-Info "VDE Clean Script"
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
Write-Info "Full Clean: $Full"

if (-not (Test-Path $buildDir)) {
    if ($ProblemsOnly) {
        Write-Pass "Nothing to clean ($Generator $Config)."
    } else {
        Write-Warn "Build directory does not exist: $buildDir"
        Write-Info "Nothing to clean"
    }
    exit 0
}

if ($Full) {
    Write-Info "Performing FULL CLEAN - removing entire build directory..."
    Write-Info "This will require a full reconfigure on next build"
    
    Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
} else {
    Push-Location $buildDir
    try {
        if (-not (Test-Path "CMakeCache.txt")) {
            if ($ProblemsOnly) {
                Write-Pass "Nothing to clean ($Generator $Config)."
            } else {
                Write-Warn "No CMakeCache.txt found - build directory may not be configured"
            }
            exit 0
        }
        
        Write-Info "Cleaning build artifacts..."

        if ($ProblemsOnly) {
            if ($Generator -eq "Ninja") {
                if (Get-Command ninja -ErrorAction SilentlyContinue) {
                    $cleanResult = Invoke-CommandForProblemsOnly -Command { & ninja -t clean }
                } else {
                    $cleanResult = Invoke-CommandForProblemsOnly -Command { & cmake --build . --target clean }
                }
            } else {
                $cleanResult = Invoke-CommandForProblemsOnly -Command { & cmake --build . --config $Config --target clean }
            }

            if ($cleanResult.ExitCode -ne 0) {
                Write-Err "FAILURE: Clean command failed."
                exit 1
            }
        } else {
            if ($Generator -eq "Ninja") {
                # For Ninja, use ninja clean
                if (Get-Command ninja -ErrorAction SilentlyContinue) {
                    ninja -t clean
                } else {
                    # Fallback to cmake --build --target clean
                    cmake --build . --target clean
                }
            } else {
                # For MSBuild, use cmake --build --target clean with config
                cmake --build . --config $Config --target clean
            }

            if ($LASTEXITCODE -ne 0) {
                Write-Err "Clean command failed"
                exit 1
            }

            Write-Success "Clean complete"
        }
    }
    finally {
        Pop-Location
    }
}

if ($ProblemsOnly) {
    if ($Full) {
        Write-Pass "Full clean completed ($Generator $Config)."
    } else {
        Write-Pass "Clean completed ($Generator $Config)."
    }
} else {
    Write-Success "=========================================="
    if ($Full) {
        Write-Success "Full clean completed!"
    } else {
        Write-Success "Clean completed!"
    }
    Write-Success "=========================================="
}
