# VDE Render Verification Script
# Runs golden-image comparison tests using FLIP perceptual metric.
# Auto-discovers examples with [render_verify] sections in vde.toml.
#
# Usage:
#   .\scripts\render-verify.ps1                          # Run all render verification tests
#   .\scripts\render-verify.ps1 -Extended                # Include priority 2 examples
#   .\scripts\render-verify.ps1 -Filter "*textured*"     # Filter by name
#   .\scripts\render-verify.ps1 -UpdateGolden            # Capture new golden images
#   .\scripts\render-verify.ps1 -Build -Verbose          # Build first, verbose output
#   .\scripts\render-verify.ps1 -ProblemsOnly            # Emit only warnings/failures plus final PASS/FAIL

param(
    [string]$Filter = "",  # Wildcard filter for executable names

    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$Build = $false,

    [switch]$Extended = $false,

    [switch]$UpdateGolden = $false,  # Capture new golden images instead of comparing

    [switch]$Verbose = $false,

    [switch]$ProblemsOnly = $false
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b)'
$warningPattern = '(?i)\b(warn|warning|validation)\b'
$problemPattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b|^\s*warn(ing)?\b|\bwarning:|\bvalidation\b)'
$outputFailurePattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

Write-Info "=========================================="
Write-Info "VDE Render Verification Script"
Write-Info "=========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot = Split-Path -Parent $scriptDir

# Select build directory based on generator
if ($Generator -eq "Ninja") {
    $buildDir = Join-Path $vdeRoot "build_ninja"
} else {
    $buildDir = Join-Path $vdeRoot "build"
}

$scriptBaseDir = Join-Path $vdeRoot 'smoketests\scripts'
$goldenDir = Join-Path $vdeRoot 'smoketests\golden'
$outputDir = Join-Path $vdeRoot 'render_verify_output'
$diffDir = Join-Path $vdeRoot 'logs\render_diffs'

Write-Info "VDE Root: $vdeRoot"
Write-Info "Generator: $Generator"
Write-Info "Build Directory: $buildDir"
Write-Info "Configuration: $Config"
if ($UpdateGolden) {
    Write-Info "Mode: Update Golden Images"
} else {
    Write-Info "Mode: Verify Against Golden Images"
}
if ($Extended) {
    Write-Info "Verify Set: Extended (priority 1 and 2)"
} else {
    Write-Info "Verify Set: Normal (priority 1 only)"
}
if ($Filter) {
    Write-Info "Filter: $Filter"
}

# Build if requested
if ($Build) {
    if ($ProblemsOnly) {
        Invoke-BuildForProblemsOnly -BuildScriptPath "$scriptDir\build.ps1" -SelectedGenerator $Generator -SelectedConfig $Config
    } else {
        Write-Info "Building before running render verification..."
        & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed! Cannot run render verification."
            exit 1
        }
    }
}

# --- TOML Parsing ---

$renderVerifyTomlCache = @{}

function Get-RenderVerifySectionMap {
    param([string]$TomlPath)

    if ($renderVerifyTomlCache.ContainsKey($TomlPath)) {
        return $renderVerifyTomlCache[$TomlPath]
    }

    $result = $null
    $currentSection = ''

    if (Test-Path $TomlPath) {
        foreach ($rawLine in Get-Content -Path $TomlPath) {
            $line = $rawLine.Trim()
            if (-not $line -or $line.StartsWith('#')) {
                continue
            }

            if ($line -match '^\[(.+)\]\s*$') {
                $currentSection = $Matches[1]
                if ($currentSection -eq 'render_verify' -and -not $result) {
                    $result = [ordered]@{
                        Scripts       = @()
                        CaptureScript = $null
                        Priority      = 1
                        Golden        = $null
                        Threshold     = 0.05
                    }
                }
                continue
            }

            if ($currentSection -ne 'render_verify' -or -not $result) {
                continue
            }

            if ($line -match '^scripts\s*=\s*\[(.*)\]\s*$') {
                $scripts = @()
                foreach ($scriptMatch in [regex]::Matches($Matches[1], '"([^"]+)"')) {
                    $scripts += [System.IO.Path]::GetFileName($scriptMatch.Groups[1].Value)
                }
                $result['Scripts'] = @($scripts)
                continue
            }

            if ($line -match '^priority\s*=\s*([0-9]+)\s*$') {
                $result['Priority'] = [int]$Matches[1]
                continue
            }

            if ($line -match '^golden\s*=\s*"([^"]+)"\s*$') {
                $result['Golden'] = $Matches[1]
                continue
            }

            if ($line -match '^capture_script\s*=\s*"([^"]+)"\s*$') {
                $result['CaptureScript'] = [System.IO.Path]::GetFileName($Matches[1])
                continue
            }

            if ($line -match '^threshold\s*=\s*([0-9.]+)\s*$') {
                $result['Threshold'] = [double]$Matches[1]
            }
        }
    }

    $renderVerifyTomlCache[$TomlPath] = $result
    return $result
}

