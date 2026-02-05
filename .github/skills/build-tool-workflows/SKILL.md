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

## Building the Project

### Build with Visual Studio (default)

The default build system uses Visual Studio's MSBuild with multi-configuration support.

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
cmake --build build --config Debug --target clean
```

### Build with Ninja (faster incremental builds)

Ninja provides faster incremental builds but requires additional setup.

**Prerequisites:**
- Ninja must be installed: `choco install ninja` or download from https://ninja-build.org/
- Visual Studio Developer environment with 64-bit tools must be loaded

**IMPORTANT:** Always load the Visual Studio Developer environment first with 64-bit architecture:

```powershell
# Load VS environment first (required for all Ninja commands)
$vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -Arch amd64
```

**Initial build (Debug):**
```powershell
# After loading VS environment (see above)
cmake -S . -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build_ninja
```

**Rebuild:**
```powershell
cmake --build build_ninja --clean-first
```

**Clean:**
```powershell
cmake --build build_ninja --target clean
```

**Release build:**
```powershell
# After loading VS environment and cleaning old build
Remove-Item -Path build_ninja -Recurse -Force -ErrorAction SilentlyContinue
cmake -S . -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_ninja
```

## Running Tests

### Using the helper script (recommended)

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

# Run specific tests matching a pattern
./scripts/build-and-test.ps1 -NoBuild -Filter "CameraTest.*"
```

### Using CTest

**Visual Studio build:**
```powershell
ctest --test-dir build --build-config Debug --output-on-failure
```

**Ninja build:**
```powershell
ctest --test-dir build_ninja --output-on-failure
```

### Direct test binary execution

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
  - Always verify the 64-bit environment is loaded when using Ninja
  - Check for `x64` or `amd64` in your PowerShell prompt
  - If builds fail with linker errors (x86/x64 mismatch), reload the environment with `-Arch amd64`

- **Testing efficiently:**
  - Run tests frequently during development with the `-NoBuild` flag to save time
  - Use `--gtest_filter` to run specific tests during focused development
  - Use CTest for CI/CD integration with detailed output
  
- **Build configurations:**
  - Debug builds include symbols and assertions for development
  - Release builds are optimized for performance testing
  - Clean the build directory when switching between Debug and Release with Ninja
