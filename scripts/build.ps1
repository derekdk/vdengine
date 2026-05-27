# VDE Build Script
# Builds the VDE project with optional Ninja or MSBuild generator
# Ninja builds also export compile_commands.json for clang-tidy.
# Usage: .\scripts\build.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Clean] [-Parallel <jobs>] [-Verbose] [-ProblemsOnly]

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [switch]$Clean = $false,
    
    [int]$Parallel = 0,  # 0 = auto-detect

    [switch]$Verbose = $false,

    [switch]$ProblemsOnly = $false
)

$ErrorActionPreference = "Stop"

$failurePattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed to\b|\bbuild failed\b|\bconfiguration failed\b|\bcmake error\b|\bninja: build stopped\b|\blink : fatal error\b)'
$warningPattern = '(?i)(^\s*warning\b|\bwarning:|\bcmake warning\b|\bdeprecated\b)'
$problemPattern = '(?i)(^\s*error\b|\berror:|\bfatal error\b|\bfailed to\b|\bbuild failed\b|\bconfiguration failed\b|\bcmake error\b|\bninja: build stopped\b|\blink : fatal error\b|^\s*warning\b|\bwarning:|\bcmake warning\b|\bdeprecated\b)'

. "$PSScriptRoot\vde-problems-only-helpers.ps1"

$ProblemsOnly = Resolve-ProblemsOnlyPreference -BoundParameters $PSBoundParameters -VerboseRequested $Verbose
$ShowWarningsInProblemsOnly = Resolve-ProblemsOnlyWarningPreference -BoundParameters $PSBoundParameters

Write-Info "=========================================="
Write-Info "VDE Build Script"
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

# Ninja-specific setup
if ($Generator -eq "Ninja") {
    Write-Info "Checking for Visual Studio Developer environment..."
    
    # Check if VS environment is loaded
    if (-not $env:VSINSTALLDIR) {
        Write-Info "Loading VS Developer environment..."
        
        $vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
        if (-not $vsPath) {
            Write-Err "Visual Studio installation not found!"
            exit 1
        }
        
        Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
        Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -Arch amd64 | Out-Null
        Write-Success "VS Developer environment loaded (x64)"
    } else {
        Write-Success "VS Developer environment is already loaded"
    }
}

# Create build directory if needed
if (-not (Test-Path $buildDir)) {
    Write-Info "Creating build directory..."
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

Push-Location $buildDir
try {
    # Configure if needed or if CMakeCache doesn't exist
    $needsConfigure = $false
    if (-not (Test-Path "CMakeCache.txt")) {
        $needsConfigure = $true
        Write-Info "CMakeCache.txt not found - configuring..."
    } elseif ($Generator -eq "Ninja") {
        # Check if generator matches
        $cacheContent = Get-Content "CMakeCache.txt" -Raw
        if ($cacheContent -notmatch "CMAKE_GENERATOR:INTERNAL=Ninja") {
            $needsConfigure = $true
            Write-Info "Generator mismatch - reconfiguring..."
        } elseif ($cacheContent -notmatch "CMAKE_BUILD_TYPE:STRING=$Config") {
            $needsConfigure = $true
            Write-Info "Build configuration mismatch - reconfiguring..."
        } elseif ($cacheContent -notmatch "CMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON") {
            $needsConfigure = $true
            Write-Info "compile_commands.json export is disabled - reconfiguring..."
        }
    } elseif ($Generator -eq "MSBuild") {
        $cacheContent = Get-Content "CMakeCache.txt" -Raw
        if ($cacheContent -match "CMAKE_GENERATOR:INTERNAL=Ninja") {
            $needsConfigure = $true
            Write-Info "Generator mismatch - reconfiguring..."
        }
    }
    
    if ($needsConfigure) {
        Write-Info "Configuring CMake with $Generator generator..."

        if ($ProblemsOnly) {
            $configureArgs = if ($Generator -eq "Ninja") {
                @('-S', $vdeRoot, '-B', $buildDir, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Config", '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
            } else {
                @('-S', $vdeRoot, '-B', $buildDir)
            }

            $configureResult = Invoke-CommandForProblemsOnly -Command { & cmake @configureArgs }
            if ($configureResult.ExitCode -ne 0) {
                Write-Err "FAILURE: CMake configuration failed."
                exit 1
            }
        } else {
            if ($Generator -eq "Ninja") {
                cmake -S $vdeRoot -B $buildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config" "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
            } else {
                cmake -S $vdeRoot -B $buildDir
            }

            if ($LASTEXITCODE -ne 0) {
                Write-Err "CMake configuration failed!"
                exit 1
            }
        }

        Write-Success "Configuration complete"
    }
    
    # Build
    Write-Info "Building VDE..."
    
    $buildArgs = @("--build", ".")
    
    if ($Generator -eq "MSBuild") {
        $buildArgs += "--config", $Config
    }
    
    if ($Clean) {
        $buildArgs += "--clean-first"
    }
    
    if ($Parallel -gt 0) {
        $buildArgs += "--parallel", $Parallel
    } else {
        $buildArgs += "--parallel"
    }
    
    if ($ProblemsOnly) {
        $buildResult = Invoke-CommandForProblemsOnly -Command { & cmake @buildArgs } -MaxLines 60
        if ($buildResult.ExitCode -ne 0) {
            Write-Err "FAILURE: Build failed."
            exit 1
        }
    } else {
        & cmake $buildArgs

        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed!"
            exit 1
        }
    }

    if ($Generator -eq "Ninja") {
        $compileDbPath = Join-Path $buildDir "compile_commands.json"
        if (-not (Test-Path $compileDbPath)) {
            Write-Warn "Compile DB not found (expected at $compileDbPath)"
        }

        if (-not $ProblemsOnly) {
            Write-Info "Output location:"
            Write-Info "  Executables: $buildDir\bin\"
            Write-Info "  Libraries: $buildDir\lib\"
            Write-Info "  Tests: $buildDir\tests\vde_tests.exe"
            Write-Info "  Compile DB: $compileDbPath"
        }
    } elseif (-not $ProblemsOnly) {
        Write-Info "Output location:"
        Write-Info "  Executables: $buildDir\examples\$Config\"
        Write-Info "  Libraries: $buildDir\$Config\"
        Write-Info "  Tests: $buildDir\tests\$Config\vde_tests.exe"
    }

    if ($ProblemsOnly) {
        Write-Pass "Build succeeded ($Generator $Config)."
    } else {
        Write-Success "=========================================="
        Write-Success "Build completed successfully!"
        Write-Success "=========================================="
    }
}
finally {
    Pop-Location
}
