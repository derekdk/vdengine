# VDE Build Scripts

This directory contains PowerShell scripts to simplify building, testing, and maintaining the VDE project.

## Scripts Overview

| Script | Purpose | Quick Example |
|--------|---------|---------------|
| **build.ps1** | Build the project | `.\scripts\build.ps1` |
| **rebuild.ps1** | Clean and rebuild | `.\scripts\rebuild.ps1 -Generator Ninja` |
| **clean.ps1** | Clean build artifacts | `.\scripts\clean.ps1 -Full` |
| **test.ps1** | Run unit tests | `.\scripts\test.ps1 -Filter "Camera*"` |
| **smoke-test.ps1** | Run smoke tests on examples and tools | `.\scripts\smoke-test.ps1 -Extended -Filter "*physics*"` |
| **lint.ps1** | Run all available linters | `.\scripts\lint.ps1` |
| **format.ps1** | Format C++ code with clang-format | `.\scripts\format.ps1` |
| **run-vlauncher.ps1** | Launch VLauncher (builds target if missing) | `\.\scripts\run-vlauncher.ps1` |
| **install-hooks.ps1** | Configure repo-managed Git hooks | `\.\scripts\install-hooks.ps1` |

## Quick Start

### Build (Default: Ninja Debug)
```powershell
.\.\scripts\build.ps1
```

### Launch VLauncher
```powershell
.\scripts\run-vlauncher.ps1
# or root shortcut
.\run-vlauncher.ps1
```

### Build with MSBuild
```powershell
.\.\scripts\build.ps1 -Generator MSBuild
```

### Run Tests
```powershell
.\scripts\test.ps1
```

### Build and Test Together
```powershell
.\scripts\test.ps1 -Build
```

### Run Smoke Tests
```powershell
.\scripts\smoke-test.ps1
```

### Run Extended Smoke Tests
```powershell
.\scripts\smoke-test.ps1 -Extended
```

### Clean Rebuild
```powershell
.\scripts\rebuild.ps1
```

### Enable Local Main-Branch Protection
```powershell
.\scripts\install-hooks.ps1
```

## Detailed Usage

### build.ps1

Build the VDE project with your choice of generator and configuration.

**Syntax:**
```powershell
.\scripts\build.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Clean] [-Parallel <N>]
```

**Parameters:**
- `-Generator` - Build system: `Ninja` (default) or `MSBuild`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Clean` - Clean before building
- `-Parallel <N>` - Parallel build jobs (0 = auto-detect)

**Examples:**
```powershell
# Default build (Ninja Debug)
.\.\scripts\build.ps1

# MSBuild
.\.\scripts\build.ps1 -Generator MSBuild

# Release build
.\scripts\build.ps1 -Config Release

# Clean build with MSBuild
.\.\scripts\build.ps1 -Generator MSBuild -Clean

# Parallel build with 8 jobs
.\scripts\build.ps1 -Parallel 8
```

**Features:**
- Automatically loads VS Developer environment for Ninja builds
- Auto-detects if reconfiguration is needed
- Ninja builds automatically generate `build_ninja/compile_commands.json` for `clang-tidy`
- Shows output locations after successful build

### rebuild.ps1

Perform a full clean rebuild (clean + build).

**Syntax:**
```powershell
.\scripts\rebuild.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release]
```

**Parameters:**
- `-Generator` - Build system: `Ninja` (default) or `MSBuild`
- `-Config` - Configuration: `Debug` (default) or `Release`

**Examples:**
```powershell
# Default rebuild (Ninja Debug)
.\.\scripts\rebuild.ps1

# MSBuild rebuild
.\scripts\rebuild.ps1 -Generator MSBuild

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
- `-Generator` - Build system: `Ninja` (default) or `MSBuild`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Full` - Remove entire build directory (requires reconfigure)

**Examples:**
```powershell
# Clean Debug artifacts (Ninja)
.\.\scripts\clean.ps1

# Clean MSBuild
.\.\scripts\clean.ps1 -Generator MSBuild

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
.\scripts\test.ps1 [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Filter <pattern>] [-Build] [-Verbose] [-ProblemsOnly]
```

**Parameters:**
- `-Generator` - Build system: `Ninja` (default) or `MSBuild`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Filter <pattern>` - GoogleTest filter pattern (default: "*")
- `-Build` - Build before running tests
- `-Verbose` - Verbose test output with timing
- `-ProblemsOnly` - Emit only `WARNING:` / `FAILURE:` lines plus a final `PASS:` or `FAILURE:` summary

