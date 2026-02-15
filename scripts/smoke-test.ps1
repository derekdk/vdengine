# VDE Smoke Test Script
# Runs automated smoke tests on all example executables
# Usage: .\scripts\smoke-test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Build] [-Verbose]

param(
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

# Build if requested
if ($Build) {
    Write-Info "Building before running smoke tests..."
    & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Build failed! Cannot run smoke tests."
        exit 1
    }
}

# Locate examples directory
$examplesDir = $null
if ($Generator -eq "Ninja") {
    $examplesDir = Join-Path $buildDir "examples"
} else {
    $examplesDir = Join-Path $buildDir "examples\$Config"
}

if (-not (Test-Path $examplesDir)) {
    Write-Err "Examples directory not found: $examplesDir"
    Write-Err "Run with -Build flag to build first, or run .\scripts\build.ps1"
    exit 1
}

Write-Info "Examples directory: $examplesDir"
Write-Info ""

# Map of example executables to their specific smoke scripts
# If not listed here, uses smoke_quick.vdescript as fallback
$smokeScriptMap = @{
  'vde_physics_demo.exe' = 'smoke_physics_demo.vdescript'
  'vde_breakout_demo.exe' = 'smoke_breakout.vdescript'
  'vde_asteroids_demo.exe' = 'smoke_asteroids.vdescript'
  'vde_sprite_demo.exe' = 'smoke_sprite.vdescript'
  'vde_multi_scene_demo.exe' = 'smoke_multi_scene.vdescript'
  'vde_imgui_demo.exe' = 'smoke_imgui.vdescript'
  'vde_audio_demo.exe' = 'smoke_audio.vdescript'
  'vde_materials_lighting_demo.exe' = 'smoke_materials.vdescript'
  'vde_resource_demo.exe' = 'smoke_resource.vdescript'
}

# List of all example executables (excluding triangle which doesn't use Game API)
$apps = @(
  'vde_sidescroller.exe',
  'vde_simple_game_example.exe',
  'vde_sprite_demo.exe',
  'vde_wireframe_viewer.exe',
  'vde_world_bounds_demo.exe',
  'vde_physics_demo.exe',
  'vde_breakout_demo.exe',
  'vde_asteroids_demo.exe',
  'vde_asteroids_physics_demo.exe',
  'vde_multi_scene_demo.exe',
  'vde_imgui_demo.exe',
  'vde_audio_demo.exe',
  'vde_physics_audio_demo.exe',
  'vde_parallel_physics_demo.exe',
  'vde_four_scene_3d_demo.exe',
  'vde_quad_viewport_demo.exe',
  'vde_materials_lighting_demo.exe',
  'vde_resource_demo.exe',
  'vde_hex_prism_stacks_demo.exe',
  'vde_geometry_repl.exe'
)

# Default smoke script
$defaultSmoke = 'smoke_quick.vdescript'

# Resolve script base directory (relative to workspace root)
$scriptBaseDir = Join-Path $vdeRoot 'scripts\input'

$results = @()
$passCount = 0
$failCount = 0

# Clear VDE_INPUT_SCRIPT environment variable to avoid contamination
$env:VDE_INPUT_SCRIPT = $null

Write-Info "Running smoke tests..."
Write-Info "=========================================="

foreach ($app in $apps) {
  # Select smoke script for this example
  $smokeScript = $defaultSmoke
  if ($smokeScriptMap.ContainsKey($app)) {
    $smokeScript = $smokeScriptMap[$app]
  }
  
  $smokeScriptPath = Join-Path $scriptBaseDir $smokeScript
  
  if (-not (Test-Path $smokeScriptPath)) {
    Write-Warn "Smoke script not found: $smokeScript (skipping $app)"
    continue
  }
  
  $stdout = [IO.Path]::GetTempFileName()
  $stderr = [IO.Path]::GetTempFileName()
  $started = $false
  $status = ''
  $exitCode = ''
  $startError = $null

  try {
    $exePath = Join-Path $examplesDir $app
    
    if (-not (Test-Path $exePath)) {
      Write-Warn "Executable not found: $app (skipping)"
      continue
    }
    
    if ($Verbose) {
      Write-Info "Testing: $app with $smokeScript"
    } else {
      Write-Host "  Testing: $app" -NoNewline
    }
    
    # Use a job to run the process with timeout
    $job = Start-Job -ScriptBlock {
      param($exe, $script, $workDir, $stdoutFile, $stderrFile)
      Set-Location $workDir
      & $exe "--input-script" $script > $stdoutFile 2> $stderrFile
      return $LASTEXITCODE
    } -ArgumentList $exePath, $smokeScriptPath, $examplesDir, $stdout, $stderr
    
    $started = $true
    
    # Wait for job with timeout
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
      Write-Success "  ✓ PASSED"
    } else {
      Write-Success " ✓"
    }
  } else {
    $failCount++
    if ($Verbose) {
      Write-Err "  ✗ FAILED (exit code: $exitCode)"
    } else {
      Write-Err " ✗ (exit code: $exitCode)"
    }
    
    if ($keyLines -and $Verbose) {
      Write-Err "  Error output:"
      foreach ($line in $keyLines) {
        Write-Err "    $line"
      }
    }
  }

  $results += [pscustomobject]@{
    exe      = $app
    script   = $smokeScript
    started  = $started
    status   = $status
    exitCode = $exitCode
    passed   = $passed
    keyLines = @($keyLines)
  }
  
  # Clean up temp files
  if (Test-Path $stdout) { Remove-Item $stdout -ErrorAction SilentlyContinue }
  if (Test-Path $stderr) { Remove-Item $stderr -ErrorAction SilentlyContinue }
}

# Summary
Write-Info ""
Write-Info "=========================================="
Write-Info "Smoke Test Summary"
Write-Info "=========================================="
Write-Info "Total: $($passCount + $failCount)"
Write-Success "Passed: $passCount"
if ($failCount -gt 0) {
  Write-Err "Failed: $failCount"
}

if ($failCount -gt 0) {
  Write-Err ""
  Write-Err "Failed examples:"
  foreach ($result in $results | Where-Object { -not $_.passed }) {
    Write-Err "  - $($result.exe) (exit code: $($result.exitCode))"
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
