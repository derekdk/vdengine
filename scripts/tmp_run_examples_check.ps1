param([string]$WorkingDir)

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
  'vde_geometry_repl_example.exe'
)

# Default smoke script
$defaultSmoke = 'smoke_quick.vdescript'

# Resolve script base directory (relative to workspace root)
$scriptBaseDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\input'

$results = @()

# Clear VDE_INPUT_SCRIPT environment variable to avoid contamination
$env:VDE_INPUT_SCRIPT = $null

foreach ($app in $apps) {
  # Select smoke script for this example
  $smokeScript = $defaultSmoke
  if ($smokeScriptMap.ContainsKey($app)) {
    $smokeScript = $smokeScriptMap[$app]
  }
  
  $smokeScriptPath = Join-Path $scriptBaseDir $smokeScript
  
  $stdout = [IO.Path]::GetTempFileName()
  $stderr = [IO.Path]::GetTempFileName()
  $started = $false
  $status = ''
  $exitCode = ''
  $startError = $null

  try {
    $exePath = Join-Path $WorkingDir $app
    
    # Pass --input-script argument
    $proc = Start-Process -FilePath $exePath `
      -ArgumentList "--input-script", $smokeScriptPath `
      -WorkingDirectory $WorkingDir `
      -PassThru `
      -RedirectStandardOutput $stdout `
      -RedirectStandardError $stderr
    
    $started = $true

    if ($proc.WaitForExit(12000)) {
      $status = 'exited'
      $exitCode = [string]$proc.ExitCode
    }
    else {
      Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
      $status = 'killed'
      $exitCode = 'killed'
    }
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

  $results += [pscustomobject]@{
    exe      = $app
    script   = $smokeScript
    started  = $started
    status   = $status
    exitCode = $exitCode
    keyLines = @($keyLines)
  }
}

$results | ConvertTo-Json -Depth 5
