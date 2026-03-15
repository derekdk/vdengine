# VDE Problems-Only Output Helpers
# Dot-source this file after defining the following variables:
#   $ProblemsOnly    - switch indicating AI-friendly output mode
#   $failurePattern  - regex for lines that are failures
#   $warningPattern  - regex for lines that are warnings
#   $problemPattern  - combined regex (failures + warnings) used by Get-ProblemLines

function Write-Success {
    param([string]$msg)
    if ($ProblemsOnly) { return }
    Write-Host $msg -ForegroundColor Green
}

function Write-Info {
    param([string]$msg)
    if ($ProblemsOnly) { return }
    Write-Host $msg -ForegroundColor Cyan
}

function Write-Warn {
    param([string]$msg)
    if ($ProblemsOnly -and $msg -notmatch '^(WARNING|FAILURE|PASS):') {
        $msg = "WARNING: $msg"
    }
    Write-Host $msg -ForegroundColor Yellow
}

function Write-Err {
    param([string]$msg)
    if ($ProblemsOnly -and $msg -notmatch '^(WARNING|FAILURE|PASS):') {
        $msg = "FAILURE: $msg"
    }
    Write-Host $msg -ForegroundColor Red
}

function Write-Pass { param([string]$msg) Write-Host "PASS: $msg" -ForegroundColor Green }

function Get-ProblemLines {
    param(
        [string[]]$Lines,
        [int]$MaxLines = 40
    )

    if (-not $Lines) {
        return @()
    }

    $selected = New-Object System.Collections.Generic.List[string]
    $captureContext = 0

    foreach ($line in $Lines) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            $captureContext = 0
            continue
        }

        if ($line -match $problemPattern) {
            if (-not $selected.Contains($line)) {
                $selected.Add($line)
            }
            $captureContext = 2
            if ($selected.Count -ge $MaxLines) {
                break
            }
            continue
        }

        if ($captureContext -gt 0) {
            if (-not $selected.Contains($line)) {
                $selected.Add($line)
            }
            $captureContext--
            if ($selected.Count -ge $MaxLines) {
                break
            }
        }
    }

    return @($selected | Select-Object -First $MaxLines)
}

function Test-IsWarningLine {
    param([string]$Line)

    return ($Line -match $warningPattern) -and ($Line -notmatch $failurePattern)
}

function Write-ProblemLines {
    param(
        [string]$Prefix = '',
        [string[]]$Lines
    )

    foreach ($line in $Lines) {
        if (Test-IsWarningLine -Line $line) {
            Write-Warn "$Prefix$line"
        } else {
            Write-Err "$Prefix$line"
        }
    }
}

function Invoke-BuildForProblemsOnly {
    param(
        [string]$BuildScriptPath,
        [string]$SelectedGenerator,
        [string]$SelectedConfig
    )

    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()

    try {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $BuildScriptPath -Generator $SelectedGenerator -Config $SelectedConfig > $stdout 2> $stderr
        $buildExitCode = $LASTEXITCODE

        $buildLines = @()
        if (Test-Path $stdout) {
            $buildLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            $buildLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue
        }

        $problemLines = Get-ProblemLines -Lines $buildLines -MaxLines 40
        if ($problemLines.Count -eq 0 -and $buildExitCode -ne 0) {
            $problemLines = @($buildLines | Select-Object -Last 40)
        }

        Write-ProblemLines -Lines $problemLines

        if ($buildExitCode -ne 0) {
            Write-Err "FAILURE: Build failed with exit code $buildExitCode"
            exit 1
        }
    }
    finally {
        if (Test-Path $stdout) {
            Remove-Item $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            Remove-Item $stderr -ErrorAction SilentlyContinue
        }
    }
}
