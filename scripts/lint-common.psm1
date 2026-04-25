Set-StrictMode -Version Latest

function Get-VdeLintRepoRoot {
    param([string]$ScriptRoot)

    return (Split-Path -Parent $ScriptRoot)
}

function ConvertTo-VdeFullPath {
    param(
        [string]$RepoRoot,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    $candidate = $Path.Replace('/', '\')
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $RepoRoot $candidate
    }

    try {
        return [System.IO.Path]::GetFullPath($candidate)
    } catch {
        return $null
    }
}

function Get-VdeRelativePath {
    param(
        [string]$RepoRoot,
        [string]$Path
    )

    $fullRepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
    $fullPath = [System.IO.Path]::GetFullPath($Path)

    if (-not $fullPath.StartsWith($fullRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }

    $relative = $fullPath.Substring($fullRepoRoot.Length)
    return $relative.TrimStart('\', '/')
}

function Resolve-VdeFiles {
    param(
        [string]$RepoRoot,
        [string[]]$Files,
        [string[]]$AllowedExtensions = @()
    )

    $resolved = @()
    foreach ($file in $Files) {
        $fullPath = ConvertTo-VdeFullPath -RepoRoot $RepoRoot -Path $file
        if (-not $fullPath) {
            continue
        }

        if (-not (Test-Path $fullPath -PathType Leaf)) {
            continue
        }

        if ($AllowedExtensions.Count -gt 0) {
            $extension = [System.IO.Path]::GetExtension($fullPath)
            if ($extension -notin $AllowedExtensions) {
                continue
            }
        }

        $resolved += $fullPath
    }

    return @($resolved | Sort-Object -Unique)
}

function Get-VdeChangedFiles {
    param(
        [string]$RepoRoot,
        [string]$Since = ""
    )

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        throw "git is required for changed-file linting."
    }

    Push-Location $RepoRoot
    try {
        $paths = @()

        if ([string]::IsNullOrWhiteSpace($Since)) {
            $paths += (& git diff --name-only --diff-filter=ACMR)
            $paths += (& git diff --cached --name-only --diff-filter=ACMR)
        } else {
            $paths += (& git diff --name-only --diff-filter=ACMR $Since --)
        }

        $paths += (& git ls-files --others --exclude-standard)

        $fullPaths = foreach ($path in ($paths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)) {
            ConvertTo-VdeFullPath -RepoRoot $RepoRoot -Path $path
        }

        return @($fullPaths | Where-Object { $_ -and (Test-Path $_ -PathType Leaf) } | Sort-Object -Unique)
    } finally {
        Pop-Location
    }
}

function Get-VdeCompileDatabasePath {
    param(
        [string]$RepoRoot,
        [ValidateSet("Auto", "Ninja", "MSBuild")]
        [string]$Generator = "Auto"
    )

    $candidates = switch ($Generator) {
        "Ninja"   { @("build_ninja\compile_commands.json") }
        "MSBuild" { @("build\compile_commands.json") }
        default    { @("build_ninja\compile_commands.json", "build\compile_commands.json") }
    }

    foreach ($candidate in $candidates) {
        $fullPath = Join-Path $RepoRoot $candidate
        if (Test-Path $fullPath -PathType Leaf) {
            return $fullPath
        }
    }

    return $null
}

Export-ModuleMember -Function Get-VdeLintRepoRoot, ConvertTo-VdeFullPath, Get-VdeRelativePath, Resolve-VdeFiles, Get-VdeChangedFiles, Get-VdeCompileDatabasePath