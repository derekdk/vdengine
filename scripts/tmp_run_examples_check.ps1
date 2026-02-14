param([string]$WorkingDir)

$apps = @(
  'vde_sidescroller.exe',
  'vde_simple_game_example.exe',
  'vde_sprite_demo.exe',
  'vde_triangle_example.exe',
  'vde_wireframe_viewer.exe',
  'vde_world_bounds_demo.exe'
)

$results = @()

foreach ($app in $apps) {
  $stdout = [IO.Path]::GetTempFileName()
  $stderr = [IO.Path]::GetTempFileName()
  $started = $false
  $status = ''
  $exitCode = ''
  $startError = $null

  try {
    $proc = Start-Process -FilePath (Join-Path $WorkingDir $app) -WorkingDirectory $WorkingDir -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
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
    started  = $started
    status   = $status
    exitCode = $exitCode
    keyLines = @($keyLines)
  }
}

$results | ConvertTo-Json -Depth 5
