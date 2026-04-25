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
Write-Cmd '.\scripts\clean-all.ps1' 'Clean both Ninja and MSBuild build directories'
Write-Cmd '.\scripts\test.ps1' 'Run unit tests'
Write-Cmd '.\scripts\smoke-test.ps1' 'Run priority 1 smoke tests'
Write-Cmd '.\scripts\render-verify.ps1' 'Run golden-image render verification tests'
Write-Cmd '.\scripts\verify.ps1' 'Full verification: build -> unit tests -> smoke tests -> render verify -> targeted lint'
Write-Cmd '.\scripts\run-vlauncher.ps1' 'Run VLauncher (builds target if missing)'
Write-Cmd '.\scripts\install-hooks.ps1' 'Configure repo-managed Git hooks (blocks commits to main)'

Write-Title "LINT & FORMAT SCRIPTS"
Write-Cmd '.\scripts\lint.ps1' 'Run all available linters (format, shaders, cppcheck, clang-tidy)'
Write-Cmd '.\scripts\lint.ps1 -ChangedOnly' 'Lint only changed C++ and shader files'
Write-Cmd '.\scripts\lint.ps1 -Quick' 'Quick lint (format check + cppcheck only)'
Write-Cmd '.\scripts\lint.ps1 -Fix' 'Auto-fix formatting issues'
Write-Cmd '.\scripts\lint-shaders.ps1' 'Run shader validation only'
Write-Cmd '.\scripts\lint-cppcheck.ps1' 'Run cppcheck only'
Write-Cmd '.\scripts\lint-clang-tidy.ps1' 'Run clang-tidy only'
Write-Cmd '.\scripts\format.ps1' 'Format all C++ files with clang-format'
Write-Cmd '.\scripts\format.ps1 -Check' 'Check formatting without modifying files'

Write-Title "BENCHMARK SCRIPTS"
Write-Cmd '.\scripts\benchmark-compile.ps1 -Label "baseline"' 'Capture per-TU compile timing (clean build)'
Write-Cmd '.\scripts\benchmark-compile.ps1 -Label "modules" -CaptureDetail' 'Capture timing + MSVC FE/BE split'
Write-Cmd '.\scripts\compare-benchmarks.ps1 -Baseline baseline -Candidate modules' 'Diff two benchmark reports'
Write-Cmd '.\scripts\compare-benchmarks.ps1 -Baseline baseline -Candidate modules -Markdown' 'Diff and write Markdown report'

Write-Title "QUICK START"
Write-Cmd '.\scripts\build.ps1' 'Build with Ninja (default; also writes compile_commands.json)'
Write-Cmd '.\scripts\build.ps1 -Generator MSBuild' 'Build with MSBuild'
Write-Cmd '.\scripts\test.ps1' 'Run all tests'
Write-Cmd '.\scripts\test.ps1 -Build' 'Build and test together'
Write-Cmd '.\scripts\smoke-test.ps1' 'Run smoke tests'
Write-Cmd '.\scripts\lint.ps1' 'Run all linters'
Write-Cmd '.\scripts\run-vlauncher.ps1' 'Launch VLauncher from root'
Write-Cmd '.\scripts\install-hooks.ps1' 'Enable local main-branch commit protection'

Write-Title "COMMON TASKS"
Write-Cmd '.\scripts\build.ps1' 'Fast incremental build (Ninja)'
Write-Cmd '.\scripts\test.ps1 -Filter "Camera*"' 'Run specific tests'
Write-Cmd '.\scripts\test.ps1 -ProblemsOnly' 'Show only warnings/failures plus final status'
Write-Cmd '.\scripts\smoke-test.ps1 -ProblemsOnly' 'Show only smoke-test warnings/failures plus final status'
Write-Cmd '.\scripts\smoke-test.ps1 -Extended' 'Include priority 2 examples and games in the smoke run'
Write-Cmd '.\scripts\lint.ps1 -ChangedOnly' 'Lint only the current git delta'
Write-Cmd '.\scripts\verify.ps1' 'Full end-to-end verification (build + tests + smoke + render + targeted lint)'
Write-Cmd '.\scripts\verify.ps1 -SkipSmoke -SkipRenderVerify' 'Quick verify (build + unit tests + targeted lint only)'
Write-Cmd '.\scripts\verify.ps1 -FullLint' 'Full verification with full-repo lint at the end'
Write-Cmd '.\scripts\rebuild.ps1' 'Clean rebuild'
Write-Cmd '.\scripts\clean.ps1 -Full' 'Full clean (removes build dir)'
Write-Cmd '.\scripts\clean-all.ps1 -Full' 'Full clean both Ninja and MSBuild directories'

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
Write-Cmd '.\scripts\new-example.ps1 -Name my_demo' 'Scaffold a new example (creates files, smoke/render scripts, CMakeLists entry)'
Write-Cmd '.\scripts\build.ps1 -Config Release' 'Release build'
Write-Cmd '.\scripts\test.ps1 -Generator Ninja -Build' 'Build with Ninja and test'
Write-Cmd '.\scripts\clean.ps1 -Full; .\scripts\build.ps1' 'Full clean rebuild'
Write-Cmd '.\run-vlauncher.ps1' 'Root shortcut to launch VLauncher'

Write-Title "HELP & DOCS"
Write-Cmd '.\scripts\help.ps1' 'Show this help'
Write-Cmd '.\scripts\show-log.ps1' 'Show latest verification log (logs/verify-latest.log)'
Write-Cmd '.\scripts\show-log.ps1 -Tail 50' 'Show last 50 lines of the latest log'
Write-Cmd '.\scripts\show-log.ps1 -List' 'List all available log files'
Write-Cmd 'Get-Help .\scripts\build.ps1 -Detailed' 'Detailed help for build.ps1'
Write-Cmd 'Get-Help .\scripts\install-hooks.ps1 -Detailed' 'Detailed help for install-hooks.ps1'
Write-Cmd 'cat .\scripts\README.md' 'Read full scripts documentation'
Write-Cmd 'cat .\.github\skills\build-tool-workflows\SKILL.md' 'Complete build guide'

Write-Host ""
Write-Host "For detailed usage, see: " -NoNewline
Write-Host ".\scripts\README.md" -ForegroundColor Cyan
Write-Host ""