# --- Source Directory Resolution ---

# Build the target-to-source mapping from a category CMakeLists.txt
function Get-CategoryTargetSourceMap {
    param([string]$CmakePath)

    $targetMap = @{}
    if (-not (Test-Path $CmakePath)) {
        return $targetMap
    }

    $collecting = $false
    $currentTarget = ''
    $currentBlockLines = @()

    foreach ($line in Get-Content -Path $CmakePath) {
        if (-not $collecting) {
            if ($line -match '^\s*(add_vde_example|add_executable)\(\s*([A-Za-z0-9_]+)') {
                $currentTarget = $Matches[2]
                $currentBlockLines = @($line)

                if ($line -match '\)') {
                    Add-TargetSourceMapEntry -TargetMap $targetMap -TargetName $currentTarget -BlockLines $currentBlockLines
                    $currentTarget = ''
                    $currentBlockLines = @()
                } else {
                    $collecting = $true
                }
            }
            continue
        }

        $currentBlockLines += $line
        if ($line -match '\)') {
            Add-TargetSourceMapEntry -TargetMap $targetMap -TargetName $currentTarget -BlockLines $currentBlockLines
            $collecting = $false
            $currentTarget = ''
            $currentBlockLines = @()
        }
    }

    return $targetMap
}

function Add-TargetSourceMapEntry {
    param(
        [hashtable]$TargetMap,
        [string]$TargetName,
        [string[]]$BlockLines
    )

    if ([string]::IsNullOrWhiteSpace($TargetName) -or $TargetName -notlike 'vde_*') {
        return
    }

    $blockText = $BlockLines -join "`n"
    $pathMatches = [regex]::Matches($blockText, '["'']?(?<path>[A-Za-z0-9_./-]+/[^"''\s\)]+)["'']?')
    foreach ($pathMatch in $pathMatches) {
        $sourcePath = $pathMatch.Groups['path'].Value
        if (-not $sourcePath) { continue }
        if ($sourcePath.StartsWith('$')) { continue }

        $sourceDir = $sourcePath.Split('/')[0]
        if (-not $sourceDir -or $sourceDir -in @('assets', 'shaders')) { continue }

        $TargetMap[$TargetName] = Join-Path (Join-Path $vdeRoot 'examples') $sourceDir
        return
    }
}

function Resolve-AppSourceDir {
    param([string]$TargetName)

    if ($exampleTargetSourceMap.ContainsKey($TargetName)) {
        return $exampleTargetSourceMap[$TargetName]
    }

    if ($gameTargetSourceMap.ContainsKey($TargetName)) {
        return $gameTargetSourceMap[$TargetName]
    }

    $candidateNames = @()
    if ($TargetName.StartsWith('vde_')) {
        $candidateNames += $TargetName.Substring(4)
    } else {
        $candidateNames += $TargetName
    }

    if ($candidateNames[0].EndsWith('_example')) {
        $candidateNames += $candidateNames[0].Substring(0, $candidateNames[0].Length - '_example'.Length)
    }

    foreach ($candidateName in ($candidateNames | Select-Object -Unique)) {
        $candidateDir = Join-Path (Join-Path $vdeRoot 'examples') $candidateName
        if (Test-Path (Join-Path $candidateDir 'vde.toml')) {
            return $candidateDir
        }

        $candidateDir = Join-Path (Join-Path $vdeRoot 'games') $candidateName
        if (Test-Path (Join-Path $candidateDir 'vde.toml')) {
            return $candidateDir
        }
    }

    return $null
}

