# VDE clang-tidy Lint Script
# Run clang-tidy against VDE translation units selected from the compile database.
#
# Usage:
#   .\scripts\lint-clang-tidy.ps1
#   .\scripts\lint-clang-tidy.ps1 -Files src\BufferUtils.cpp
#   .\scripts\lint-clang-tidy.ps1 -Generator Ninja
#   .\scripts\lint-clang-tidy.ps1 -Path src -ChunkSize 10 -ListChunks
#   .\scripts\lint-clang-tidy.ps1 -Path src -ChunkSize 10 -Chunk 2

[CmdletBinding()]
param(
    [string[]]$Files = @(),

    [string[]]$Path = @(),

    [ValidateRange(0, 100000)]
    [int]$ChunkSize = 0,

    [ValidateRange(0, 100000)]
    [int]$Chunk = 0,

    [switch]$ListChunks,

    [ValidateSet("Auto", "Ninja", "MSBuild")]
    [string]$Generator = "Auto",

    [switch]$Help
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "lint-common.psm1") -Force

function Show-Help {
    Write-Host @"
VDE clang-tidy Lint Script

Usage:
    .\scripts\lint-clang-tidy.ps1
    .\scripts\lint-clang-tidy.ps1 -Files src\BufferUtils.cpp
    .\scripts\lint-clang-tidy.ps1 -Generator Ninja
    .\scripts\lint-clang-tidy.ps1 -Path src,tests
    .\scripts\lint-clang-tidy.ps1 -Path src -ChunkSize 10 -ListChunks
    .\scripts\lint-clang-tidy.ps1 -Path src -ChunkSize 10 -Chunk 2

Description:
    Runs clang-tidy against VDE translation units discovered from compile_commands.json.
    With no -Files argument, all user translation units in the compile database are linted.
    With -Files, explicit source files and nearby translation units for changed headers are linted.

    For incremental clean-up, narrow the run with -Path and/or split it into fixed-size
    chunks with -ChunkSize. Chunks are taken from the sorted target list, so the same
    -Path/-ChunkSize/-Chunk combination selects the same files on every run (as long as
    the compile database does not gain or lose translation units).

Parameters:
    -Files <paths>   Optional explicit file list (relative or absolute)
    -Path <paths>    Only lint translation units under these repo-relative directories/files
                     (e.g. src, tests, examples\sprite_demo)
    -ChunkSize <n>   Split the selected translation units into chunks of n files
    -Chunk <k>       Run only chunk k (1-based); requires -ChunkSize
    -ListChunks      Print the chunk layout for -ChunkSize and exit without running clang-tidy
    -Generator       Prefer compile_commands.json from Ninja, MSBuild, or Auto (default)
    -Help            Show this help
"@
    exit 0
}

if ($Help) {
    Show-Help
}

if ($ChunkSize -le 0 -and ($Chunk -gt 0 -or $ListChunks)) {
    Write-Host "ERROR: -Chunk and -ListChunks require -ChunkSize." -ForegroundColor Red
    exit 1
}

$RepoRoot = Get-VdeLintRepoRoot -ScriptRoot $PSScriptRoot
Set-Location $RepoRoot

# -ListChunks only needs the compile database, so clang-tidy presence is checked later.
$clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
if (-not $clangTidy -and -not $ListChunks) {
    Write-Host "SKIPPED: clang-tidy (not found in PATH)" -ForegroundColor DarkGray
    exit 0
}

$compileDb = Get-VdeCompileDatabasePath -RepoRoot $RepoRoot -Generator $Generator
if (-not $compileDb) {
    Write-Host "SKIPPED: clang-tidy (no compile_commands.json found)" -ForegroundColor DarkGray
    Write-Host "Generate one with: .\scripts\build.ps1 -Generator Ninja" -ForegroundColor DarkGray
    exit 0
}

$compileDbDir = Split-Path -Parent $compileDb
$compileEntries = Get-Content -Path $compileDb -Raw | ConvertFrom-Json

$translationUnits = @($compileEntries |
    ForEach-Object { ConvertTo-VdeFullPath -RepoRoot $RepoRoot -Path $_.file } |
    Where-Object { $_ } |
    Sort-Object -Unique)

