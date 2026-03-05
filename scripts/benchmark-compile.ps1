# VDE Compile Benchmark Script
# Performs a clean rebuild, captures per-TU wall-clock times from the Ninja log, and
# (optionally) captures MSVC front-end/back-end split via a second serial verbose pass.
# Results are saved as a JSON file in benchmarks/ for later comparison.
#
# Usage:
#   .\scripts\benchmark-compile.ps1 -Label "baseline-headers"
#   .\scripts\benchmark-compile.ps1 -Label "phase1-modules" -CaptureDetail
#
# Parameters:
#   -Label          Required. Short tag written into the JSON (e.g. "baseline-headers")
#   -Config         Debug (default) | Release
#   -CaptureDetail  Add a second serial build pass to capture MSVC /Bt FE/BE timing
#   -OutputDir      Directory to write reports into  (default: <root>/benchmarks)
#   -SkipConfigure  Skip cmake -DVDE_TIMING reconfigure (use when cache is already correct)

param(
    [Parameter(Mandatory=$true)]
    [string]$Label,

    [ValidateSet("Debug","Release")]
    [string]$Config = "Debug",

    [switch]$CaptureDetail = $false,

    [string]$OutputDir = "",

    [switch]$SkipConfigure = $false
)

$ErrorActionPreference = "Stop"

