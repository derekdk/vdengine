---
name: build-tool-workflows
description: Build and test workflows for the VDE project. Use this when you need to build or run tests.
---

# Build and Test Workflows

This skill provides the essential workflows for building and testing the Vulkan Display Engine project.

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
| `test.ps1` | Run unit tests | `.\scripts\test.ps1 -Filter "CameraTest.*"` |
| `smoke-test.ps1` | Run smoke tests on examples and tools | `.\scripts\smoke-test.ps1 -Build` |

### Common Build Tasks

**Build with Ninja (default):**
```powershell
.\.\scripts\build.ps1
```

**Build with MSBuild:**
```powershell
.\.\scripts\build.ps1 -Generator MSBuild
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

**Build and test in one command:**
```powershell
.\scripts\test.ps1 -Build
```

**Run smoke tests on all examples and tools:**
```powershell
.\scripts\smoke-test.ps1
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

**Build and smoke test:**
```powershell
.\scripts\smoke-test.ps1 -Build
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

**smoke-test.ps1**
- `-Category` - All (default), Examples, or Tools
- `-Filter` - Wildcard pattern for executable names (e.g. `"*physics*"`)
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Build` - Build before running smoke tests
- `-Verbose` - Verbose output with detailed error messages

**clean.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Full` - Remove entire build directory

**test.ps1**
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
- `-Filter` - GoogleTest filter pattern (default: "*")
- `-Build` - Build before running tests
- `-Verbose` - Verbose test output

## Manual Build (Advanced)

If you need finer control or are troubleshooting, you can use CMake directly:

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

**Clean:**
```powershell

**Note:** The `build.ps1 -Generator Ninja` script handles VS environment setup automatically. Manual steps below are only needed if not using the script.

Ninja provides faster incremental builds but requires additional setup.

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
test script (RECOMMENDED)

```powershell
.\scripts\test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Filter <pattern>] [-Build]
```

**Examples:**
```powershell
# Run all tests (default Ninja Debug)
.\.\scripts\test.ps1

# Run all tests with MSBuild
.\.\scripts\test.ps1 -Generator MSBuild

# Run tests matching a pattern
.\scripts\test.ps1 -Filter "CameraTest.*"

# Build and test in one command
.\scripts\test.ps1 -Build

# Run release tests with Ninja
.\scripts\test.ps1 -Generator Ninja -Config Release -Build
```

## Smoke Tests (Automated Example Testing)

Smoke tests auto-discover and run all built examples and tools with automated input scripts to verify they launch and exit cleanly.

```powershell
.\scripts\smoke-test.ps1 [-Category All|Examples|Tools] [-Filter <pattern>] [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Build] [-Verbose]
```

**Examples:**
```powershell
# Run all smoke tests — examples and tools (default Ninja Debug)
.\scripts\smoke-test.ps1

# Run only example smoke tests
.\scripts\smoke-test.ps1 -Category Examples

# Run only tool smoke tests
.\scripts\smoke-test.ps1 -Category Tools

# Filter to run specific tests by name pattern
.\scripts\smoke-test.ps1 -Filter "*physics*"
.\scripts\smoke-test.ps1 -Filter "vde_vlauncher*"

# Build and smoke test in one command
.\scripts\smoke-test.ps1 -Build

# Verbose mode shows detailed error output
.\scripts\smoke-test.ps1 -Verbose

# Smoke test with MSBuild Release configuration
.\scripts\smoke-test.ps1 -Generator MSBuild -Config Release -Build
```

**VS Code Task:**
- Run Task: `scripts: smoke-test`

**What it does:**
- Auto-discovers `vde_*.exe` in the build directory (examples and tools)
- Runs each executable with its corresponding smoke test script from `scripts/input/`
- Uses `smoke_quick.vdescript` for executables without custom scripts
- Reports pass/fail per executable with category breakdown (Examples/Tools)
- Exits with code 1 if any test fails

**See also:** The **smoke-testing** and **scripted-input** skills for detailed guidance.

### Using the legacy build-and-test script

```powershell
./scripts/build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest] [-Filter <gtest-pattern>] [-Generator MSBuild|Ninja]
```

**Note:** This script is deprecated. Use `build.ps1` and `test.ps1` instea

```powershell
./scripts/build-and-test.ps1 [-BuildConfig Debug|Release] [-NoBuild] [-NoTest] [-Filter <gtest-pattern>]
```

This script builds and runs tests with GoogleTest filters if provided.

**Examples:**
```powershell
# Build and run all tests
./scripts/build-and-test.ps1

# Run tests without building
./scripts/build-and-test.ps1 -NoBuild

# RuUse the build scripts for all standard tasks:**
  - `.\scripts\build.ps1` - Primary build command
  - `.\scripts\test.ps1` - Run tests efficiently
  - `.\scripts\rebuild.ps1` - Clean rebuild when needed
  - `.\scripts\clean.ps1` - Clean build artifacts
  
- **Choose the right build system:**
  - Ninja (default) - Faster builds, better for development/iteration
  - MSBuild - Multi-configuration, IDE integration, simple setup
  - Testing efficiently:**
  - Use `.\scripts\test.ps1` for quick test runs
  - Use `-Filter` to run specific tests during focused development
  - Use `-Build` flag to ensure latest code is tested
  - Run tests frequently during developmen
```powershell
ctest --test-dir build --build-config Debug --output-on-failure
```

**Ninja build:**
```powershell
ctest --test-dir build_ninja --output-on-failure
```
(default) - Includes symbols and assertions for development
  - Release - Optimized for performance testing
  - Scripts handle configuration switching automatically

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
**Visual Studio build:**
```powershell
build/tests/Debug/vde_tests.exe [--gtest_filter=Pattern]
```

**Ninja build:**
```powershell
build_ninja/tests/vde_tests.exe [--gtest_filter=Pattern]
```

## Best Practices

- **Choose the right build system:**
  - Visual Studio for multi-configuration workflows and debugging in the IDE
  - Ninja for faster iterative development and CI/CD pipelines
  
- **Ninja environment setup:**
  - **ALWAYS verify the 64-bit VS Developer environment is loaded before running any ninja commands**
  - Ninja will fail with "no include path set" errors if the environment is not loaded
  - The environment must be reloaded in every new PowerShell session
  - Check for `x64` or `amd64` in your PowerShell prompt after loading the environment
  - If builds fail with "no include path" errors, load the VS environment and try again

- **Testing efficiently:**
  - Run tests frequently during development with the `-NoBuild` flag to save time
  - Use `--gtest_filter` to run specific tests during focused development
  - Use CTest for CI/CD integration with detailed output
  
- **Build configurations:**
  - Debug builds include symbols and assertions for development
  - Release builds are optimized for performance testing
  - Clean the build directory when switching between Debug and Release with Ninja
