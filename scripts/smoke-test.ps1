# VDE Smoke Test Script
# Runs automated smoke tests on example and tool executables
# Auto-discovers vde_*.exe in the build directory for both examples and tools.
#
# Usage:
#   .\scripts\smoke-test.ps1                              # Run all (examples + tools)
#   .\scripts\smoke-test.ps1 -Category Examples           # Examples only
#   .\scripts\smoke-test.ps1 -Category Tools              # Tools only
#   .\scripts\smoke-test.ps1 -Filter "*physics*"          # Filter by name
#   .\scripts\smoke-test.ps1 -Build -Verbose              # Build first, verbose output

param(
    [ValidateSet("All", "Examples", "Tools")]
    [string]$Category = "All",

    [string]$Filter = "",  # Wildcard filter for executable names (e.g. "*physics*", "vde_vlauncher*")

    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$Build = $false,  # Build before testing

    [switch]$Verbose = $false  # Verbose output
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-Info { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-Warn { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-Err { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-Info "=========================================="
Write-Info "VDE Smoke Test Script"
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
Write-Info "Category: $Category"
if ($Filter) {
    Write-Info "Filter: $Filter"
}

# Build if requested
if ($Build) {
    Write-Info "Building before running smoke tests..."
    & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Build failed! Cannot run smoke tests."
        exit 1
    }
}

# --- Configuration ---

# Executables to exclude from example smoke testing
$excludeFromExamples = @(
    'vde_triangle_example.exe'      # Doesn't use Game API
    'vde_geometry_repl_example.exe'  # Legacy duplicate
    'vde_geometry_repl.exe'          # Stale build artifact (the real one is in tools/)
)

# Executables to exclude from tool smoke testing
$excludeFromTools = @(
)

# Map of executable names to their specific smoke scripts.
# If not listed here, uses smoke_quick.vdescript as fallback.
$smokeScriptMap = @{
    # Examples
    'vde_physics_demo.exe'             = 'smoke_physics_demo.vdescript'
    'vde_breakout_demo.exe'            = 'smoke_breakout.vdescript'
    'vde_asteroids_demo.exe'           = 'smoke_asteroids.vdescript'
    'vde_asteroids_physics_demo.exe'   = 'smoke_asteroids_physics.vdescript'
    'vde_sprite_demo.exe'              = 'smoke_sprite.vdescript'
    'vde_multi_scene_demo.exe'         = 'smoke_multi_scene.vdescript'
    'vde_imgui_demo.exe'               = 'smoke_imgui.vdescript'
    'vde_audio_demo.exe'               = 'smoke_audio.vdescript'
    'vde_materials_lighting_demo.exe'  = 'smoke_materials.vdescript'
    'vde_textured_cube_demo.exe'       = 'smoke_textured_cube.vdescript'
    'vde_resource_demo.exe'            = 'smoke_resource.vdescript'
    'vde_four_scene_3d_demo.exe'       = 'smoke_four_scene_3d.vdescript'
    # Tools
    'vde_vlauncher.exe'                = 'smoke_vlauncher.vdescript'
    'vde_geometry_repl.exe'            = 'smoke_geometry_repl.vdescript'
}

$defaultSmoke = 'smoke_quick.vdescript'
$scriptBaseDir = Join-Path $vdeRoot 'smoketests\scripts'

# --- Executable Discovery ---

function Get-ExampleExes {
    if ($Generator -eq "Ninja") {
        $dir = Join-Path $buildDir "examples"
    } else {
        $dir = Join-Path $buildDir "examples\$Config"
    }

    if (-not (Test-Path $dir)) {
        Write-Warn "Examples directory not found: $dir"
        return @()
    }

    $exes = Get-ChildItem -Path $dir -Filter "vde_*.exe" -File |
        Where-Object { $_.Name -notin $excludeFromExamples } |
        ForEach-Object {
            [pscustomobject]@{
                Name     = $_.Name
                FullPath = $_.FullName
                WorkDir  = $_.DirectoryName
                Category = "Example"
            }
        }
    return @($exes)
}

function Get-ToolExes {
    $dir = Join-Path $buildDir "tools"

    if (-not (Test-Path $dir)) {
        Write-Warn "Tools directory not found: $dir"
        return @()
    }

    # Tools are in subdirectories: tools/<toolname>/vde_*.exe (Ninja)
    # or tools/<toolname>/<Config>/vde_*.exe (MSBuild)
    $exes = Get-ChildItem -Path $dir -Recurse -Filter "vde_*.exe" -File |
        Where-Object { $_.Name -notin $excludeFromTools } |
        ForEach-Object {
            [pscustomobject]@{
                Name     = $_.Name
                FullPath = $_.FullName
                WorkDir  = $_.DirectoryName
                Category = "Tool"
            }
        }
    return @($exes)
}

# Gather executables based on category
$allExes = @()

if ($Category -eq "All" -or $Category -eq "Examples") {
    $allExes += Get-ExampleExes
}

if ($Category -eq "All" -or $Category -eq "Tools") {
    $allExes += Get-ToolExes
}

# Apply wildcard filter
if ($Filter) {
    $allExes = @($allExes | Where-Object { $_.Name -like $Filter })
}

if ($allExes.Count -eq 0) {
    Write-Warn "No executables found to test."
    if ($Filter) {
        Write-Warn "Filter '$Filter' matched nothing. Try a different pattern."
    }
    Write-Warn "Run with -Build flag to build first, or run .\scripts\build.ps1"
    exit 1
}

# Sort: examples first, then tools, alphabetically within each category
$allExes = @($allExes | Sort-Object Category, Name)

Write-Info ""
Write-Info "Discovered $($allExes.Count) executable(s) to test:"
$exampleCount = @($allExes | Where-Object { $_.Category -eq "Example" }).Count
$toolCount = @($allExes | Where-Object { $_.Category -eq "Tool" }).Count
if ($exampleCount -gt 0) { Write-Info "  Examples: $exampleCount" }
if ($toolCount -gt 0) { Write-Info "  Tools:    $toolCount" }

# --- Run Smoke Tests ---

$results = @()
$passCount = 0
$failCount = 0
$skipCount = 0

# Clear VDE_INPUT_SCRIPT environment variable to avoid contamination
$env:VDE_INPUT_SCRIPT = $null

$currentCategory = ""

Write-Info ""
Write-Info "Running smoke tests..."
Write-Info "=========================================="

foreach ($exe in $allExes) {
    # Print category header when it changes
    if ($exe.Category -ne $currentCategory) {
        $currentCategory = $exe.Category
        Write-Info ""
        Write-Info "--- ${currentCategory}s ---"
    }

    # Select smoke script
    $smokeScript = $defaultSmoke
    if ($smokeScriptMap.ContainsKey($exe.Name)) {
        $smokeScript = $smokeScriptMap[$exe.Name]
    }

    $smokeScriptPath = Join-Path $scriptBaseDir $smokeScript

    if (-not (Test-Path $smokeScriptPath)) {
        Write-Warn "  Smoke script not found: $smokeScript (skipping $($exe.Name))"
        $skipCount++
        $results += [pscustomobject]@{
            exe      = $exe.Name
            category = $exe.Category
            script   = $smokeScript
            started  = $false
            status   = 'skipped'
            exitCode = 'n/a'
            passed   = $false
            skipped  = $true
            keyLines = @("Smoke script not found: $smokeScript")
        }
        continue
    }

    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()
    $started = $false
    $status = ''
    $exitCode = ''
    $startError = $null

    try {
        if ($Verbose) {
            Write-Info "  Testing: $($exe.Name) with $smokeScript"
        } else {
            Write-Host "  Testing: $($exe.Name)" -NoNewline
        }

        # Run the process with timeout via background job
        $job = Start-Job -ScriptBlock {
            param($exePath, $scriptPath, $workDir, $stdoutFile, $stderrFile)
            Set-Location $workDir
            & $exePath "--input-script" $scriptPath > $stdoutFile 2> $stderrFile
            return $LASTEXITCODE
        } -ArgumentList $exe.FullPath, $smokeScriptPath, $exe.WorkDir, $stdout, $stderr

        $started = $true

        # Wait with 12-second timeout
        $completed = Wait-Job $job -Timeout 12

        if ($completed) {
            $exitCode = [string](Receive-Job $job)
            $status = 'exited'
        }
        else {
            Stop-Job $job
            $status = 'timeout'
            $exitCode = 'timeout'
        }

        Remove-Job $job -Force -ErrorAction SilentlyContinue
    }
    catch {
        $status = 'failed_to_start'
        $exitCode = 'n/a'
        $startError = $_.Exception.Message
    }

    $allLines = @()
    if (Test-Path $stdout) { $allLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { $allLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue }

    $keyLines = $allLines |
        Where-Object { $_ -match '(?i)(error|warn|warning|assert|validation|failed|exception|fatal)' } |
        Select-Object -First 20

    if ((-not $keyLines) -and $startError) {
        $keyLines = @($startError)
    }

    # Determine pass/fail
    $passed = ($exitCode -eq '0')

    if ($passed) {
        $passCount++
        if ($Verbose) {
            Write-Success "  PASSED"
        } else {
            Write-Success " PASSED"
        }
    } else {
        $failCount++
        if ($Verbose) {
            Write-Err "  FAILED (exit code: $exitCode)"
        } else {
            Write-Err " FAILED (exit code: $exitCode)"
        }

        if ($keyLines -and $Verbose) {
            Write-Err "  Error output:"
            foreach ($line in $keyLines) {
                Write-Err "    $line"
            }
        }
    }

    $results += [pscustomobject]@{
        exe      = $exe.Name
        category = $exe.Category
        script   = $smokeScript
        started  = $started
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

Write-Info ""
Write-Info "=========================================="
Write-Info "Smoke Test Summary"
Write-Info "=========================================="
Write-Info "Total: $totalRun (discovered: $($allExes.Count), skipped: $skipCount)"

# Per-category breakdown
foreach ($cat in @("Example", "Tool")) {
    $catResults = @($results | Where-Object { $_.category -eq $cat -and -not $_.skipped })
    if ($catResults.Count -gt 0) {
        $catPassed = @($catResults | Where-Object { $_.passed }).Count
        $catFailed = @($catResults | Where-Object { -not $_.passed }).Count
        $catSkipped = @($results | Where-Object { $_.category -eq $cat -and $_.skipped }).Count
        $catLine = "  ${cat}s: $($catResults.Count) run, $catPassed passed, $catFailed failed"
        if ($catSkipped -gt 0) { $catLine += ", $catSkipped skipped" }
        if ($catFailed -gt 0) {
            Write-Err $catLine
        } else {
            Write-Success $catLine
        }
    }
}

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
        Write-Err "  - [$($result.category)] $($result.exe) (exit code: $($result.exitCode))"
        if ($result.keyLines -and $Verbose) {
            foreach ($line in $result.keyLines) {
                Write-Err "      $line"
            }
        }
    }

    Write-Err ""
    Write-Err "=========================================="
    Write-Err "Smoke tests FAILED!"
    Write-Err "=========================================="
    exit 1
}

Write-Success ""
Write-Success "=========================================="
Write-Success "All smoke tests PASSED!"
Write-Success "=========================================="
exit 0