**Examples:**
```powershell
# Run all tests
.\scripts\test.ps1

# Run tests with MSBuild
.\.\scripts\test.ps1 -Generator MSBuild

# Build and test
.\scripts\test.ps1 -Build

# Run specific test suite
.\scripts\test.ps1 -Filter "CameraTest.*"

# Run tests matching pattern
.\scripts\test.ps1 -Filter "*Bounds*"

# Verbose output
.\scripts\test.ps1 -Verbose

# AI-friendly failure summary output
.\scripts\test.ps1 -ProblemsOnly

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

### smoke-test.ps1

Run smoke tests against examples and tools, with optional filtering, priority-based example selection, and AI-friendly failure-only output.

**Syntax:**
```powershell
.\scripts\smoke-test.ps1 [-Category All|Examples|Tools] [-Filter <pattern>] [-Generator MSBuild|Ninja] [-Config Debug|Release] [-Build] [-Extended] [-Verbose] [-ProblemsOnly]
```

**Parameters:**
- `-Category` - `All` (default), `Examples`, or `Tools`
- `-Filter <pattern>` - Wildcard pattern for executable names (for example `"*physics*"`)
- `-Generator` - Build system: `Ninja` (default) or `MSBuild`
- `-Config` - Configuration: `Debug` (default) or `Release`
- `-Build` - Build before running smoke tests
- `-Extended` - Include priority 2 examples; default runs only priority 1 examples while tools always run
- `-Verbose` - Verbose output with detailed error messages
- `-ProblemsOnly` - Emit only `WARNING:` / `FAILURE:` lines plus a final `PASS:` or `FAILURE:` summary

**Examples:**
```powershell
# Run all smoke tests
.\scripts\smoke-test.ps1

# Run the extended example set (priority 1 + 2)
.\scripts\smoke-test.ps1 -Extended

# Run only example smoke tests
.\scripts\smoke-test.ps1 -Category Examples

# Run one subset
.\scripts\smoke-test.ps1 -Filter "*physics*"

# Build, then smoke test
.\scripts\smoke-test.ps1 -Build

# AI-friendly failure summary output
.\scripts\smoke-test.ps1 -ProblemsOnly
```

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

### lint.ps1

Run all available linters in sequence. Each linter is skipped if its tool is not installed.

**Syntax:**
```powershell
.\scripts\lint.ps1 [-Quick] [-Fix] [-Help]
```

**Parameters:**
- `-Quick` - Only run format check + cppcheck (fast)
- `-Fix` - Auto-fix formatting issues (runs clang-format in fix mode)
- `-Help` - Show detailed help

**Linters (in order):**
1. **clang-format** — Verifies C++ formatting matches `.clang-format`
2. **glslangValidator** — Validates GLSL shaders against the Vulkan spec
3. **cppcheck** — Static analysis for bugs, performance, portability
4. **clang-tidy** — Deep static analysis (needs `compile_commands.json`, generated automatically by `./scripts/build.ps1 -Generator Ninja`, or by configuring Ninja with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`)

**Examples:**
```powershell
# Run all available linters
.\scripts\lint.ps1

# Quick lint (format + cppcheck only)
.\scripts\lint.ps1 -Quick

# Auto-fix formatting issues
.\scripts\lint.ps1 -Fix

# Show help
.\scripts\lint.ps1 -Help
```

**Tool Installation:**
- **clang-format:** Visual Studio C++ clang tools or LLVM distribution
- **glslangValidator:** Vulkan SDK
- **cppcheck:** https://cppcheck.sourceforge.io/ or package manager
- **clang-tidy:** Visual Studio C++ clang tools or LLVM distribution

### install-hooks.ps1

Configure this clone to use tracked hooks from `.githooks/`.

**Syntax:**
```powershell
.\scripts\install-hooks.ps1
```

**What it does:**
- Sets `core.hooksPath` to `.githooks` (local repository config)
- Enables the shared `pre-commit` hook that blocks commits on `main`
- Keeps Git LFS pre-push checks via `.githooks/pre-push`

**Examples:**
```powershell
# Run once after cloning
.\scripts\install-hooks.ps1

# Verify configured hook path
git config --local --get core.hooksPath
```

## Common Workflows

### Daily Development
```powershell
# Quick Ninja build and test (default)
.\.\scripts\build.ps1
.\.\scripts\test.ps1

# Or combined
.\.\scripts\test.ps1 -Build
```

### Working on Specific Feature
```powershell
# Build
.\.\scripts\build.ps1

# Test related tests only
.\.\scripts\test.ps1 -Filter "MyFeature*"
```

### Clean Rebuild (after CMake changes)
```powershell
.\.\scripts\rebuild.ps1
```

### Pre-commit Checks
```powershell
# Ensure local hooks are enabled first
.\scripts\install-hooks.ps1

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

### Capture a Compile Benchmark (e.g. to measure a modules migration)
```powershell
# 1. Record a baseline before any changes
.\scripts\benchmark-compile.ps1 -Label "baseline-headers"

# 2. Make your code changes (e.g. convert a module)

# 3. Record the candidate run
.\scripts\benchmark-compile.ps1 -Label "phase1-modules"

# 4. Compare the two
.\scripts\compare-benchmarks.ps1 -Baseline "baseline-headers" -Candidate "phase1-modules"