$userTranslationUnits = @(foreach ($file in $translationUnits) {
    $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $file
    if (-not $relative) {
        continue
    }

    if ($relative.StartsWith("src\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("examples\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("games\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("tests\", [System.StringComparison]::OrdinalIgnoreCase) -or
        $relative.StartsWith("tools\", [System.StringComparison]::OrdinalIgnoreCase)) {
        $file
    }
})

function Add-MatchingTranslationUnits {
    param(
        [System.Collections.Generic.HashSet[string]]$TargetSet,
        [string[]]$Candidates,
        [string]$Prefix
    )

    foreach ($candidate in $Candidates) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $candidate
        if ($relative -and $relative.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            [void]$TargetSet.Add($candidate)
        }
    }
}

function Add-RelativeTranslationUnit {
    param(
        [System.Collections.Generic.HashSet[string]]$TargetSet,
        [string[]]$Candidates,
        [string]$RelativePath
    )

    foreach ($candidate in $Candidates) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $candidate
        if ($relative -and $relative.Equals($RelativePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            [void]$TargetSet.Add($candidate)
            return
        }
    }
}

function Add-PairedTranslationUnits {
    param(
        [System.Collections.Generic.HashSet[string]]$TargetSet,
        [string[]]$Candidates,
        [string]$RelativePath
    )

    if ($RelativePath.StartsWith('include\vde\', [System.StringComparison]::OrdinalIgnoreCase)) {
        $subPath = $RelativePath.Substring('include\vde\'.Length)
        $pairedRelative = Join-Path 'src' ([System.IO.Path]::ChangeExtension($subPath, '.cpp'))
        Add-RelativeTranslationUnit -TargetSet $TargetSet -Candidates $Candidates -RelativePath $pairedRelative
        return
    }

    if ($RelativePath.StartsWith('src\', [System.StringComparison]::OrdinalIgnoreCase) -or
        $RelativePath -match '^(examples|games|tools|tests)\\[^\\]+\\') {
        $pairedRelative = [System.IO.Path]::ChangeExtension($RelativePath, '.cpp')
        Add-RelativeTranslationUnit -TargetSet $TargetSet -Candidates $Candidates -RelativePath $pairedRelative
    }
}

function Get-HeaderIncludePatterns {
    param([string]$RelativePath)

    $normalized = $RelativePath.Replace('\', '/')
    $patterns = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    $fileName = [System.IO.Path]::GetFileName($normalized)
    if (-not [string]::IsNullOrWhiteSpace($fileName)) {
        [void]$patterns.Add('"' + $fileName + '"')
        [void]$patterns.Add('<' + $fileName + '>')
    }

    if ($normalized.StartsWith('include/vde/', [System.StringComparison]::OrdinalIgnoreCase)) {
        $publicPath = $normalized.Substring('include/'.Length)
        [void]$patterns.Add('"' + $publicPath + '"')
        [void]$patterns.Add('<' + $publicPath + '>')
    } elseif ($normalized.StartsWith('src/', [System.StringComparison]::OrdinalIgnoreCase)) {
        $sourcePath = $normalized.Substring('src/'.Length)
        [void]$patterns.Add('"' + $sourcePath + '"')
        [void]$patterns.Add('<' + $sourcePath + '>')
    } elseif ($normalized -match '^(examples|games|tools|tests)/[^/]+/(.+)$') {
        $projectRelative = $matches[2]
        [void]$patterns.Add('"' + $projectRelative + '"')
        [void]$patterns.Add('<' + $projectRelative + '>')
    }

    return @($patterns)
}

function Add-DirectIncludingTranslationUnits {
    param(
        [System.Collections.Generic.HashSet[string]]$TargetSet,
        [string[]]$Candidates,
        [string]$RelativeHeaderPath,
        [string]$CandidatePrefix = ''
    )

    $patterns = Get-HeaderIncludePatterns -RelativePath $RelativeHeaderPath
    if (-not $patterns -or $patterns.Count -eq 0) {
        return
    }

    foreach ($candidate in $Candidates) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $candidate
        if (-not $relative) {
            continue
        }

        if ($CandidatePrefix -and -not $relative.StartsWith($CandidatePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        if (Select-String -Path $candidate -Pattern $patterns -SimpleMatch -Quiet) {
            [void]$TargetSet.Add($candidate)
        }
    }
}

if ($Files.Count -gt 0) {
    $selectedFiles = Resolve-VdeFiles -RepoRoot $RepoRoot -Files $Files -AllowedExtensions @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx')
    $targetSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($file in $selectedFiles) {
        $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $file
        if (-not $relative) {
            continue
        }

        $extension = [System.IO.Path]::GetExtension($file)
        if ($extension -in @('.cpp', '.cc', '.cxx')) {
            if ($userTranslationUnits -contains $file) {
                [void]$targetSet.Add($file)
            }
            continue
        }

        Add-PairedTranslationUnits -TargetSet $targetSet -Candidates $userTranslationUnits -RelativePath $relative

        $directIncludePrefix = ''
        if ($relative.StartsWith('include\vde\', [System.StringComparison]::OrdinalIgnoreCase)) {
            $directIncludePrefix = 'src\'
        } elseif ($relative -match '^(examples|games|tools)\\([^\\]+)\\') {
            $directIncludePrefix = "{0}\{1}\" -f $matches[1], $matches[2]
        } elseif ($relative.StartsWith('tests\', [System.StringComparison]::OrdinalIgnoreCase)) {
            $directIncludePrefix = 'tests\'
        }

        Add-DirectIncludingTranslationUnits -TargetSet $targetSet -Candidates $userTranslationUnits -RelativeHeaderPath $relative -CandidatePrefix $directIncludePrefix
    }

    $targets = @(@($targetSet) | Sort-Object)
} else {
    $targets = @($userTranslationUnits | Sort-Object)
}

if ($Path.Count -gt 0) {
    $scopePrefixes = @(foreach ($scope in $Path) {
        $scopeFull = ConvertTo-VdeFullPath -RepoRoot $RepoRoot -Path $scope
        $scopeRelative = if ($scopeFull) { Get-VdeRelativePath -RepoRoot $RepoRoot -Path $scopeFull } else { $null }
        if ($null -eq $scopeRelative) {
            Write-Host "ERROR: -Path '$scope' is not inside the repository." -ForegroundColor Red
            exit 1
        }
        $scopeRelative.TrimEnd('\')
    })

    $selectScoped = {
        param([string[]]$Candidates)
        @(foreach ($candidate in $Candidates) {
            $relative = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $candidate
            foreach ($prefix in $scopePrefixes) {
                if ($prefix -eq '' -or
                    $relative.Equals($prefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                    $relative.StartsWith($prefix + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
                    $candidate
                    break
                }
            }
        })
    }

    $targets = @(& $selectScoped -Candidates $targets)

    if ($targets.Count -eq 0) {
        $pathLabel = $Path -join ', '
        if ($Files.Count -gt 0 -and (& $selectScoped -Candidates $userTranslationUnits).Count -gt 0) {
            Write-Host "SKIPPED: clang-tidy (no translation units selected by -Files are under -Path $pathLabel)" -ForegroundColor DarkGray
            exit 0
        }
        Write-Host "ERROR: no translation units in the compile database match -Path $pathLabel." -ForegroundColor Red
        exit 1
    }
}

if ($targets.Count -eq 0) {
    Write-Host "SKIPPED: clang-tidy (no matching translation units selected)" -ForegroundColor DarkGray
    exit 0
}

$chunkTotal = 0
$chunkLabel = ''
if ($ChunkSize -gt 0) {
    $selectedCount = $targets.Count
    $chunkTotal = [int][Math]::Ceiling($selectedCount / $ChunkSize)

    if ($ListChunks) {
        Write-Host "$selectedCount translation unit(s) in $chunkTotal chunk(s) of up to $($ChunkSize):" -ForegroundColor Cyan
        for ($i = 0; $i -lt $chunkTotal; $i++) {
            $start = $i * $ChunkSize
            $end = [Math]::Min($start + $ChunkSize, $selectedCount) - 1
            Write-Host ("Chunk {0}/{1}:" -f ($i + 1), $chunkTotal) -ForegroundColor Cyan
            foreach ($target in $targets[$start..$end]) {
                Write-Host "  $(Get-VdeRelativePath -RepoRoot $RepoRoot -Path $target)"
            }
        }
        exit 0
    }

    if ($Chunk -lt 1 -or $Chunk -gt $chunkTotal) {
        Write-Host "ERROR: -Chunk must be between 1 and $chunkTotal for -ChunkSize $ChunkSize (use -ListChunks to preview)." -ForegroundColor Red
        exit 1
    }

    $start = ($Chunk - 1) * $ChunkSize
    $end = [Math]::Min($start + $ChunkSize, $selectedCount) - 1
    $targets = @($targets[$start..$end])
    $chunkLabel = " (chunk $Chunk/$chunkTotal of $selectedCount selected)"
}

if (-not $clangTidy) {
    Write-Host "SKIPPED: clang-tidy (not found in PATH)" -ForegroundColor DarkGray
    exit 0
}

Write-Host "Using compile database: $compileDb" -ForegroundColor Cyan
Write-Host "Running clang-tidy on $($targets.Count) translation unit(s)$chunkLabel..." -ForegroundColor Cyan

$hasFindings = $false
$failedTargets = [System.Collections.Generic.List[string]]::new()
$runTimer = [System.Diagnostics.Stopwatch]::StartNew()
$index = 0
foreach ($target in $targets) {
    $index++
    $relativeTarget = Get-VdeRelativePath -RepoRoot $RepoRoot -Path $target
    $progress = "[$index/$($targets.Count)]"
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()

    try {
        $processArgs = @{
            FilePath               = $clangTidy.Source
            ArgumentList           = @('-p', $compileDbDir, '-quiet', $target)
            NoNewWindow            = $true
            Wait                   = $true
            PassThru               = $true
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError  = $stderrPath
        }
        $process = Start-Process @processArgs

        $output = @()
        if (Test-Path $stdoutPath) {
            $output += Get-Content -Path $stdoutPath
        }
        if (Test-Path $stderrPath) {
            $output += Get-Content -Path $stderrPath
        }

        $relevantOutput = $output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $reportedIssue = $false
        foreach ($line in $relevantOutput) {
            if ($line -match ':\d+:\d+:\s+(warning|error):') {
                $reportedIssue = $true
                break
            }
        }

        $elapsed = "{0:N1}s" -f $timer.Elapsed.TotalSeconds
        if ($process.ExitCode -ne 0 -or $reportedIssue) {
            Write-Host "$progress FAILURE: clang-tidy issues in $relativeTarget ($elapsed)" -ForegroundColor Red
            foreach ($line in $relevantOutput) {
                Write-Host "  $line"
            }
            $hasFindings = $true
            $failedTargets.Add($relativeTarget)
        } else {
            Write-Host "$progress ok $relativeTarget ($elapsed)" -ForegroundColor DarkGray
        }
    } finally {
        Remove-Item -Path $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ("clang-tidy finished in {0:N1}s{1}." -f $runTimer.Elapsed.TotalSeconds, $chunkLabel) -ForegroundColor Cyan
if ($chunkTotal -gt 0 -and $Chunk -lt $chunkTotal) {
    $nextArgs = @()
    if ($Files.Count -gt 0) { $nextArgs += "-Files $($Files -join ',')" }
    if ($Path.Count -gt 0) { $nextArgs += "-Path $($Path -join ',')" }
    if ($Generator -ne 'Auto') { $nextArgs += "-Generator $Generator" }
    $nextArgs += "-ChunkSize $ChunkSize -Chunk $($Chunk + 1)"
    Write-Host "Next chunk: $($nextArgs -join ' ')" -ForegroundColor Cyan
}

if ($hasFindings) {
    Write-Host "FAILURE: clang-tidy reported issues in $($failedTargets.Count) translation unit(s):" -ForegroundColor Red
    foreach ($failed in $failedTargets) {
        Write-Host "  $failed" -ForegroundColor Red
    }
    exit 1
}

Write-Host "PASS: clang-tidy found no issues in the selected translation units." -ForegroundColor Green
exit 0