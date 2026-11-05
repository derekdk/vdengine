# VDE Doctor Script
# Checks the Windows build environment before running expensive workflows.
#
# Usage:
#   .\scripts\doctor.ps1
#   .\scripts\doctor.ps1 -Generator MSBuild
#   .\scripts\doctor.ps1 -SkipIcdCheck

param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",
    [switch]$SkipIcdCheck,
    [switch]$SkipVulkanRuntimeCheck
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$script:failures = 0
$clangFormatVersion = "21.1.8"
$clangFormatDirectory = Join-Path $repoRoot ".venv\Scripts"
if (Test-Path $clangFormatDirectory) {
    $env:PATH = "$clangFormatDirectory;$env:PATH"
}

function Write-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Fix
    )

    if ($Passed) {
        Write-Host "[PASS] $Name" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] $Name - Fix: $Fix" -ForegroundColor Red
        $script:failures++
    }
}

function Get-CommandVersion {
    param([string]$Name, [string[]]$Arguments)

    $tool = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $tool) {
        return $null
    }

    $output = & $tool.Source @Arguments 2>&1
    if (-not $?) {
        return $null
    }
    return [string]($output | Select-Object -First 1)
}

Write-Host "VDE Windows preflight (generator: $Generator)"

$cmakeVersion = Get-CommandVersion -Name "cmake" -Arguments @("--version")
Write-Check "CMake $cmakeVersion" ($null -ne $cmakeVersion) "Install CMake and add it to PATH."

$gitVersion = Get-CommandVersion -Name "git" -Arguments @("--version")
Write-Check "Git $gitVersion" ($null -ne $gitVersion) "Install Git for Windows and add it to PATH."

$gitLfsVersion = Get-CommandVersion -Name "git" -Arguments @("lfs", "version")
Write-Check "Git LFS $gitLfsVersion" ($null -ne $gitLfsVersion) "Install Git LFS, then run git lfs install."

if ($Generator -eq "Ninja") {
    $ninjaVersion = Get-CommandVersion -Name "ninja" -Arguments @("--version")
    Write-Check "Ninja $ninjaVersion" ($null -ne $ninjaVersion) "Install Ninja and add it to PATH."
}

$pythonVersion = Get-CommandVersion -Name "python" -Arguments @("--version")
if (-not $pythonVersion) {
    $pythonVersion = Get-CommandVersion -Name "py" -Arguments @("--version")
}
Write-Check "Python $pythonVersion" ($null -ne $pythonVersion) "Install Python 3 and add python or py to PATH."

$clangFormat = Get-Command "clang-format" -ErrorAction SilentlyContinue
$clangFormatOutput = if ($clangFormat) {
    Get-CommandVersion -Name "clang-format" -Arguments @("--version")
} else {
    $null
}
$clangFormatPassed = $null -ne $clangFormatOutput -and $clangFormatOutput -match "(^|[^0-9])$([regex]::Escape($clangFormatVersion))([^0-9]|$)"
Write-Check "clang-format $clangFormatOutput" $clangFormatPassed "Run: python -m venv .venv; .\.venv\Scripts\python.exe -m pip install clang-format==$clangFormatVersion"

$glslangVersion = Get-CommandVersion -Name "glslangValidator" -Arguments @("--version")
Write-Check "glslangValidator $glslangVersion" ($null -ne $glslangVersion) "Install the Vulkan SDK and add its Bin directory to PATH."

$vulkanSdk = $env:VULKAN_SDK
$loaderPath = if ($vulkanSdk) { Join-Path $vulkanSdk "Bin\vulkan-1.dll" } else { "" }
$systemLoaderPath = if ($env:SystemRoot) { Join-Path $env:SystemRoot "System32\vulkan-1.dll" } else { "" }
$icdPath = if ($vulkanSdk) { Join-Path $vulkanSdk "Bin\VkICD" } else { "" }
$hasSdkLoader = $vulkanSdk -and (Test-Path -LiteralPath $loaderPath -PathType Leaf)
$hasSystemLoader = $systemLoaderPath -and (Test-Path -LiteralPath $systemLoaderPath -PathType Leaf)
$hasSdkIcd = $vulkanSdk -and (Test-Path -LiteralPath $icdPath -PathType Container) -and
    @(Get-ChildItem -Path $icdPath -Filter "*.json" -File -ErrorAction SilentlyContinue).Count -gt 0
