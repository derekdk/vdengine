# VDE Smoke Test Script
# Runs automated smoke tests on example, game, and tool executables
# Auto-discovers vde_*.exe in the build directory for examples, games, and tools.
#
# Usage:
#   .\scripts\smoke-test.ps1                              # Run all (examples + games + tools)
#   .\scripts\smoke-test.ps1 -Extended                    # Run priority 1 and 2 examples/games
#   .\scripts\smoke-test.ps1 -Category Examples           # Examples only
#   .\scripts\smoke-test.ps1 -Category Games              # Games only
#   .\scripts\smoke-test.ps1 -Category Tools              # Tools only
#   .\scripts\smoke-test.ps1 -Filter "*physics*"          # Filter by name
#   .\scripts\smoke-test.ps1 -Build -Verbose              # Build first, verbose output
#   .\scripts\smoke-test.ps1 -ProblemsOnly                # Emit only warnings/failures plus final PASS/FAIL

param(
    [ValidateSet("All", "Examples", "Games", "Tools")]
    [string]$Category = "All",

    [string]$Filter = "",  # Wildcard filter for executable names (e.g. "*physics*", "vde_vlauncher*")

    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$Build = $false,  # Build before testing

    [switch]$Extended = $false,  # Include priority 2 examples/games

    [switch]$Verbose = $false,  # Verbose output

    [switch]$ProblemsOnly = $false  # Emit only warnings/failures plus a final PASS/FAIL line
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b)'
$warningPattern = '(?i)\b(warn|warning|validation)\b'
$problemPattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b|^\s*warn(ing)?\b|\bwarning:|\bvalidation\b)'
$outputFailurePattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

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
if ($Extended) {
    Write-Info "Smoke Set: Extended (priority 1 and 2 examples/games)"
} else {
    Write-Info "Smoke Set: Normal (priority 1 examples/games only)"
}
if ($Filter) {
    Write-Info "Filter: $Filter"
}

