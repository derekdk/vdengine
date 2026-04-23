---
name: build-tool-workflows
description: Build and test workflows for the VDE project. Use this when you need to build or run tests.
---

# Build and Test Workflows

This skill provides the essential workflows for building and testing the Vulkan Display Engine project.

For the fast inner loop while debugging one failing unit test, also consult the `test-fix-loop` skill. This skill is the broad command reference; `test-fix-loop` is the focused red-green workflow.

## When to use this skill

- Building the project with Visual Studio or Ninja
- Running unit tests or specific test suites
- Setting up a development environment
- Troubleshooting build issues
- Choosing between build systems

## Quick Start - Using Build Scripts (RECOMMENDED)

**The VDE project provides convenient PowerShell scripts in the `scripts/` directory for all build operations.**

### Build Scripts Overview

| Script | Purpose | Example |
|--------|---------|---------|
| `build.ps1` | Build the project | `.\scripts\build.ps1 -Generator Ninja -Config Debug` |
| `rebuild.ps1` | Clean and rebuild | `.\scripts\rebuild.ps1 -Generator MSBuild -Config Release` |
| `clean.ps1` | Clean build artifacts | `.\scripts\clean.ps1 -Generator Ninja -Full` |
| `clean-all.ps1` | Clean both Ninja and MSBuild builds | `.\scripts\clean-all.ps1 -Full` |
| `test.ps1` | Run unit tests | `.\scripts\test.ps1 -Filter "CameraTest.*"` |
| `smoke-test.ps1` | Run smoke tests on examples, games, and tools | `.\scripts\smoke-test.ps1 -Build` |
| `format.ps1` | Format C++ code with clang-format | `.\scripts\format.ps1 -Check` |
| `run-vlauncher.ps1` | Launch VLauncher (builds if missing) | `.\scripts\run-vlauncher.ps1` |
| `help.ps1` | Show quick help for build scripts | `.\scripts\help.ps1` |

### Common Build Tasks

**Build with Ninja (default):**
```powershell
.\scripts\build.ps1
```

**Build with MSBuild:**
```powershell
.\scripts\build.ps1 -Generator MSBuild
```

**Release build:**
```powershell
.\scripts\build.ps1 -Config Release
```

**Clean and rebuild:**
```powershell
.\scripts\rebuild.ps1 -Generator Ninja
```

**Clean build artifacts:**
```powershell
.\scripts\clean.ps1
```

**Full clean (removes entire build directory):**
```powershell
.\scripts\clean.ps1 -Full
```

**Run tests:**
```powershell
.\scripts\test.ps1
```

**Run tests with filter:**
```powershell
.\scripts\test.ps1 -Filter "CameraTest.*"
```

**Run tests with AI-friendly failure-only output:**
```powershell
.\scripts\test.ps1 -ProblemsOnly
```

**Build and test in one command:**
```powershell
.\scripts\test.ps1 -Build
```

**Run priority 1 smoke tests (default):**
```powershell
.\scripts\smoke-test.ps1
```

**Run all smoke tests (priority 1 + 2):**
```powershell
.\scripts\smoke-test.ps1 -Extended
```

**Smoke test only examples:**
```powershell
.\scripts\smoke-test.ps1 -Category Examples
```

**Smoke test only tools:**
```powershell
.\scripts\smoke-test.ps1 -Category Tools
```

**Smoke test with filter:**
```powershell
.\scripts\smoke-test.ps1 -Filter "*physics*"
```

**Run smoke tests with AI-friendly failure-only output:**
```powershell
.\scripts\smoke-test.ps1 -ProblemsOnly
```

**Build and smoke test:**
```powershell
.\scripts\smoke-test.ps1 -Build
```

**Clean both Ninja and MSBuild builds:**
```powershell
.\scripts\clean-all.ps1
```

**Full clean both build directories:**
```powershell
.\scripts\clean-all.ps1 -Full
```

**Format C++ code:**
```powershell
.\scripts\format.ps1
```

**Check formatting without modifying files:**
```powershell
.\scripts\format.ps1 -Check
```

**Launch VLauncher:**
```powershell
.\scripts\run-vlauncher.ps1
```

**Show quick help:**
```powershell
.\scripts\help.ps1
```

### Script Parameters Reference

**build.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Clean` - Clean before building
- `-Parallel <N>` - Number of parallel build jobs (0 = auto)

**rebuild.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release

**clean.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Full` - Remove entire build directory

**clean-all.ps1**
- `-Full` - Remove entire build directories for both Ninja and MSBuild

**test.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Filter` - GoogleTest filter pattern (default: "*")
- `-Build` - Build before running tests
- `-Verbose` - Verbose test output
- `-ProblemsOnly` - Emit only warnings/failures plus a final PASS/FAIL line

**smoke-test.ps1**
- `-Category` - All (default), Examples, or Tools
- `-Filter` - Wildcard pattern for executable names (e.g. `"*physics*"`)
- `-Extended` - Include priority 2 examples (default runs only priority 1)
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Build` - Build before running smoke tests
- `-Verbose` - Verbose output with detailed error messages
- `-ProblemsOnly` - Emit only warnings/failures plus a final PASS/FAIL line

**format.ps1**
- `-Check` - Check formatting without modifying files (CI/pre-commit)
- `-Help` - Show detailed help