$exampleTargetSourceMap = Get-CategoryTargetSourceMap -CmakePath (Join-Path $vdeRoot 'examples\CMakeLists.txt')
$gameTargetSourceMap = Get-CategoryTargetSourceMap -CmakePath (Join-Path $vdeRoot 'games\CMakeLists.txt')

# --- Executable Discovery ---

function Get-VerifyExes {
    $searchDirs = @()
    if ($Generator -eq "Ninja") {
        $searchDirs += Join-Path $buildDir "examples"
        $searchDirs += Join-Path $buildDir "games"
    } else {
        $searchDirs += Join-Path $buildDir "examples\$Config"
        $searchDirs += Join-Path $buildDir "games\$Config"
    }

    $exes = @()
    foreach ($dir in $searchDirs) {
        if (-not (Test-Path $dir)) {
            Write-Warn "Directory not found: $dir"
            continue
        }

        foreach ($exeFile in Get-ChildItem -Path $dir -Recurse -Filter "vde_*.exe" -File) {
            $targetName = [System.IO.Path]::GetFileNameWithoutExtension($exeFile.Name)
            $sourceDir = Resolve-AppSourceDir -TargetName $targetName
            if (-not $sourceDir) { continue }

            $tomlPath = Join-Path $sourceDir 'vde.toml'
            $rvSection = Get-RenderVerifySectionMap -TomlPath $tomlPath
            if (-not $rvSection) { continue }
            if ($rvSection['Scripts'].Count -eq 0) { continue }

            $exes += [pscustomobject]@{
                Name          = $exeFile.Name
                FullPath      = $exeFile.FullName
                WorkDir       = $exeFile.DirectoryName
                VerifyScript  = $rvSection['Scripts'][0]
                CaptureScript = $rvSection['CaptureScript']
                Priority      = $rvSection['Priority']
                Golden        = $rvSection['Golden']
                Threshold     = $rvSection['Threshold']
            }
        }
    }

    return @($exes | Sort-Object FullPath -Unique)
}

# Gather executables
$allExes = Get-VerifyExes

# Apply wildcard filter
if ($Filter) {
    $allExes = @($allExes | Where-Object { $_.Name -like $Filter })
}

$discoveredCount = $allExes.Count
$filteredPriority2Count = 0
if (-not $Extended) {
    $filteredPriority2Count = @($allExes | Where-Object { $_.Priority -eq 2 }).Count
    $allExes = @($allExes | Where-Object { $_.Priority -eq 1 })
}

if ($allExes.Count -eq 0) {
    Write-Warn "No executables with [render_verify] metadata found."
    if ($Filter) {
        Write-Warn "Filter '$Filter' matched nothing."
    }
    if (-not $Extended -and $filteredPriority2Count -gt 0) {
        Write-Warn "Only priority 2 examples matched. Re-run with -Extended."
    }
    Write-Warn "Ensure examples/games have [render_verify] sections in vde.toml."
    exit 1
}

# Sort alphabetically
$allExes = @($allExes | Sort-Object Name)

Write-Info ""
Write-Info "Selected $($allExes.Count) executable(s) for render verification (from $discoveredCount discovered):"
if (-not $Extended -and $filteredPriority2Count -gt 0) {
    Write-Info "  Priority 2 excluded: $filteredPriority2Count"
}

# Ensure output directories exist
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}
if (-not (Test-Path $goldenDir)) {
    New-Item -ItemType Directory -Path $goldenDir -Force | Out-Null
}
if (-not (Test-Path $diffDir)) {
    New-Item -ItemType Directory -Path $diffDir -Force | Out-Null
}

