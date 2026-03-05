# VDE Compile Benchmark Comparison Script
# Compares two benchmark JSON files produced by benchmark-compile.ps1 and reports
# per-TU improvements, regressions, and aggregate totals.
#
# Usage:
#   .\scripts\compare-benchmarks.ps1 -Baseline benchmarks/20260304-120000-baseline.json `
#                                    -Candidate benchmarks/20260304-140000-phase1-modules.json
#   .\scripts\compare-benchmarks.ps1 -Baseline baseline -Candidate phase1-modules
#                                    (resolves by matching label substring in benchmarks/)

param(
    [Parameter(Mandatory=$true)]
    [string]$Baseline,

    [Parameter(Mandatory=$true)]
    [string]$Candidate,

    [string]$BenchmarksDir = "",

    # Minimum absolute delta (ms) to include in the per-TU table
    [int]$MinDeltaMs = 100,

    # Write a markdown comparison report alongside the JSON files
    [switch]$Markdown = $false
)

$ErrorActionPreference = "Stop"

function Write-CmpInfo   { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-CmpOk     { param([string]$msg) Write-Host $msg -ForegroundColor Green }
function Write-CmpWarn   { param([string]$msg) Write-Host $msg -ForegroundColor Yellow }
function Write-CmpErr    { param([string]$msg) Write-Host $msg -ForegroundColor Red }
function Write-CmpNeutral{ param([string]$msg) Write-Host $msg }

$scriptDir     = Split-Path -Parent $MyInvocation.MyCommand.Path
$vdeRoot       = Split-Path -Parent $scriptDir
if ($BenchmarksDir -eq "") { $BenchmarksDir = Join-Path $vdeRoot "benchmarks" }

# ---------------------------------------------------------------------------
# Helper: resolve path or label substring -> full JSON path
# ---------------------------------------------------------------------------
function Resolve-ReportPath([string]$QueryStr) {
    if (Test-Path $QueryStr) { return $QueryStr }
    # Try as basename
    $fullPath = Join-Path $BenchmarksDir $QueryStr
    if (Test-Path $fullPath) { return $fullPath }
    # Try label substring match in benchmarks dir
    $foundFiles = Get-ChildItem $BenchmarksDir -Filter "*.json" |
               Where-Object { $_.Name -like "*$QueryStr*" } |
               Sort-Object LastWriteTime -Descending
    if ($foundFiles.Count -gt 0) { return $foundFiles[0].FullName }
    throw "Cannot find benchmark report matching '$QueryStr'. Run benchmark-compile.ps1 first."
}

$baselinePath  = Resolve-ReportPath -QueryStr $Baseline
$candidatePath = Resolve-ReportPath -QueryStr $Candidate

Write-CmpInfo "==========================================="
Write-CmpInfo "VDE Benchmark Comparison"
Write-CmpInfo "==========================================="
Write-CmpInfo "Baseline  : $baselinePath"
Write-CmpInfo "Candidate : $candidatePath"

$baseJson = Get-Content $baselinePath  -Raw | ConvertFrom-Json
$candJson = Get-Content $candidatePath -Raw | ConvertFrom-Json

Write-CmpInfo ""
Write-CmpInfo ("Baseline  label: {0}  ({1}  config:{2})" -f $baseJson.label, $baseJson.timestamp, $baseJson.config)
Write-CmpInfo ("Candidate label: {0}  ({1}  config:{2})" -f $candJson.label, $candJson.timestamp, $candJson.config)

# ---------------------------------------------------------------------------
# Build lookup: display_path -> entry for each report
# ---------------------------------------------------------------------------
function New-LookupMap([object[]]$Files) {
    $map = @{}
    foreach ($f in $Files) {
        $key = $f.display_path
        if (-not $map.ContainsKey($key)) { $map[$key] = $f }
    }
    return $map
}

$baseLookup = New-LookupMap @($baseJson.files)
$candLookup = New-LookupMap @($candJson.files)

# Merge all keys
$allKeys = [System.Collections.Generic.HashSet[string]]::new()
foreach ($k in $baseLookup.Keys) { [void]$allKeys.Add($k) }
foreach ($k in $candLookup.Keys) { [void]$allKeys.Add($k) }

# ---------------------------------------------------------------------------
# Compute delta rows
# ---------------------------------------------------------------------------
$rows = [System.Collections.Generic.List[object]]::new()

foreach ($key in $allKeys) {
    $inBase = $baseLookup.ContainsKey($key)
    $inCand = $candLookup.ContainsKey($key)

    $baseMs = if ($inBase) { [int]$baseLookup[$key].wall_ms } else { $null }
    $candMs = if ($inCand) { [int]$candLookup[$key].wall_ms } else { $null }
    $target = if ($inBase) { $baseLookup[$key].target } elseif ($inCand) { $candLookup[$key].target } else { 'other' }

    $deltaMs  = $null
    $deltaPct = $null
    $status   = 'unchanged'

    if ($null -ne $baseMs -and $null -ne $candMs) {
        $deltaMs  = $candMs - $baseMs
        $deltaPct = if ($baseMs -gt 0) { [math]::Round($deltaMs / $baseMs * 100, 1) } else { 0 }
        if    ($deltaMs -lt -$MinDeltaMs)  { $status = 'faster' }
        elseif($deltaMs -gt  $MinDeltaMs)  { $status = 'slower' }
        else                               { $status = 'unchanged' }
    } elseif ($null -eq $baseMs) {
        $status = 'new'
    } else {
        $status = 'removed'
    }

    $rows.Add([pscustomobject]@{
        display_path = $key
        target       = $target
        base_ms      = $baseMs
        cand_ms      = $candMs
        delta_ms     = $deltaMs
        delta_pct    = $deltaPct
        status       = $status
    }) | Out-Null
}

# ---------------------------------------------------------------------------
# Aggregate totals (only files present in both)
# ---------------------------------------------------------------------------
$bothRows    = @($rows | Where-Object { $null -ne $_.base_ms -and $null -ne $_.cand_ms })
$fasterRows  = @($rows | Where-Object { $_.status -eq 'faster' } | Sort-Object delta_ms)
$slowerRows  = @($rows | Where-Object { $_.status -eq 'slower' } | Sort-Object delta_ms -Descending)
$newRows     = @($rows | Where-Object { $_.status -eq 'new'  })
$removedRows = @($rows | Where-Object { $_.status -eq 'removed' })

$baseTotalMs  = ($bothRows | Measure-Object base_ms -Sum).Sum
$candTotalMs  = ($bothRows | Measure-Object cand_ms -Sum).Sum
$totalDeltaMs = $candTotalMs - $baseTotalMs
$totalDeltaPct = if ($baseTotalMs -gt 0) { [math]::Round($totalDeltaMs / $baseTotalMs * 100, 1) } else { 0 }

$buildDeltaSec = [math]::Round($candJson.build_wall_sec - $baseJson.build_wall_sec, 2)

# Per-target breakdown
function Get-TargetRows([object[]]$All, [string]$Target) {
    $tRows = @($All | Where-Object { $_.target -eq $Target })
    if ($tRows.Count -eq 0) { return $null }
    $bSum = ($tRows | Where-Object { $null -ne $_.base_ms } | Measure-Object base_ms -Sum).Sum
    $cSum = ($tRows | Where-Object { $null -ne $_.cand_ms } | Measure-Object cand_ms -Sum).Sum
    $dMs  = $cSum - $bSum
    $dPct = if ($bSum -gt 0) { [math]::Round($dMs / $bSum * 100, 1) } else { 0 }
    return [pscustomobject]@{ target=$Target; base_total_ms=$bSum; cand_total_ms=$cSum; delta_ms=$dMs; delta_pct=$dPct }
}

# ---------------------------------------------------------------------------
# Console output
# ---------------------------------------------------------------------------
Write-CmpInfo ""
Write-CmpInfo "--- Overall ---"
$wallLine = ("  Build wall-clock : {0:N1}s -> {1:N1}s  (D {2:N1}s)" -f
    $baseJson.build_wall_sec, $candJson.build_wall_sec, $buildDeltaSec)
if ($buildDeltaSec -lt 0) { Write-CmpOk $wallLine } elseif ($buildDeltaSec -gt 0) { Write-CmpWarn $wallLine } else { Write-CmpNeutral $wallLine }

$sumLine  = ("  Sum per-TU wall  : {0:N0} ms -> {1:N0} ms  (D {2:N0} ms  {3}%)" -f
    $baseTotalMs, $candTotalMs, $totalDeltaMs, $totalDeltaPct)
if ($totalDeltaMs -lt 0) { Write-CmpOk $sumLine } elseif ($totalDeltaMs -gt 0) { Write-CmpWarn $sumLine } else { Write-CmpNeutral $sumLine }

Write-CmpInfo ""
Write-CmpInfo "--- Per-target totals ---"
$targetOrder = @('vde','examples','tools','tests','deps','other')
foreach ($t in $targetOrder) {
    $tr = Get-TargetRows @($rows) $t
    if ($null -eq $tr) { continue }
    $dSign  = if ($tr.delta_ms -gt 0) { "+" } else { "" }
    $line   = ("  {0,-10}  {1,7:N0} ms -> {2,7:N0} ms  ({3}{4:N0} ms  {5}{6}%)" -f
        $tr.target, $tr.base_total_ms, $tr.cand_total_ms, $dSign, $tr.delta_ms, $dSign, $tr.delta_pct)
    if ($tr.delta_ms -lt 0) { Write-CmpOk $line } elseif ($tr.delta_ms -gt 0) { Write-CmpWarn $line } else { Write-CmpNeutral $line }
}

if ($fasterRows.Count -gt 0) {
    Write-CmpInfo ""
    Write-CmpOk ("--- Faster TUs (D <= -{0} ms, sorted by improvement) ---" -f $MinDeltaMs)
    foreach ($r in $fasterRows) {
        Write-CmpOk ("  {0,7:N0} ms -> {1,7:N0} ms  D {2,7:N0} ms  ({3}%)  {4}" -f
            $r.base_ms, $r.cand_ms, $r.delta_ms, $r.delta_pct, $r.display_path)
    }
}

if ($slowerRows.Count -gt 0) {
    Write-CmpInfo ""
    Write-CmpWarn ("--- Slower TUs (D >= +{0} ms, sorted by regression) ---" -f $MinDeltaMs)
    foreach ($r in $slowerRows) {
        Write-CmpWarn ("  {0,7:N0} ms -> {1,7:N0} ms  D +{2,7:N0} ms  (+{3}%)  {4}" -f
            $r.base_ms, $r.cand_ms, $r.delta_ms, $r.delta_pct, $r.display_path)
    }
}

if ($newRows.Count -gt 0) {
    Write-CmpInfo ""
    Write-CmpInfo "--- New TUs (appeared in candidate only) ---"
    foreach ($r in $newRows) {
        Write-CmpInfo ("  {0,7:N0} ms  {1}" -f $r.cand_ms, $r.display_path)
    }
}

if ($removedRows.Count -gt 0) {
    Write-CmpInfo ""
    Write-CmpInfo "--- Removed TUs (present in baseline only) ---"
    foreach ($r in $removedRows) {
        Write-CmpInfo ("  {0,7:N0} ms  {1}" -f $r.base_ms, $r.display_path)
    }
}

# ---------------------------------------------------------------------------
# Optional Markdown report
# ---------------------------------------------------------------------------
if ($Markdown) {
    $mdSlug    = [System.DateTimeOffset]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $mdFile    = Join-Path $BenchmarksDir ("cmp-{0}-vs-{1}-{2}.md" -f
                    ($baseJson.label -replace '[^A-Za-z0-9_]','-'),
                    ($candJson.label -replace '[^A-Za-z0-9_]','-'),
                    $mdSlug)

    $md = [System.Text.StringBuilder]::new()
    [void]$md.AppendLine("# VDE Compile Benchmark Comparison")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("| | Baseline | Candidate |")
    [void]$md.AppendLine("|---|---|---|")
    [void]$md.AppendLine("| **Label** | $($baseJson.label) | $($candJson.label) |")
    [void]$md.AppendLine("| **Timestamp** | $($baseJson.timestamp) | $($candJson.timestamp) |")
    [void]$md.AppendLine("| **Config** | $($baseJson.config) | $($candJson.config) |")
    [void]$md.AppendLine("| **Build wall (s)** | $($baseJson.build_wall_sec) | $($candJson.build_wall_sec) |")
    [void]$md.AppendLine("| **Sum per-TU wall (ms)** | $baseTotalMs | $candTotalMs |")
    $dSign = if ($totalDeltaMs -lt 0) { "" } else { "+" }
    [void]$md.AppendLine("| **D per-TU total (ms)** | | $dSign$totalDeltaMs ($dSign$totalDeltaPct%) |")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("## Per-target breakdown")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("| Target | Baseline (ms) | Candidate (ms) | D (ms) | D% |")
    [void]$md.AppendLine("|---|---|---|---|---|")
    foreach ($t in $targetOrder) {
        $tr = Get-TargetRows @($rows) $t
        if ($null -eq $tr) { continue }
        $dsign2 = if ($tr.delta_ms -gt 0) { "+" } else { "" }
        [void]$md.AppendLine("| $($tr.target) | $($tr.base_total_ms) | $($tr.cand_total_ms) | $dsign2$($tr.delta_ms) | $dsign2$($tr.delta_pct)% |")
    }
    [void]$md.AppendLine("")
    [void]$md.AppendLine("## Changed TUs (|D| >= $MinDeltaMs ms)")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("| File | Baseline (ms) | Candidate (ms) | D (ms) | D% | Status |")
    [void]$md.AppendLine("|---|---|---|---|---|---|")
    $changedRows = @($rows | Where-Object { $_.status -in @('faster','slower') } | Sort-Object delta_ms)
    foreach ($r in $changedRows) {
        $dsign3 = if ($r.delta_ms -gt 0) { "+" } else { "" }
        [void]$md.AppendLine("| $($r.display_path) | $($r.base_ms) | $($r.cand_ms) | $dsign3$($r.delta_ms) | $dsign3$($r.delta_pct)% | $($r.status) |")
    }
    [void]$md.AppendLine("")
    $md.ToString() | Out-File -Encoding utf8 $mdFile
    Write-CmpInfo ""
    Write-CmpOk "Markdown report saved to:"
    Write-CmpOk "  $mdFile"
}

# ---------------------------------------------------------------------------
# Final summary line
# ---------------------------------------------------------------------------
Write-CmpInfo ""
$verdict = if ($totalDeltaMs -lt 0) { "FASTER by {0:N0} ms ({1}%) vs baseline" -f [math]::Abs($totalDeltaMs), [math]::Abs($totalDeltaPct) }
           elseif ($totalDeltaMs -gt 0) { "SLOWER by {0:N0} ms (+{1}%) vs baseline" -f $totalDeltaMs, $totalDeltaPct }
           else { "No meaningful change vs baseline" }
Write-CmpInfo "==========================================="
if ($totalDeltaMs -lt 0) { Write-CmpOk   "Verdict: Candidate is $verdict" }
elseif ($totalDeltaMs -gt 0) { Write-CmpWarn "Verdict: Candidate is $verdict" }
else                          { Write-CmpNeutral "Verdict: $verdict" }
Write-CmpInfo "==========================================="