**run-vlauncher.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Build` - Build VLauncher if it doesn't exist or is out of date

**help.ps1**
- No parameters - displays quick reference for all build scripts

## Switching Build Configurations with Ninja

`build.ps1` uses a single `build_ninja` directory and detects whether reconfiguration is needed by comparing the cached `CMAKE_BUILD_TYPE` and `CMAKE_EXPORT_COMPILE_COMMANDS` values against the requested ones. If either differs, it forces a reconfigure automatically.

**However:** if an existing `build_ninja` cache was created with a different config *outside* of `build.ps1` (e.g. raw `cmake` commands), `build.ps1` may not detect the mismatch. When in doubt, clean first:

```powershell
.\scripts\clean.ps1 -Generator Ninja -Full
.\scripts\build.ps1 -Generator Ninja -Config Release
```

**Rule:** Never assume changing `-Config` alone on an existing `build_ninja` tree switches the build type. Always clean or use `rebuild.ps1` when switching between Debug and Release to ensure a correct build.

```powershell
# Safe way to switch configs
.\scripts\rebuild.ps1 -Generator Ninja -Config Release
```

## Reference

The sections below contain manual build commands for advanced troubleshooting. The scripts above handle all standard workflows.

## Manual Build (Advanced)

If you need finer control or are troubleshooting, you can use CMake directly:

### Build with Ninja (Default)

Ninja provides faster incremental builds and is the recommended build system for development.

**Initial build (Debug):**
```powershell
cmake -S . -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build_ninja
```

**Rebuild:**
```powershell
cmake --build build_ninja --clean-first
```

### Build with Visual Studio (MSBuild)

**Initial build (Debug):**
```powershell
cmake -S . -B build
cmake --build build --config Debug
```

**Rebuild:**
```powershell
cmake --build build --config Debug --clean-first
```

**Note:** The `build.ps1 -Generator Ninja` script handles VS environment setup automatically. Manual steps below are only needed if not using the script.

### Build with Ninja (faster incremental builds)

Ninja provides faster incremental builds but requires additional setup.

**Prerequisites:**
- Ninja must be installed: `choco install ninja` or download from https://ninja-build.org/
- Visual Studio Developer environment with 64-bit tools **MUST** be loaded

**CRITICAL:** Ninja builds will fail with "no include path set" errors if the VS Developer environment is not loaded. This is required for **every new PowerShell session** where you run ninja commands.

**Step 1 - Load Visual Studio Developer Environment (required every session):**
```powershell
# Load VS environment with 64-bit architecture (REQUIRED before any ninja commands)
$vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -Arch amd64

# Verify environment is loaded - you should see "Visual Studio 2022 Developer PowerShell"
# and the prompt should show architecture (e.g., x64)
```

**Step 2 - Configure and build:**
```powershell
# Initial configure (only needed once or when CMakeLists.txt changes)
cmake -S . -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build_ninja
```

**Quick rebuild (after loading VS environment):**
```powershell
cd build_ninja
ninja
```

**Parallel build (faster):**
```powershell
cd build_ninja
ninja -j 8  # Use 8 parallel jobs (adjust based on your CPU cores)
```

**Full rebuild (clean first):**
```powershell
cmake --build build_ninja --clean-first
# Or use ninja directly:
cd build_ninja
ninja -t clean
ninja
```

**Release build:**
```powershell
# Clean and reconfigure for Release
Remove-Item -Path build_ninja -Recurse -Force -ErrorAction SilentlyContinue
cmake -S . -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_ninja
```

**Common Issues:**

**Problem:** Errors like "fatal error C1034: algorithm: no include path set" or "Cannot open include file: 'stddef.h'"
**Solution:** The VS Developer environment is not loaded. Run the Step 1 commands above to load the environment, then try building again.

## Running Unit Tests

```powershell
.\scripts\test.ps1                                    # Run all tests (default Ninja Debug)
.\scripts\test.ps1 -Filter "CameraTest.*"             # Filter by test name
.\scripts\test.ps1 -Build                             # Build first, then test
.\scripts\test.ps1 -Build -Filter "Suite.TestName"    # Build + filtered test
.\scripts\test.ps1 -ProblemsOnly                      # AI-friendly output
.\scripts\test.ps1 -Generator MSBuild -Config Release # Non-default config
```

For fast red-green iteration with filtered tests, see the `test-fix-loop` skill.

## Troubleshooting

**Problem:** Build fails with "no include path set" or "Cannot open include file"
**Solution:** 
- When using scripts: Run `.\scripts\build.ps1 -Generator Ninja` - it handles environment setup
- When using manual commands: Load VS Developer environment first (see Manual Build section)

**Problem:** Wrong architecture errors (x86 vs x64)
**Solution:** The build scripts automatically configure for x64. If using manual commands, ensure `-Arch amd64` is used.

**Problem:** Tests fail to run or executable not found
**Solution:** 
- Ensure you've built first: `.\scripts\test.ps1 -Build`
- Or build separately: `.\scripts\build.ps1` then `.\scripts\test.ps1`
- Check the correct generator is specified if you have both build directories

## Best Practices

- **Use the build scripts for all standard tasks** — never bypass them with raw CMake unless troubleshooting.
- **Ninja (default)** for faster builds during development. MSBuild for IDE integration.
- **Use `-Filter`** for focused test runs during development. See `test-fix-loop` skill for the full iteration strategy.
- **Use `-ProblemsOnly`** when running tests for verification — keeps output small and parseable.
- **Run `.\scripts\format.ps1 -Check`** before committing to verify formatting.
- **See the `terminal-management` skill** for PowerShell pitfalls, long-running command handling, and terminal session health.
