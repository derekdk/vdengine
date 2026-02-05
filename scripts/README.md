# VDE Build Scripts

This directory contains PowerShell scripts to simplify building, testing, and maintaining the VDE project.

## Scripts Overview

| Script | Purpose | Quick Example |
|--------|---------|---------------|
| **build.ps1** | Build the project | `.\scripts\build.ps1` |
| **rebuild.ps1** | Clean and rebuild | `.\scripts\rebuild.ps1 -Generator Ninja` |
| **clean.ps1** | Clean build artifacts | `.\scripts\clean.ps1 -Full` |
| **test.ps1** | Run unit tests | `.\scripts\test.ps1 -Filter "Camera*"` |
| **format.ps1** | Format C++ code with clang-format | `.\scripts\format.ps1` |

## Quick Start

### Build (Default: MSBuild Debug)
```powershell
.\scripts\build.ps1
```

### Build with Ninja (Faster)
```powershell
.\scripts\build.ps1 -Generator Ninja
```

### Run Tests
```powershell
.\scripts\test.ps1
```

### Build and Test Together
```powershell
.\scripts\test.ps1 -Build
```

### Clean Rebuild
```powershell
.\scripts\rebuild.ps1
```

## Detailed Usage

### build.ps1

Build the VDE project with your choice of generator and configuration.

**Syntax:**
```powershell
.\scripts\build.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Clean] [-Parallel <N>]
```

**Parameters:**
- `-Generator` - Build system: `MSBuild` (default) or `Ninja`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Clean` - Clean before building
- `-Parallel <N>` - Parallel build jobs (0 = auto-detect)

**Examples:**
```powershell
# Default build (MSBuild Debug)
.\scripts\build.ps1

# Ninja build
.\scripts\build.ps1 -Generator Ninja

# Release build
.\scripts\build.ps1 -Config Release

# Clean build with Ninja
.\scripts\build.ps1 -Generator Ninja -Clean

# Parallel build with 8 jobs
.\scripts\build.ps1 -Parallel 8
```

**Features:**
- Automatically loads VS Developer environment for Ninja builds
- Auto-detects if reconfiguration is needed
- Shows output locations after successful build

### rebuild.ps1

Perform a full clean rebuild (clean + build).

**Syntax:**
```powershell
.\scripts\rebuild.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release]
```

**Parameters:**
- `-Generator` - Build system: `MSBuild` (default) or `Ninja`
- `-Config` - Configuration: `Debug` (default) or `Release`

**Examples:**
```powershell
# Default rebuild (MSBuild Debug)
.\scripts\rebuild.ps1

# Ninja rebuild
.\scripts\rebuild.ps1 -Generator Ninja

# Release rebuild
.\scripts\rebuild.ps1 -Config Release
```

### clean.ps1

Clean build artifacts or completely remove the build directory.

**Syntax:**
```powershell
.\scripts\clean.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Full]
```

**Parameters:**
- `-Generator` - Build system: `MSBuild` (default) or `Ninja`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Full` - Remove entire build directory (requires reconfigure)

**Examples:**
```powershell
# Clean Debug artifacts (MSBuild)
.\scripts\clean.ps1

# Clean Ninja build
.\scripts\clean.ps1 -Generator Ninja

# Full clean - removes entire build directory
.\scripts\clean.ps1 -Full

# Full clean both generators
.\scripts\clean.ps1 -Full
.\scripts\clean.ps1 -Generator Ninja -Full
```

### test.ps1

Run unit tests with optional filtering and building.

**Syntax:**
```powershell
.\scripts\test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Filter <pattern>] [-Build] [-Verbose]
```

**Parameters:**
- `-Generator` - Build system: `MSBuild` (default) or `Ninja`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Filter <pattern>` - GoogleTest filter pattern (default: "*")
- `-Build` - Build before running tests
- `-Verbose` - Verbose test output with timing

**Examples:**
```powershell
# Run all tests
.\scripts\test.ps1

# Run tests with Ninja build
.\scripts\test.ps1 -Generator Ninja

# Build and test
.\scripts\test.ps1 -Build

# Run specific test suite
.\scripts\test.ps1 -Filter "CameraTest.*"

# Run tests matching pattern
.\scripts\test.ps1 -Filter "*Bounds*"

# Verbose output
.\scripts\test.ps1 -Verbose