# Build if requested
if ($Build) {
    if ($ProblemsOnly) {
        Invoke-BuildForProblemsOnly -BuildScriptPath "$scriptDir\build.ps1" -SelectedGenerator $Generator -SelectedConfig $Config
    } else {
        Write-Info "Building before running smoke tests..."
        & "$scriptDir\build.ps1" -Generator $Generator -Config $Config
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed! Cannot run smoke tests."
            exit 1
        }
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

# Executables to exclude from game smoke testing
$excludeFromGames = @(
)

# Tools still use an explicit mapping; example smoke metadata is read from
# each source directory's vde.toml.
$toolSmokeScriptMap = @{
    'vde_vlauncher.exe'                = 'smoke_vlauncher.vdescript'
    'vde_geometry_repl.exe'            = 'smoke_geometry_repl.vdescript'
    'vde_resource_editor.exe'          = 'smoke_resource_editor.vdescript'
}

$defaultSmoke = 'smoke_quick.vdescript'
$defaultSmokePriority = 1
$scriptBaseDir = Join-Path $vdeRoot 'smoketests\scripts'
$exampleTargetSourceMap = @{}
$explicitExampleTomlTargetMap = @{}
$missingExampleMetadataWarnings = @{}
$gameTargetSourceMap = @{}
$explicitGameTomlTargetMap = @{}
$missingGameMetadataWarnings = @{}
$smokeTomlCache = @{}

function Add-TargetSourceMapEntry {
    param(
        [hashtable]$TargetMap,
        [string]$TargetName,
        [string[]]$BlockLines,
        [string]$SourceRootPath,
        [string]$CmakeDirectoryPath
    )

    if ([string]::IsNullOrWhiteSpace($TargetName) -or $TargetName -notlike 'vde_*') {
        return
    }

    $blockText = $BlockLines -join "`n"
    $pathMatches = [regex]::Matches($blockText, '["'']?(?<path>[A-Za-z0-9_./-]+/[^"''\s\)]+)["'']?')
    foreach ($pathMatch in $pathMatches) {
        $sourcePath = $pathMatch.Groups['path'].Value
        if (-not $sourcePath) {
            continue
        }

        if ($sourcePath.StartsWith('$')) {
            continue
        }

        $sourceDir = $sourcePath.Split('/')[0]
        if (-not $sourceDir -or $sourceDir -in @('assets', 'shaders')) {
            continue
        }

        $TargetMap[$TargetName] = Join-Path $SourceRootPath $sourceDir
        return
    }

    if ([string]::IsNullOrWhiteSpace($CmakeDirectoryPath)) {
        return
    }

    $normalizedSourceRoot = [System.IO.Path]::GetFullPath($SourceRootPath).TrimEnd('\', '/')
    $normalizedCmakeDir = [System.IO.Path]::GetFullPath($CmakeDirectoryPath).TrimEnd('\', '/')
    $sourceRootPrefix = $normalizedSourceRoot + [System.IO.Path]::DirectorySeparatorChar
    if ($normalizedCmakeDir.StartsWith($sourceRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relativeDir = $normalizedCmakeDir.Substring($sourceRootPrefix.Length)
        if (-not [string]::IsNullOrWhiteSpace($relativeDir)) {
            $TargetMap[$TargetName] = Join-Path $SourceRootPath $relativeDir
        }
    }
}

function Get-TargetSourceMap {
    param(
        [string]$CmakePath,
        [string]$SourceRootPath,
        [string[]]$CommandNames
    )

    $targetMap = @{}
    if (-not (Test-Path $CmakePath)) {
        return $targetMap
    }

    $commandPattern = (($CommandNames | ForEach-Object { [regex]::Escape($_) }) -join '|')

    $collecting = $false
    $currentTarget = ''
    $currentBlockLines = @()

    foreach ($line in Get-Content -Path $CmakePath) {
        if (-not $collecting) {
            if ($line -match "^\s*($commandPattern)\(\s*([A-Za-z0-9_]+)") {
                $currentTarget = $Matches[2]
                $currentBlockLines = @($line)

                if ($line -match '\)') {
                    Add-TargetSourceMapEntry -TargetMap $targetMap -TargetName $currentTarget -BlockLines $currentBlockLines -SourceRootPath $SourceRootPath -CmakeDirectoryPath (Split-Path -Path $CmakePath -Parent)
                    $currentTarget = ''
                    $currentBlockLines = @()
                } else {
                    $collecting = $true
                }
            }

            continue
        }

        $currentBlockLines += $line
        if ($line -match '\)') {
            Add-TargetSourceMapEntry -TargetMap $targetMap -TargetName $currentTarget -BlockLines $currentBlockLines -SourceRootPath $SourceRootPath -CmakeDirectoryPath (Split-Path -Path $CmakePath -Parent)
            $collecting = $false
            $currentTarget = ''
            $currentBlockLines = @()
        }
    }

    return $targetMap
}

function Get-CategoryTargetSourceMap {
    param(
        [string]$SourceRootPath,
        [string[]]$CommandNames
    )

    $targetMap = @{}
    if (-not (Test-Path $SourceRootPath)) {
        return $targetMap
    }

    foreach ($cmakeFile in Get-ChildItem -Path $SourceRootPath -Filter 'CMakeLists.txt' -Recurse -File) {
        $partialMap = Get-TargetSourceMap -CmakePath $cmakeFile.FullName -SourceRootPath $SourceRootPath -CommandNames $CommandNames
        foreach ($targetName in $partialMap.Keys) {
            $targetMap[$targetName] = $partialMap[$targetName]
        }
    }

    return $targetMap
}

function Get-SmokeSectionMap {
    param([string]$TomlPath)

    if ($smokeTomlCache.ContainsKey($TomlPath)) {
        return $smokeTomlCache[$TomlPath]
    }

    $sectionMap = @{}
    $currentSection = ''

    if (Test-Path $TomlPath) {
        foreach ($rawLine in Get-Content -Path $TomlPath) {
            $line = $rawLine.Trim()
            if (-not $line -or $line.StartsWith('#')) {
                continue
            }

            if ($line -match '^\[(.+)\]\s*$') {
                $currentSection = $Matches[1]
                if ($currentSection -like 'smoke*' -and -not $sectionMap.ContainsKey($currentSection)) {
                    $sectionMap[$currentSection] = [ordered]@{
                        Scripts  = @()
                        Priority = $null
                    }
                }
                continue
            }

            if (-not $currentSection -or -not $sectionMap.ContainsKey($currentSection)) {
                continue
            }

            if ($line -match '^scripts\s*=\s*\[(.*)\]\s*$') {
                $scripts = @()
                foreach ($scriptMatch in [regex]::Matches($Matches[1], '"([^"]+)"')) {
                    $scripts += [System.IO.Path]::GetFileName($scriptMatch.Groups[1].Value)
                }
                $sectionMap[$currentSection]['Scripts'] = @($scripts)
                continue
            }

            if ($line -match '^priority\s*=\s*([0-9]+)\s*$') {
                $sectionMap[$currentSection]['Priority'] = [int]$Matches[1]
            }
        }
    }

    $smokeTomlCache[$TomlPath] = $sectionMap
    return $sectionMap
}

function Get-AppSmokeMetadata {
    param(
        [string]$ExeName,
        [string]$CategoryRoot,
        [hashtable]$TargetSourceMap,
        [hashtable]$ExplicitTomlTargetMap,
        [hashtable]$MissingMetadataWarnings,
        [string]$CategoryLabel
    )

    $targetName = [System.IO.Path]::GetFileNameWithoutExtension($ExeName)
    $sourceDir = Resolve-AppSourceDir -TargetName $targetName -CategoryRoot $CategoryRoot -TargetSourceMap $TargetSourceMap -ExplicitTomlTargetMap $ExplicitTomlTargetMap
    if (-not $sourceDir -and -not $MissingMetadataWarnings.ContainsKey($targetName)) {
        Write-Warn "No $CategoryLabel vde.toml source mapping found for $targetName; using default smoke metadata"
        $MissingMetadataWarnings[$targetName] = $true
    }

    $tomlPath = $null
    if ($sourceDir) {
        $tomlPath = Join-Path $sourceDir 'vde.toml'
    }

    $sectionMap = @{}
    if ($tomlPath) {
        $sectionMap = Get-SmokeSectionMap -TomlPath $tomlPath
    }

    $section = $null
    $perTargetSection = "smoke.$targetName"
    if ($sectionMap.ContainsKey($perTargetSection)) {
        $section = $sectionMap[$perTargetSection]
    } elseif ($sectionMap.ContainsKey('smoke')) {
        $section = $sectionMap['smoke']
    }

    $smokeScript = $defaultSmoke
    if ($section -and $section['Scripts'].Count -gt 0) {
        $smokeScript = $section['Scripts'][0]
    }

    $smokePriority = $defaultSmokePriority
    if ($section -and $null -ne $section['Priority']) {
        $smokePriority = [int]$section['Priority']
        if ($smokePriority -notin @(1, 2)) {
            Write-Warn "Invalid smoke priority $smokePriority for $targetName in $tomlPath; using priority $defaultSmokePriority"
            $smokePriority = $defaultSmokePriority
        }
    }

    return [pscustomobject]@{
        SmokeScript   = $smokeScript
        SmokePriority = $smokePriority
        SourceDir     = $sourceDir
        TomlPath      = $tomlPath
    }
}

function Get-ExplicitTomlTargetMap {
    param([string]$SourceDir)

    $targetMap = @{}
    if (-not (Test-Path $SourceDir)) {
        return $targetMap
    }

    foreach ($tomlFile in Get-ChildItem -Path $SourceDir -Filter 'vde.toml' -Recurse -File) {
        $sectionMap = Get-SmokeSectionMap -TomlPath $tomlFile.FullName
        foreach ($sectionName in $sectionMap.Keys) {
            if ($sectionName -notlike 'smoke.*') {
                continue
            }

            $targetName = $sectionName.Substring('smoke.'.Length)
            if (-not $targetName -or $targetMap.ContainsKey($targetName)) {
                continue
            }

            $targetMap[$targetName] = Split-Path -Parent $tomlFile.FullName
        }
    }

    return $targetMap
}

function Resolve-AppSourceDir {
    param(
        [string]$TargetName,
        [string]$CategoryRoot,
        [hashtable]$TargetSourceMap,
        [hashtable]$ExplicitTomlTargetMap
    )

    if ($TargetSourceMap.ContainsKey($TargetName)) {
        return $TargetSourceMap[$TargetName]
    }

    if ($ExplicitTomlTargetMap.ContainsKey($TargetName)) {
        return $ExplicitTomlTargetMap[$TargetName]
    }

    $candidateNames = @()
    if ($TargetName.StartsWith('vde_')) {
        $candidateNames += $TargetName.Substring(4)
    } else {
        $candidateNames += $TargetName
    }

    if ($candidateNames[0].EndsWith('_example')) {
        $candidateNames += $candidateNames[0].Substring(0, $candidateNames[0].Length - '_example'.Length)
    }

    foreach ($candidateName in ($candidateNames | Select-Object -Unique)) {
        $candidateDir = Join-Path (Join-Path $vdeRoot $CategoryRoot) $candidateName
        if (Test-Path (Join-Path $candidateDir 'vde.toml')) {
            return $candidateDir
        }
    }

    return $null
}

$exampleTargetSourceMap = Get-CategoryTargetSourceMap -SourceRootPath (Join-Path $vdeRoot 'examples') -CommandNames @('add_vde_example', 'add_executable')
$explicitExampleTomlTargetMap = Get-ExplicitTomlTargetMap -SourceDir (Join-Path $vdeRoot 'examples')
$gameTargetSourceMap = Get-CategoryTargetSourceMap -SourceRootPath (Join-Path $vdeRoot 'games') -CommandNames @('add_vde_game', 'add_executable')
$explicitGameTomlTargetMap = Get-ExplicitTomlTargetMap -SourceDir (Join-Path $vdeRoot 'games')

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
            $metadata = Get-AppSmokeMetadata -ExeName $_.Name -CategoryRoot 'examples' -TargetSourceMap $exampleTargetSourceMap -ExplicitTomlTargetMap $explicitExampleTomlTargetMap -MissingMetadataWarnings $missingExampleMetadataWarnings -CategoryLabel 'example'
            [pscustomobject]@{
                Name          = $_.Name
                FullPath      = $_.FullName
                WorkDir       = $_.DirectoryName
                Category      = "Example"
                SmokeScript   = $metadata.SmokeScript
                SmokePriority = $metadata.SmokePriority
            }
        }
    return @($exes)
}

function Get-GameExes {
    $dir = Join-Path $buildDir "games"

    if (-not (Test-Path $dir)) {
        Write-Warn "Games directory not found: $dir"
        return @()
    }

    $exes = Get-ChildItem -Path $dir -Recurse -Filter "vde_*.exe" -File |
        Where-Object { $_.Name -notin $excludeFromGames } |
        ForEach-Object {
            $metadata = Get-AppSmokeMetadata -ExeName $_.Name -CategoryRoot 'games' -TargetSourceMap $gameTargetSourceMap -ExplicitTomlTargetMap $explicitGameTomlTargetMap -MissingMetadataWarnings $missingGameMetadataWarnings -CategoryLabel 'game'
            [pscustomobject]@{
                Name          = $_.Name
                FullPath      = $_.FullName
                WorkDir       = $_.DirectoryName
                Category      = "Game"
                SmokeScript   = $metadata.SmokeScript
                SmokePriority = $metadata.SmokePriority
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
            $smokeScript = $defaultSmoke
            if ($toolSmokeScriptMap.ContainsKey($_.Name)) {
                $smokeScript = $toolSmokeScriptMap[$_.Name]
            }

            [pscustomobject]@{
                Name          = $_.Name
                FullPath      = $_.FullName
                WorkDir       = $_.DirectoryName
                Category      = "Tool"
                SmokeScript   = $smokeScript
                SmokePriority = 1
            }
        }
    return @($exes)
}

# Gather executables based on category
$allExes = @()

if ($Category -eq "All" -or $Category -eq "Examples") {
    $allExes += Get-ExampleExes
}

if ($Category -eq "All" -or $Category -eq "Games") {
    $allExes += Get-GameExes
}

if ($Category -eq "All" -or $Category -eq "Tools") {
    $allExes += Get-ToolExes
}

# Apply wildcard filter
if ($Filter) {
    $allExes = @($allExes | Where-Object { $_.Name -like $Filter })
}

$discoveredCount = $allExes.Count
$filteredPriority2Count = 0
if (-not $Extended) {
    $filteredPriority2Count = @($allExes | Where-Object { ($_.Category -eq 'Example' -or $_.Category -eq 'Game') -and $_.SmokePriority -eq 2 }).Count
    $allExes = @($allExes | Where-Object { (($_.Category -ne 'Example') -and ($_.Category -ne 'Game')) -or $_.SmokePriority -eq 1 })
}

if ($allExes.Count -eq 0) {
    Write-Warn "No executables found to test."
    if ($Filter) {
        Write-Warn "Filter '$Filter' matched nothing. Try a different pattern."
    }
    if (-not $Extended -and $filteredPriority2Count -gt 0) {
        Write-Warn "Only priority 2 examples/games matched. Re-run with -Extended to include them."
    }
    Write-Warn "Run with -Build flag to build first, or run .\scripts\build.ps1"
    exit 1
}

# Sort: examples first, then games, then tools, alphabetically within each category
$categoryOrder = @{
    'Example' = 0
    'Game' = 1
    'Tool' = 2
}
$allExes = @($allExes | Sort-Object @{ Expression = { $categoryOrder[$_.Category] } }, Name)

Write-Info ""
Write-Info "Selected $($allExes.Count) executable(s) to test (from $discoveredCount discovered):"
$exampleCount = @($allExes | Where-Object { $_.Category -eq "Example" }).Count
$gameCount = @($allExes | Where-Object { $_.Category -eq "Game" }).Count
$toolCount = @($allExes | Where-Object { $_.Category -eq "Tool" }).Count
if ($exampleCount -gt 0) { Write-Info "  Examples: $exampleCount" }
if ($gameCount -gt 0) { Write-Info "  Games:    $gameCount" }
if ($toolCount -gt 0) { Write-Info "  Tools:    $toolCount" }
if (-not $Extended -and $filteredPriority2Count -gt 0) {
    Write-Info "  Priority 2 examples/games excluded: $filteredPriority2Count"
}

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
    $smokeScript = $exe.SmokeScript
    if (-not $smokeScript) {
        $smokeScript = $defaultSmoke
    }

    $smokeScriptPath = Join-Path $scriptBaseDir $smokeScript

    if (-not (Test-Path $smokeScriptPath)) {
        Write-Warn "  Smoke script not found: $smokeScript (skipping $($exe.Name))"
        if ($ProblemsOnly) {
            Write-Warn "WARNING: [$($exe.Category)] $($exe.Name) skipped because smoke script '$smokeScript' was not found"
        }
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
        } elseif (-not $ProblemsOnly) {
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

    $keyLines = Get-ProblemLines -Lines $allLines -MaxLines 20

    if ((-not $keyLines) -and $startError) {
        $keyLines = @($startError)
    }

    if ($status -eq 'exited' -and @($allLines | Where-Object { $_ -match $outputFailurePattern }).Count -gt 0) {
        $status = 'output_failure'
        $exitCode = 'output_failure'
    }

    # Determine pass/fail
    $passed = ($exitCode -eq '0')

    if ($passed) {
        $passCount++
        if ($ProblemsOnly) {
            Write-ProblemLines -Prefix "[$($exe.Category)] $($exe.Name): " -Lines @($keyLines | Where-Object { Test-IsWarningLine -Line $_ })
        } elseif ($Verbose) {
            Write-Success "  PASSED"
        } else {
            Write-Success " PASSED"
        }
    } else {
        $failCount++
        if ($ProblemsOnly) {
            Write-Err "FAILURE: [$($exe.Category)] $($exe.Name) (script: $smokeScript, exit code: $exitCode)"
            if ($keyLines) {
                Write-ProblemLines -Prefix "[$($exe.Category)] $($exe.Name): " -Lines $keyLines
            }
        } elseif ($Verbose) {
            Write-Err "  FAILED (exit code: $exitCode)"
        } else {
            Write-Err " FAILED (exit code: $exitCode)"
        }

        if ($keyLines -and $Verbose -and -not $ProblemsOnly) {
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

if ($ProblemsOnly) {
    if ($failCount -gt 0) {
        Write-Err "FAILURE: Smoke tests failed. Passed: $passCount, Failed: $failCount, Skipped: $skipCount"
        exit 1
    }

    if ($skipCount -gt 0) {
        Write-Pass "Smoke tests passed with $skipCount warning(s)."
    } else {
        Write-Pass "All smoke tests passed."
    }
    exit 0
}

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