$driverOverride = if ($env:VK_DRIVER_FILES) { $env:VK_DRIVER_FILES } else { $env:VK_ICD_FILENAMES }
if (-not [string]::IsNullOrWhiteSpace($driverOverride)) {
    $overridePaths = @($driverOverride -split ";" | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $hasIcd = $overridePaths.Count -gt 0 -and
        @($overridePaths | Where-Object { -not (Test-Path -LiteralPath $_) }).Count -eq 0
} else {
    $adapterIcdPaths = @()
    foreach ($registryPath in @(
        "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\*",
        "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\*"
    )) {
        $adapterIcdPaths += @(Get-ItemProperty -Path $registryPath -Name VulkanDriverName, VulkanDriverNameWow -ErrorAction SilentlyContinue |
            ForEach-Object { $_.VulkanDriverName; $_.VulkanDriverNameWow })
    }
    $hasAdapterIcd = @($adapterIcdPaths | Where-Object { $_ -and (Test-Path -LiteralPath $_) }).Count -gt 0
    $hasRegisteredIcd = Test-Path "HKLM:\SOFTWARE\Khronos\Vulkan\Drivers"
    $hasIcd = $hasSdkIcd -or $hasAdapterIcd -or $hasRegisteredIcd
}
$hasVulkanLoader = $hasSdkLoader -or $hasSystemLoader
if ($SkipVulkanRuntimeCheck) {
    Write-Host "[SKIP] Vulkan loader and ICD - not required for this headless build/test workflow." -ForegroundColor DarkGray
} elseif ($SkipIcdCheck) {
    Write-Check "Vulkan loader" $hasVulkanLoader "Install a Vulkan-capable driver or place vulkan-1.dll in the SDK Bin directory."
    Write-Host "[SKIP] Vulkan ICD - not required for this build/test workflow." -ForegroundColor DarkGray
} else {
    Write-Check "Vulkan loader and ICD" ($hasVulkanLoader -and $hasIcd) "Install a Vulkan loader and configure a Vulkan ICD."
}

$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsFound = $false
if (Test-Path $vsWhere) {
    $vsInstall = & $vsWhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vsFound = $LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($vsInstall)
}
Write-Check "MSVC C++ build tools" $vsFound "Install Visual Studio 2022 with the Desktop development with C++ workload."

$null = & git -C $repoRoot lfs fsck 2>&1
$lfsFsckPassed = $LASTEXITCODE -eq 0
$lfsListing = & git -C $repoRoot lfs ls-files --long 2>&1
$lfsListPassed = $LASTEXITCODE -eq 0
$unmaterializedLfsFiles = @($lfsListing | Where-Object { "$_" -match "^[0-9a-f]{64} - " })
$lfsPassed = $lfsFsckPassed -and $lfsListPassed -and $unmaterializedLfsFiles.Count -eq 0
Write-Check "Git LFS assets" $lfsPassed "Run: git lfs pull; git lfs checkout"

$hooksPath = & git -C $repoRoot config --local --get core.hooksPath 2>$null
$hooksPassed = $LASTEXITCODE -eq 0 -and $hooksPath -eq ".githooks"
Write-Check "Repository hooks" $hooksPassed "Run: .\scripts\install-hooks.ps1"

if ($script:failures -gt 0) {
    Write-Host "FAILURE: Preflight found $($script:failures) problem(s)." -ForegroundColor Red
    exit 1
}

Write-Host "PASS: Windows preflight checks." -ForegroundColor Green