# 5. Add -Markdown to also write a shareable .md report
.\scripts\compare-benchmarks.ps1 -Baseline "baseline-headers" -Candidate "phase1-modules" -Markdown
```

Each benchmark run produces a JSON file in `benchmarks/` named
`<timestamp>-<label>.json`. Labels are matched as substring lookups so you
don't need to type the full filename in `compare-benchmarks.ps1`.

## Benchmark Scripts

### benchmark-compile.ps1

Triggers a full clean rebuild, collects per-TU wall-clock times from the
Ninja log (`.ninja_log`), and optionally captures MSVC front-end / back-end
timing via the `/Bt` compiler flag.  Results are written to `benchmarks/`.

**Syntax:**
```powershell
.\scripts\benchmark-compile.ps1 -Label <string> [-Config Debug|Release]
                                 [-CaptureDetail] [-OutputDir <path>]
                                 [-SkipConfigure]
```

**Parameters:**
- `-Label`          Required. Short tag for the run (e.g. `"baseline-headers"`)
- `-Config`         `Debug` (default) or `Release`
- `-CaptureDetail`  Adds a second serial verbose pass to capture MSVC `/Bt`
                    front-end / back-end CPU split.  Slower, but shows what
                    fraction of each TU is header-parsing vs code-generation.
- `-OutputDir`      Where to write JSON reports (default: `<root>/benchmarks/`)
- `-SkipConfigure`  Skip cmake reconfigure (use when the cache already has
                    `VDE_TIMING=ON` from a prior run)

**The JSON report contains:**
- `build_wall_sec` — real elapsed seconds for the whole build
- `summary` — aggregate stats (total/avg/max/p95 wall ms across all TUs)
- `by_target` — per-target totals (`vde`, `examples`, `tools`, `tests`, `deps`)
- `files` — per-TU row with `wall_ms`, and (if `-CaptureDetail`) `frontend_cpu_s`,
  `backend_cpu_s`, `frontend_pct`

**Requirements:**
- Ninja generator (uses `build_ninja/`)
- MSVC via VS 2022 (script auto-loads VS dev environment)

### compare-benchmarks.ps1

Loads two benchmark JSON files, matches TUs by path, and prints a colour-coded
table of improvements and regressions.

**Syntax:**
```powershell
.\scripts\compare-benchmarks.ps1 -Baseline <report-or-label>
                                  -Candidate <report-or-label>
                                  [-BenchmarksDir <path>]
                                  [-MinDeltaMs <N>]
                                  [-Markdown]
```

**Parameters:**
- `-Baseline`       Path to a JSON file, filename, or a label substring.
                    The most-recent matching file in `benchmarks/` is used.
- `-Candidate`      Same as `-Baseline` but for the newer run.
- `-MinDeltaMs`     Minimum absolute time change to show in the per-TU table
                    (default: 100 ms — filters out noise).
- `-Markdown`       Write an `.md` report to `benchmarks/` as well.

**Output columns:**
- Baseline wall ms → Candidate wall ms → Δ ms → Δ%
- Colour-coded: green = faster, yellow = slower, white = within threshold
- Per-target breakdown plus a final FASTER / SLOWER / no-change verdict

**CMake timing flag (`VDE_TIMING`):**
Both benchmark scripts configure CMake with `-DVDE_TIMING=ON`, which adds the
undocumented-but-reliable MSVC `/Bt` flag (wall time per TU, broken down into
CPU front-end and back-end).  On GCC/Clang the equivalent is `-ftime-report`.
This option is OFF by default so normal builds are unaffected.

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

## CI/CD Workflows

The project uses GitHub Actions for continuous integration. Workflows are defined in `.github/workflows/`:

| Workflow | File | Trigger | Purpose |
|----------|------|---------|---------|
| **Lint** | `lint.yml` | PR + push to main | clang-format, shader validation, cppcheck, cmake-lint, PSScriptAnalyzer |
| **Build & Test** | `build-and-test.yml` | PR + push to main | Build (Debug/Release, `-Werror` on VDE targets) + unit tests + sanitizer run |
| **CodeQL** | `codeql.yml` | PR + push to main + weekly | Security vulnerability scanning |

### Lint Workflow Jobs

- **format-check** — Verifies all C++ files match `.clang-format` rules (~30s)
- **shader-lint** — Validates GLSL shaders with `glslangValidator -V` (~10s)
- **cppcheck** — Static analysis for bugs, performance, portability (~2min)
- **cmake-lint** — Lints `CMakeLists.txt` files (advisory, non-blocking)
- **powershell-lint** — PSScriptAnalyzer on `scripts/` (errors block, warnings advisory)

### Build & Test Workflow Jobs

- **build-and-test (Debug)** — Build with `-Werror` on VDE targets + unit tests
- **build-and-test (Release)** — Build with `-Werror` on VDE targets + unit tests
- **sanitizer** — Build with AddressSanitizer + UndefinedBehaviorSanitizer, run tests