function Write-BenchInfo  { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-BenchOk    { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-BenchWarn  { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-BenchErr   { param([string]$msg) Write-Host $msg -ForegroundColor Red }

Write-BenchInfo "==========================================="
Write-BenchInfo "VDE Compile Benchmark"
Write-BenchInfo "==========================================="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot   = Split-Path -Parent $scriptDir
$buildDir  = Join-Path $vdeRoot "build_ninja"
$ninjaLog  = Join-Path $buildDir ".ninja_log"

if ($OutputDir -eq "") {
    $OutputDir = Join-Path $vdeRoot "benchmarks"
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-BenchInfo "Label    : $Label"
Write-BenchInfo "Config   : $Config"
Write-BenchInfo "BuildDir : $buildDir"
Write-BenchInfo "OutputDir: $OutputDir"

# ---------------------------------------------------------------------------
# 1. Ensure VS Developer environment is loaded (required for Ninja + MSVC)
# ---------------------------------------------------------------------------
if (-not $env:VSINSTALLDIR) {
    Write-BenchInfo "Loading Visual Studio Developer environment..."
    $vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    if (-not $vsPath) { Write-BenchErr "Visual Studio installation not found."; exit 1 }
    Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -Arch amd64
    Write-BenchOk "VS Developer environment loaded (x64)"
} else {
    Write-BenchOk "VS Developer environment already active"
}

# ---------------------------------------------------------------------------
# 2. (Re-)configure with VDE_TIMING=ON so /Bt is wired in
# ---------------------------------------------------------------------------
if (-not $SkipConfigure) {
    Write-BenchInfo ""
    Write-BenchInfo "Configuring CMake with VDE_TIMING=ON ..."
    cmake -S $vdeRoot -B $buildDir -G Ninja `
          "-DCMAKE_BUILD_TYPE=$Config" `
          -DVDE_TIMING=ON
    if ($LASTEXITCODE -ne 0) { Write-BenchErr "CMake configure failed."; exit 1 }
    Write-BenchOk "Configure complete."
} else {
    Write-BenchInfo "Skipping configure (-SkipConfigure)."
}

# ---------------------------------------------------------------------------
# 3. Record ninja log line-count before the build (to isolate this run)
# ---------------------------------------------------------------------------
$logLinesBefore = 0
if (Test-Path $ninjaLog) {
    $logLinesBefore = (Get-Content $ninjaLog).Count
}

# ---------------------------------------------------------------------------
# 4. Clean build (parallel) -- this is the primary wall-clock measurement
# ---------------------------------------------------------------------------
Write-BenchInfo ""
Write-BenchInfo "Starting clean rebuild (parallel) ..."
$buildStart = [System.DateTimeOffset]::UtcNow

cmake --build $buildDir --clean-first --parallel

if ($LASTEXITCODE -ne 0) { Write-BenchErr "Build failed."; exit 1 }

$buildEnd = [System.DateTimeOffset]::UtcNow
$totalWallSec = [math]::Round(($buildEnd - $buildStart).TotalSeconds, 2)
Write-BenchOk "Clean build finished in ${totalWallSec}s."

# ---------------------------------------------------------------------------
# 5. Parse ninja log -- only entries added by this build run
# ---------------------------------------------------------------------------
Write-BenchInfo ""
Write-BenchInfo "Parsing Ninja log ..."

function Read-NinjaLog {
    param([string]$LogPath, [int]$SkipLines = 0)

    $entries = @{}
    $lines   = Get-Content $LogPath
    $idx     = 0
    foreach ($line in $lines) {
        $idx++
        if ($idx -le $SkipLines) { continue }
        if ($line.StartsWith('#') -or [string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t"
        if ($parts.Length -lt 4) { continue }
        $startMs = [int]$parts[0]
        $endMs   = [int]$parts[1]
        $out     = $parts[3].Trim()
        $entries[$out] = [pscustomobject]@{
            start_ms  = $startMs
            end_ms    = $endMs
            wall_ms   = $endMs - $startMs
            output    = $out
        }
    }
    return $entries
}

$newEntries = Read-NinjaLog -LogPath $ninjaLog -SkipLines $logLinesBefore
Write-BenchInfo "  Recorded $($newEntries.Count) compiled objects."

# ---------------------------------------------------------------------------
# 6. Optional: serial verbose pass to capture MSVC /Bt FE/BE split
# ---------------------------------------------------------------------------
$detailMap = @{}

if ($CaptureDetail) {
    Write-BenchInfo ""
    Write-BenchWarn "CaptureDetail: running serial verbose rebuild to capture MSVC /Bt ..."
    Write-BenchWarn "(This will take longer than the parallel build -- results are for FE/BE ratio only)"

    $detailLog = Join-Path $buildDir "benchmark_detail_capture.txt"
    # Run serial + verbose; capture stderr (where /Bt writes) together with stdout
    $ninjaExe = "ninja"
    Push-Location $buildDir
    try {
        & $ninjaExe -j 1 -v 2>&1 | Out-File -Encoding utf8 $detailLog
    } finally {
        Pop-Location
    }

    # Parse lines like:  time(c:\...\File.cpp)=4.716s (3.800s CPU front end, 0.916s CPU back end)
    $btPattern = [regex]'time\((?<file>[^)]+)\)=(?<total>[\d.]+)s\s+\((?<fe>[\d.]+)s\s+CPU front end,\s+(?<be>[\d.]+)s\s+CPU back end\)'
    foreach ($line in (Get-Content $detailLog)) {
        $m = $btPattern.Match($line)
        if ($m.Success) {
            $file  = $m.Groups['file'].Value -replace '\\','/' # normalise separators
            $total = [double]$m.Groups['total'].Value
            $fe    = [double]$m.Groups['fe'].Value
            $be    = [double]$m.Groups['be'].Value
            $detailMap[$file] = [pscustomobject]@{
                total_cpu_s      = $total
                frontend_cpu_s   = $fe
                backend_cpu_s    = $be
                frontend_pct     = if ($total -gt 0) { [math]::Round($fe / $total * 100, 1) } else { 0 }
            }
        }
    }
    Write-BenchOk "  Parsed $($detailMap.Count) /Bt timing entries."
}

# ---------------------------------------------------------------------------
# 7. Classify entries by target (vde, tests, examples, tools, deps)
# ---------------------------------------------------------------------------
function Get-Target([string]$OutputPath) {
    if ($OutputPath -like 'CMakeFiles/vde.dir/*')           { return 'vde' }
    if ($OutputPath -like 'tests/CMakeFiles/*')              { return 'tests' }
    if ($OutputPath -like 'examples/CMakeFiles/*')           { return 'examples' }
    if ($OutputPath -like 'tools/CMakeFiles/*' -or
        $OutputPath -like 'tools/*/CMakeFiles/*')            { return 'tools' }
    if ($OutputPath -like '_deps/*')                         { return 'deps' }
    return 'other'
}

$byTarget = @{}
$allFiles = [System.Collections.Generic.List[object]]::new()

# Snapshot values into an array so the hashtable is never iterated live
$entryList = @($newEntries.Values)

foreach ($e in $entryList) {
    $target = Get-Target $e.output

    # Strip build dir prefix to get project-relative path
    $displayPath = $e.output -replace '^CMakeFiles/vde\.dir/', 'src/'
    $displayPath = $displayPath -replace '\.obj$',''
    $displayPath = $displayPath -replace '\.o$',''

    # Try to match a /Bt detail entry by searching for path substring
    $detail = $null
    if ($CaptureDetail -and $detailMap.Count -gt 0) {
        foreach ($dk in $detailMap.Keys) {
            if ($dk -like "*$($displayPath -replace '/','*')*") {
                $detail = $detailMap[$dk]
                break
            }
        }
    }

    $fileEntry = [pscustomobject]@{
        output           = $e.output
        display_path     = $displayPath
        target           = $target
        wall_ms          = $e.wall_ms
    }
    if ($null -ne $detail) {
        $fileEntry | Add-Member -NotePropertyName frontend_cpu_s  -NotePropertyValue $detail.frontend_cpu_s
        $fileEntry | Add-Member -NotePropertyName backend_cpu_s   -NotePropertyValue $detail.backend_cpu_s
        $fileEntry | Add-Member -NotePropertyName frontend_pct    -NotePropertyValue $detail.frontend_pct
    }

    $allFiles.Add($fileEntry)

    if (-not $byTarget.ContainsKey($target)) { $byTarget[$target] = [System.Collections.Generic.List[object]]::new() }
    $byTarget[$target].Add($fileEntry)
}

# Sort each target's files by wall_ms descending
$targetKeys = @($byTarget.Keys)
foreach ($t in $targetKeys) {
    $byTarget[$t] = @($byTarget[$t] | Sort-Object wall_ms -Descending)
}

# ---------------------------------------------------------------------------
# 8. Compute per-target summaries
# ---------------------------------------------------------------------------
function Get-Summary([object[]]$Files) {
    if ($Files.Count -eq 0) { return $null }
    $totalMs = ($Files | Measure-Object wall_ms -Sum).Sum
    $avgMs   = [math]::Round($totalMs / $Files.Count, 1)
    $maxMs   = ($Files | Measure-Object wall_ms -Maximum).Maximum
    $p95Ms   = ($Files | Sort-Object wall_ms -Descending | Select-Object -First ([math]::Max(1,[int][math]::Ceiling($Files.Count*0.05))) | Measure-Object wall_ms -Average).Average
    return [pscustomobject]@{
        file_count      = $Files.Count
        total_wall_ms   = $totalMs
        avg_wall_ms     = $avgMs
        max_wall_ms     = $maxMs
        p95_wall_ms     = [math]::Round($p95Ms, 1)
    }
}

$targetSummaries = @{}
foreach ($t in $byTarget.Keys) {
    $targetSummaries[$t] = Get-Summary @($byTarget[$t])
}

$overallSummary = Get-Summary @($allFiles)

# ---------------------------------------------------------------------------
# 9. Build the report object and serialise to JSON
# ---------------------------------------------------------------------------
$timestamp = [System.DateTimeOffset]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")
$dateSlug  = [System.DateTimeOffset]::UtcNow.ToString("yyyyMMdd-HHmmss")
$safeLabel = $Label -replace '[^A-Za-z0-9_\-]','-'
$reportFile = Join-Path $OutputDir "${dateSlug}-${safeLabel}.json"

# Compute compiler info
$clExeCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
$clExe = if ($clExeCmd) { $clExeCmd.Source } else { "cl.exe" }

$report = [ordered]@{
    schema_version      = 1
    label               = $Label
    timestamp           = $timestamp
    config              = $Config
    generator           = "Ninja"
    detail_captured     = $CaptureDetail.IsPresent
    build_wall_sec      = $totalWallSec
    compiler            = $clExe
    summary             = $overallSummary
    by_target           = $targetSummaries
    files               = @($allFiles | Select-Object output, display_path, target, wall_ms,
                              @{n='frontend_cpu_s';e={$_.frontend_cpu_s}},
                              @{n='backend_cpu_s' ;e={$_.backend_cpu_s }},
                              @{n='frontend_pct'  ;e={$_.frontend_pct  }})
}

$report | ConvertTo-Json -Depth 10 | Out-File -Encoding utf8 $reportFile

# ---------------------------------------------------------------------------
# 10. Print console summary
# ---------------------------------------------------------------------------
Write-BenchInfo ""
Write-BenchInfo "==========================================="
Write-BenchOk   "Benchmark complete"
Write-BenchInfo "  Label         : $Label"
Write-BenchInfo ("  Total objects : {0}" -f $overallSummary.file_count)
Write-BenchInfo ("  Total wall    : {0:N0} ms  ({1} s wall-clock for whole build)" -f $overallSummary.total_wall_ms, $totalWallSec)
Write-BenchInfo ("  Avg per-TU    : {0:N0} ms" -f $overallSummary.avg_wall_ms)
Write-BenchInfo ("  Max single-TU : {0:N0} ms" -f $overallSummary.max_wall_ms)
Write-BenchInfo ""
Write-BenchInfo "  Per-target breakdown:"

$targetOrder = @('vde','examples','tools','tests','deps','other')
foreach ($t in $targetOrder) {
    if ($targetSummaries.ContainsKey($t)) {
        $s = $targetSummaries[$t]
        Write-BenchInfo ("    {0,-10}  {1,3} files  total {2,6:N0} ms  avg {3,5:N0} ms  max {4,5:N0} ms" -f
            $t, $s.file_count, $s.total_wall_ms, $s.avg_wall_ms, $s.max_wall_ms)
    }
}

Write-BenchInfo ""
Write-BenchInfo "  Top 10 slowest TUs (wall ms):"
$top10 = @($allFiles | Sort-Object wall_ms -Descending | Select-Object -First 10)
foreach ($f in $top10) {
    $detail = if ($null -ne $f.frontend_pct) { "  (FE {0}%)" -f $f.frontend_pct } else { "" }
    Write-BenchInfo ("    {0,6:N0} ms  {1}{2}" -f $f.wall_ms, $f.display_path, $detail)
}

Write-BenchInfo ""
Write-BenchOk   "Report saved to:"
Write-BenchOk   "  $reportFile"
Write-BenchInfo "==========================================="
