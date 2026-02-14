# VDE Quick Help Script
# Shows available build scripts and common usage

$ErrorActionPreference = "Stop"

function Write-Title { param([string]$msg) Write-Host "`n$msg" -ForegroundColor Cyan }
function Write-Cmd { param([string]$cmd, [string]$desc) 
    Write-Host "  " -NoNewline
    Write-Host $cmd -ForegroundColor Yellow -NoNewline
    Write-Host " - $desc"
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "   VDE Build Scripts Quick Reference" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green

Write-Title "BUILD SCRIPTS"
Write-Cmd '.\scripts\build.ps1' 'Build the project (default Ninja Debug)'
Write-Cmd '.\scripts\rebuild.ps1' 'Clean and rebuild'
Write-Cmd '.\scripts\clean.ps1' 'Clean build artifacts'
Write-Cmd '.\scripts\test.ps1' 'Run unit tests'
Write-Cmd '.\scripts\run-vlauncher.ps1' 'Run VLauncher (builds target if missing)'

Write-Title "QUICK START"
Write-Cmd '.\scripts\build.ps1' 'Build with Ninja (default)'
Write-Cmd '.\scripts\build.ps1 -Generator MSBuild' 'Build with MSBuild'
Write-Cmd '.\scripts\test.ps1' 'Run all tests'
Write-Cmd '.\scripts\test.ps1 -Build' 'Build and test together'
Write-Cmd '.\scripts\run-vlauncher.ps1' 'Launch VLauncher from root'

Write-Title "COMMON TASKS"
Write-Cmd '.\scripts\build.ps1' 'Fast incremental build (Ninja)'
Write-Cmd '.\scripts\test.ps1 -Filter "Camera*"' 'Run specific tests'
Write-Cmd '.\scripts\rebuild.ps1' 'Clean rebuild'
Write-Cmd '.\scripts\clean.ps1 -Full' 'Full clean (removes build dir)'

Write-Title "BUILD OPTIONS"
Write-Host "  Generators - " -NoNewline
Write-Host "Ninja" -ForegroundColor Yellow -NoNewline
Write-Host " (default), " -NoNewline
Write-Host "MSBuild" -ForegroundColor Yellow
Write-Host "  Configs    - " -NoNewline
Write-Host "Debug" -ForegroundColor Yellow -NoNewline
Write-Host " (default), " -NoNewline
Write-Host "Release" -ForegroundColor Yellow

Write-Title "EXAMPLES"
Write-Cmd '.\scripts\build.ps1 -Config Release' 'Release build'
Write-Cmd '.\scripts\test.ps1 -Generator Ninja -Build' 'Build with Ninja and test'
Write-Cmd '.\scripts\clean.ps1 -Full; .\scripts\build.ps1' 'Full clean rebuild'
Write-Cmd '.\run-vlauncher.ps1' 'Root shortcut to launch VLauncher'

Write-Title "HELP & DOCS"
Write-Cmd '.\scripts\help.ps1' 'Show this help'
Write-Cmd 'Get-Help .\scripts\build.ps1 -Detailed' 'Detailed help for build.ps1'
Write-Cmd 'cat .\scripts\README.md' 'Read full scripts documentation'
Write-Cmd 'cat .\.github\skills\build-tool-workflows\SKILL.md' 'Complete build guide'

Write-Host ""
Write-Host "For detailed usage, see: " -NoNewline
Write-Host ".\scripts\README.md" -ForegroundColor Cyan
Write-Host ""
