# VDE Problems-Only Output Helpers
# Dot-source this file after defining the following variables:
#   $ProblemsOnly    - switch indicating AI-friendly output mode
#   $ShowWarningsInProblemsOnly - set true only when callers explicitly want warning lines in quiet mode
#   $failurePattern  - regex for lines that are failures
#   $warningPattern  - regex for lines that are warnings
#   $problemPattern  - combined regex (failures + warnings) used by Get-ProblemLines
#
# Quiet-mode contract:
#   default (no flags)  -> PASS / FAILURE only
#   -ProblemsOnly       -> PASS / WARNING / FAILURE
#   -Verbose            -> detailed tool output

$ignoredExternalLayerPattern = '(?i)(GalaxyOverlayVkLayer(?:_VERBOSE|_DEBUG)?|VK_LAYER_NV_optimus|VK_LAYER_NV_present|VK_LAYER_OBS_HOOK|VK_LAYER_VALVE_steam_fossilize|VK_LAYER_VALVE_steam_overlay)'
$ignoredProblemPattern = '(?i)(duplicated_message_limit value|Policy #LLP_LAYER_3|CategoryInfo.*LLP_LAYER_3)'

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
    if ($ProblemsOnly -and -not $ShowWarningsInProblemsOnly) {
        return
    }
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

function Resolve-ProblemsOnlyPreference {
    param(
        [hashtable]$BoundParameters,
        [bool]$VerboseRequested
    )

    if ($VerboseRequested) {
        return $false
    }

    if ($BoundParameters.ContainsKey('ProblemsOnly')) {
        $boundValue = $BoundParameters['ProblemsOnly']
        if ($boundValue -is [System.Management.Automation.SwitchParameter]) {
            return $boundValue.IsPresent
        }

        return [bool]$boundValue
    }

    return $true
}

function Resolve-ProblemsOnlyWarningPreference {
    param([hashtable]$BoundParameters)

    if (-not $BoundParameters.ContainsKey('ProblemsOnly')) {
        return $false
    }

    $boundValue = $BoundParameters['ProblemsOnly']
    if ($boundValue -is [System.Management.Automation.SwitchParameter]) {
        return $boundValue.IsPresent
    }

    return [bool]$boundValue
}

function Invoke-CommandForProblemsOnly {
    param(
        [scriptblock]$Command,
        [int]$MaxLines = 40
    )

    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()

    try {
        & $Command > $stdout 2> $stderr
        $exitCode = $LASTEXITCODE

        $allLines = @()
        if (Test-Path $stdout) {
            $allLines += Get-Content -Path $stdout -ErrorAction SilentlyContinue
        }
        if (Test-Path $stderr) {
            $allLines += Get-Content -Path $stderr -ErrorAction SilentlyContinue
        }

        $problemLines = Get-ProblemLines -Lines $allLines -MaxLines $MaxLines
        if ($problemLines.Count -eq 0 -and $exitCode -ne 0) {
            $problemLines = @($allLines | Select-Object -Last $MaxLines)
        }

        if ($problemLines.Count -gt 0) {
            Write-ProblemLines -Lines $problemLines
        }

        return [pscustomobject]@{
            ExitCode     = $exitCode
            Lines        = @($allLines)
            ProblemLines = @($problemLines)
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

function Get-ProblemLines {
    param(
        [string[]]$Lines,
        [int]$MaxLines = 40,
        [int]$ContextLines = 0
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

        if (($line -match $ignoredProblemPattern) -or ($line -match $ignoredExternalLayerPattern)) {
            continue
        }

        if ($line -match $problemPattern) {
            if (-not $selected.Contains($line)) {
                $selected.Add($line)
            }
            $captureContext = $ContextLines
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

function Write-CompactWarnings {
    param(
        [string]$Prefix = '',
        [string[]]$Lines,
        [int]$MaxLines = 4
    )

    $warningLines = @($Lines | Where-Object { (Test-IsWarningLine -Line $_) -and ($_ -notmatch $ignoredProblemPattern) -and ($_ -notmatch $ignoredExternalLayerPattern) } | Select-Object -Unique)
    $displayLines = @($warningLines | Select-Object -First $MaxLines)

    if ($displayLines.Count -gt 0) {
        Write-ProblemLines -Prefix $Prefix -Lines $displayLines
    }

    if ($warningLines.Count -gt $displayLines.Count) {
        $suppressedCount = $warningLines.Count - $displayLines.Count
        Write-Warn "$Prefix$suppressedCount additional warning line(s) suppressed"
    }
}

function Write-WarningSummary {
    param(
        [string]$Prefix = '',
        [string[]]$Lines
    )

    $warningLines = @($Lines | Where-Object { (Test-IsWarningLine -Line $_) -and ($_ -notmatch $ignoredProblemPattern) -and ($_ -notmatch $ignoredExternalLayerPattern) } | Select-Object -Unique)
    if ($warningLines.Count -eq 0) {
        return
    }

    $summary = $warningLines[0]
    if ($warningLines.Count -gt 1) {
        $summary = "$summary ($($warningLines.Count) unique warning line(s))"
    }

    Write-Warn "$Prefix$summary"
}

function Invoke-ScriptWithMode {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments = @(),
        [switch]$VerboseOutput
    )

    $shell = if (Get-Command powershell.exe -ErrorAction SilentlyContinue) {
        'powershell.exe'
    } elseif (Get-Command powershell -ErrorAction SilentlyContinue) {
        'powershell'
    } else {
        'pwsh'
    }

    $scriptArgs = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-NonInteractive',
        '-File', $ScriptPath
    ) + $Arguments

    if ($VerboseOutput) {
        $scriptArgs += '-Verbose'
    } else {
        $scriptArgs += '-ProblemsOnly'
    }

    & $shell @scriptArgs | Out-Host
    return $LASTEXITCODE
}

function Invoke-BuildForProblemsOnly {
    param(
        [string]$BuildScriptPath,
        [string]$SelectedGenerator,
        [string]$SelectedConfig
    )

    $buildExitCode = Invoke-ScriptWithMode -ScriptPath $BuildScriptPath -Arguments @('-Generator', $SelectedGenerator, '-Config', $SelectedConfig)
    if ($buildExitCode -ne 0) {
        exit $buildExitCode
    }
}
