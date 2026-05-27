# VDE Clean All Script
# Cleans both Ninja and MSBuild build directories
# Usage: .\scripts\clean-all.ps1 [-Full] [-Verbose] [-ProblemsOnly]

param(
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
Write-Info "VDE Clean All Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir

$buildDirs = @(
    @{ Name = "Ninja"; Path = Join-Path $vdeRoot "build_ninja" },
    @{ Name = "MSBuild"; Path = Join-Path $vdeRoot "build" }
)

Write-Info "VDE Root: $vdeRoot"
Write-Info "Full Clean: $Full"
Write-Info ""

$cleanedCount = 0

foreach ($buildInfo in $buildDirs) {
    $buildDir = $buildInfo.Path
    $generatorName = $buildInfo.Name
    
    if (-not (Test-Path $buildDir)) {
        if (-not $ProblemsOnly) {
            Write-Warn "Build directory does not exist: $buildDir"
            Write-Info "Skipping $generatorName build"
            Write-Info ""
        }
        continue
    }
    
    Write-Info "Cleaning $generatorName build directory: $buildDir"
    
    if ($Full) {
        Write-Info "Performing FULL CLEAN - removing entire build directory..."
        Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
        if (-not $ProblemsOnly) {
            Write-Success "${generatorName}: Full clean complete - build directory removed"
        }
        $cleanedCount++
    } else {
        Push-Location $buildDir
        try {
            if (-not (Test-Path "CMakeCache.txt")) {
                if (-not $ProblemsOnly) {
                    Write-Warn "No CMakeCache.txt found - build directory may not be configured"
                    Write-Info "Skipping $generatorName build"
                }
            } else {
                Write-Info "Cleaning $generatorName build artifacts..."

                if ($ProblemsOnly) {
                    if ($generatorName -eq "Ninja") {
                        if (Get-Command ninja -ErrorAction SilentlyContinue) {
                            $cleanResult = Invoke-CommandForProblemsOnly -Command { & ninja -t clean }
                        } else {
                            $cleanResult = Invoke-CommandForProblemsOnly -Command { & cmake --build . --target clean }
                        }
                    } else {
                        $debugCleanResult = Invoke-CommandForProblemsOnly -Command { & cmake --build . --config Debug --target clean }
                        if ($debugCleanResult.ExitCode -ne 0) {
                            Write-Err "FAILURE: ${generatorName} clean failed for Debug."
                            exit 1
                        }

                        $cleanResult = Invoke-CommandForProblemsOnly -Command { & cmake --build . --config Release --target clean }
                    }

                    if ($cleanResult.ExitCode -ne 0) {
                        Write-Err "FAILURE: ${generatorName} clean failed."
                        exit 1
                    }
                } else {
                    if ($generatorName -eq "Ninja") {
                        # For Ninja, use ninja clean if available
                        if (Get-Command ninja -ErrorAction SilentlyContinue) {
                            ninja -t clean
                        } else {
                            cmake --build . --target clean
                        }
                    } else {
                        # For MSBuild, clean all configurations
                        cmake --build . --config Debug --target clean
                        cmake --build . --config Release --target clean
                    }

                    if ($LASTEXITCODE -ne 0) {
                        Write-Err "${generatorName}: Clean command failed"
                        exit 1
                    }

                    Write-Success "${generatorName}: Clean complete"
                }

                if (-not $ProblemsOnly) {
                    $cleanedCount++
                }

                if ($ProblemsOnly) {
                    $cleanedCount++
                }
            }
        }
        finally {
            Pop-Location
        }
    }
    
    Write-Info ""
}

if ($ProblemsOnly) {
    if ($cleanedCount -gt 0) {
        Write-Pass "Clean all completed ($cleanedCount build(s) cleaned)."
    } else {
        Write-Pass "Nothing to clean."
    }
} else {
    Write-Success "=========================================="
    if ($cleanedCount -gt 0) {
        Write-Success "Clean All completed! ($cleanedCount build(s) cleaned)"
    } else {
        Write-Warn "No builds were cleaned"
    }
    Write-Success "=========================================="
}