# --- Run Render Verification ---

$results = @()
$passCount = 0
$failCount = 0
$skipCount = 0

$env:VDE_INPUT_SCRIPT = $null

Write-Info ""
Write-Info "Running render verification..."
Write-Info "=========================================="

foreach ($exe in $allExes) {
    # In UpdateGolden mode, use the capture script (no compare step)
    if ($UpdateGolden -and $exe.CaptureScript) {
        $verifyScript = $exe.CaptureScript
    } else {
        $verifyScript = $exe.VerifyScript
    }
    $verifyScriptPath = Join-Path $scriptBaseDir $verifyScript

    if (-not (Test-Path $verifyScriptPath)) {
        Write-Warn "  Verify script not found: $verifyScript (skipping $($exe.Name))"
        $skipCount++
        $results += [pscustomobject]@{
            exe      = $exe.Name
            script   = $verifyScript
            status   = 'skipped'
            exitCode = 'n/a'
            passed   = $false
            skipped  = $true
            keyLines = @("Verify script not found: $verifyScript")
        }
        continue
    }

    # In UpdateGolden mode, check if golden image is configured
    if (-not $UpdateGolden) {
        $goldenImagePath = Join-Path $goldenDir $exe.Golden
        if ($exe.Golden -and -not (Test-Path $goldenImagePath)) {
            Write-Warn "  Golden image not found: $($exe.Golden) (skipping $($exe.Name)). Run with -UpdateGolden first."
            $skipCount++
            $results += [pscustomobject]@{
                exe      = $exe.Name
                script   = $verifyScript
                status   = 'skipped'
                exitCode = 'n/a'
                passed   = $false
                skipped  = $true
                keyLines = @("Golden image not found: $($exe.Golden) - run with -UpdateGolden first")
            }
            continue
        }
    }

    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()
    $started = $false
    $status = ''
    $exitCode = ''

    try {
        $actionLabel = if ($UpdateGolden) { "Capturing" } else { "Verifying" }
        if ($Verbose) {
            Write-Info "  ${actionLabel}: $($exe.Name) with $verifyScript"
        } elseif (-not $ProblemsOnly) {
            Write-Host "  ${actionLabel}: $($exe.Name)" -NoNewline
        }

        $job = Start-Job -ScriptBlock {
            param($exePath, $scriptPath, $workDir, $stdoutFile, $stderrFile)
            Set-Location $workDir
            & $exePath "--input-script" $scriptPath > $stdoutFile 2> $stderrFile
            return $LASTEXITCODE
        } -ArgumentList $exe.FullPath, $verifyScriptPath, $vdeRoot, $stdout, $stderr

        $started = $true

        # 20-second timeout (screenshot capture + FLIP comparison takes longer)
        $completed = Wait-Job $job -Timeout 20

        if ($completed) {
            $exitCode = [string](Receive-Job $job)
            $status = 'exited'
        } else {
            Stop-Job $job
            $status = 'timeout'
            $exitCode = 'timeout'
        }

        Remove-Job $job -Force -ErrorAction SilentlyContinue
    } catch {
        $status = 'failed_to_start'
        $exitCode = 'n/a'
    }

    $allLines = @()
    if (Test-Path $stdout) { $allLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { $allLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue }

    $keyLines = Get-ProblemLines -Lines $allLines -MaxLines 20

    if ($status -eq 'exited' -and @($allLines | Where-Object { $_ -match $outputFailurePattern }).Count -gt 0) {
        $status = 'output_failure'
        $exitCode = 'output_failure'
    }

    $passed = ($exitCode -eq '0')

    # In UpdateGolden mode, copy screenshot to golden directory.
    # Note: examples set cwd to exe directory at startup (setWorkingDirectoryToExecutablePath),
    # so screenshots are saved relative to the exe's build output directory, not $vdeRoot.
    # Fallback: check $vdeRoot in case the exe doesn't change cwd.
    if ($UpdateGolden -and $passed -and $exe.Golden) {
        $capturedPath = Join-Path (Join-Path $exe.WorkDir 'render_verify_output') $exe.Golden
        if (-not (Test-Path $capturedPath)) {
            $capturedPath = Join-Path $vdeRoot 'render_verify_output' $exe.Golden
        }
        if (Test-Path $capturedPath) {
            Copy-Item -Path $capturedPath -Destination (Join-Path $goldenDir $exe.Golden) -Force
            if ($Verbose) {
                Write-Info "    Updated golden: $($exe.Golden)"
            }
        } else {
            Write-Warn "    Screenshot not found at expected path: $capturedPath"
        }
    }

    if ($passed) {
        $passCount++
        if ($ProblemsOnly) {
            Write-ProblemLines -Prefix "[$($exe.Name)]: " -Lines @($keyLines | Where-Object { Test-IsWarningLine -Line $_ })
        } elseif ($Verbose) {
            Write-Success "  PASSED"
        } else {
            Write-Success " PASSED"
        }
    } else {
        $failCount++
        if ($ProblemsOnly) {
            Write-Err "FAILURE: $($exe.Name) (script: $verifyScript, exit code: $exitCode)"
            if ($keyLines) {
                Write-ProblemLines -Prefix "$($exe.Name): " -Lines $keyLines
            }
        } elseif ($Verbose) {
            Write-Err "  FAILED (exit code: $exitCode)"
        } else {
            Write-Err " FAILED (exit code: $exitCode)"
        }

        if ($keyLines -and $Verbose -and -not $ProblemsOnly) {
            Write-Err "  Error output:"
            foreach ($line in $keyLines) {
                Write-Err "    $line"
            }
        }
    }

    $results += [pscustomobject]@{
        exe      = $exe.Name
        script   = $verifyScript
        status   = $status
        exitCode = $exitCode
        passed   = $passed
        skipped  = $false
        keyLines = @($keyLines)
    }

    # Clean up temp files
    if (Test-Path $stdout) { Remove-Item $stdout -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { Remove-Item $stderr -ErrorAction SilentlyContinue }
}

# --- Summary ---

$totalRun = $passCount + $failCount

if ($ProblemsOnly) {
    if ($failCount -gt 0) {
        Write-Err "FAILURE: Render verification failed. Passed: $passCount, Failed: $failCount, Skipped: $skipCount"
        exit 1
    }

    if ($skipCount -gt 0) {
        Write-Pass "Render verification passed with $skipCount warning(s)."
    } else {
        Write-Pass "All render verification tests passed."
    }
    exit 0
}

Write-Info ""
Write-Info "=========================================="
if ($UpdateGolden) {
    Write-Info "Golden Image Update Summary"
} else {
    Write-Info "Render Verification Summary"
}
Write-Info "=========================================="
Write-Info "Total: $totalRun (discovered: $($allExes.Count), skipped: $skipCount)"

Write-Success "Passed: $passCount"
if ($failCount -gt 0) {
    Write-Err "Failed: $failCount"
}
if ($skipCount -gt 0) {
    Write-Warn "Skipped: $skipCount"
}

if ($failCount -gt 0) {
    Write-Err ""
    Write-Err "Failed:"
    foreach ($result in $results | Where-Object { -not $_.passed -and -not $_.skipped }) {
        Write-Err "  - $($result.exe) (exit code: $($result.exitCode))"
        if ($result.keyLines -and $Verbose) {
            foreach ($line in $result.keyLines) {
                Write-Err "      $line"
            }
        }
    }

    Write-Err ""
    Write-Err "=========================================="
    Write-Err "Render verification FAILED!"
    Write-Err "=========================================="
    exit 1
}

Write-Success ""
Write-Success "=========================================="
if ($UpdateGolden) {
    Write-Success "Golden images updated successfully!"
} else {
    Write-Success "All render verification tests PASSED!"
}
Write-Success "=========================================="
exit 0