# Build, then run filtered tests
.\scripts\test.ps1 -Build -Filter "CameraTest.*"
```

**GoogleTest Filter Patterns:**
- `*` - All tests (default)
- `TestSuite.*` - All tests in a suite
- `TestSuite.TestName` - Specific test
- `*Pattern*` - Tests containing pattern
- `Test1:Test2` - Multiple tests (colon-separated)
- `-Pattern*` - Exclude tests matching pattern

### format.ps1

Format C++ source files using clang-format according to the project's style guide.

**Syntax:**
```powershell
.\scripts\format.ps1 [-Check] [-Help]
```

**Parameters:**
- `-Check` - Check formatting without modifying files
- `-Help` - Show detailed help

**Examples:**
```powershell
# Format all C++ files in-place
.\scripts\format.ps1

# Check formatting (CI/pre-commit)
.\scripts\format.ps1 -Check

# Show help
.\scripts\format.ps1 -Help
```

**Files Formatted:**
- `include/vde/**/*.h` - Public headers
- `src/**/*.cpp` - Implementation files
- `examples/**/*.cpp` - Example code
- `tests/**/*.cpp` - Test files

**Requirements:**
- clang-format must be installed and in PATH
- Install via Visual Studio (C++ clang tools) or LLVM distribution

## Common Workflows

### Daily Development
```powershell
# Quick Ninja build and test
.\scripts\build.ps1 -Generator Ninja
.\scripts\test.ps1 -Generator Ninja

# Or combined
.\scripts\test.ps1 -Generator Ninja -Build
```

### Working on Specific Feature
```powershell
# Build
.\scripts\build.ps1 -Generator Ninja

# Test related tests only
.\scripts\test.ps1 -Generator Ninja -Filter "MyFeature*"
```

### Clean Rebuild (after CMake changes)
```powershell
.\scripts\rebuild.ps1 -Generator Ninja
```

### Pre-commit Checks
```powershell
# Full clean, build, and test
.\scripts\clean.ps1 -Full
.\scripts\build.ps1
.\scripts\test.ps1

# Check code formatting
.\scripts\format.ps1 -Check
```

### Release Testing
```powershell
# Build and test release configuration
.\scripts\build.ps1 -Config Release
.\scripts\test.ps1 -Config Release
```

### Format Code
```powershell
# Format all C++ files
.\scripts\format.ps1

# Check formatting without modifying files
.\scripts\format.ps1 -Check
```

## Build Directories

The scripts use different build directories based on the generator:

| Generator | Build Directory | Test Executable Location |
|-----------|----------------|--------------------------|
| MSBuild | `build/` | `build/tests/Debug/vde_tests.exe` |
| Ninja | `build_ninja/` | `build_ninja/tests/vde_tests.exe` |

You can have both build directories simultaneously and switch between them by specifying `-Generator`.

## Ninja vs MSBuild

**Use Ninja when:**
- You want faster incremental builds
- You're doing iterative development
- You don't need Visual Studio IDE integration

**Use MSBuild when:**
- You want to debug in Visual Studio
- You need multi-configuration support (Debug/Release in same build)
- You prefer simpler setup (no VS environment needed)

**Note:** The scripts handle all environment setup automatically, so Ninja is just as easy to use as MSBuild.

## Legacy Scripts

### build-and-test.ps1 (DEPRECATED)

The old `build-and-test.ps1` script is still available for backward compatibility but is deprecated. It now calls the new `build.ps1` and `test.ps1` scripts internally.

**Migration:**
```powershell
# Old way
./scripts/build-and-test.ps1

# New way
.\scripts\build.ps1
.\scripts\test.ps1

# Or combined
.\scripts\test.ps1 -Build
```

## Troubleshooting

**Problem:** Ninja build fails with "no include path set"
**Solution:** The script should automatically load the VS environment. If it still fails, try manually loading it first (see `.github/skills/build-tool-workflows/SKILL.md`)

**Problem:** Test executable not found
**Solution:** Run with `-Build` flag: `.\scripts\test.ps1 -Build`

**Problem:** Need to switch configurations
**Solution:** Specify both generator and config: `.\scripts\build.ps1 -Generator Ninja -Config Release`

**Problem:** CMake cache is corrupted
**Solution:** Full clean and rebuild:
```powershell
.\scripts\clean.ps1 -Full
.\scripts\build.ps1
```

**Problem:** clang-format not found
**Solution:** Install clang-format via Visual Studio (C++ clang tools component) or download LLVM from https://releases.llvm.org/

## Help

For more detailed information about build workflows, see:
- `.github/skills/build-tool-workflows/SKILL.md` - Complete build documentation
- `README.md` - Project overview
- `docs/GETTING_STARTED.md` - Getting started guide
