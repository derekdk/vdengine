# VDE Show Log Script
# Shows verification log contents.  Defaults to the latest verify log.
#
# Usage:
#   .\scripts\show-log.ps1                   # Show full verify-latest.log
#   .\scripts\show-log.ps1 -Tail 50          # Show last 50 lines
#   .\scripts\show-log.ps1 -Head 30          # Show first 30 lines
#   .\scripts\show-log.ps1 -List             # List all available logs
#   .\scripts\show-log.ps1 -Path <file>      # Show a specific log file
#
# AI agents: prefer read_file on logs/verify-latest.log directly as it
# avoids terminal output truncation.  Use this script for quick checks.

param(
    # Show last N lines (0 = show all)
    [int]$Tail = 0,

    # Show first N lines (0 = show all)
    [int]$Head = 0,

    # List available log files instead of showing content
    [switch]$List,

    # Path to a specific log file (default: logs/verify-latest.log)
    [string]$Path = ""
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$logsDir   = Join-Path $repoRoot "logs"

if ($List) {
    if (-not (Test-Path $logsDir)) {
        Write-Host "No logs directory found.  Run .\scripts\verify.ps1 first." -ForegroundColor Yellow
        exit 0
    }
    $logFiles = Get-ChildItem -Path $logsDir -Filter "*.log" | Sort-Object Name
    if ($logFiles.Count -eq 0) {
        Write-Host "No log files found in $logsDir" -ForegroundColor Yellow
    } else {
        Write-Host "Available logs in $logsDir" -ForegroundColor Cyan
        foreach ($f in $logFiles) {
            $sizeKb = [int]($f.Length / 1024)
            Write-Host "  $($f.Name)  (${sizeKb} KB)" -ForegroundColor White
        }
    }
    exit 0
}

$logFile = $Path
if (-not $logFile) {
    $logFile = Join-Path $logsDir "verify-latest.log"
}

if (-not (Test-Path $logFile)) {
    Write-Host "Log file not found: $logFile" -ForegroundColor Yellow
    Write-Host "Run .\scripts\verify.ps1 first to generate a log." -ForegroundColor Yellow
    exit 1
}

if ($Tail -gt 0) {
    Get-Content -Path $logFile -Tail $Tail
} elseif ($Head -gt 0) {
    Get-Content -Path $logFile -TotalCount $Head
} else {
    Get-Content -Path $logFile
}
