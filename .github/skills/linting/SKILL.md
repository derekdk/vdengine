---
name: linting
description: Guide for running static analysis and code quality checks in VDE using lint.ps1. Use this when running linters locally, diagnosing lint failures, or setting up lint tools.
---

# Linting in VDE

This skill covers how to run the VDE lint script, what each linter does, and how to ensure tools are discoverable. `scripts/lint.ps1` remains the top-level entry point and now orchestrates dedicated sub-scripts for shader validation, cppcheck, and clang-tidy.

## When to use this skill

- Running static analysis or format checks locally
- Diagnosing a skipped or failing lint stage
- Setting up clang-tidy, cppcheck, or clang-format for the first time
- Understanding why a linter was skipped
- Adding inline suppressions for cppcheck

---

## The Lint Script

```powershell
.\scripts\lint.ps1
```

Runs four linters in order. Each linter is **skipped silently** if its tool is not found on PATH:

| # | Linter | Tool | Speed |
|---|--------|------|-------|
| 1 | clang-format | `clang-format` | Fast |
| 2 | GLSL shader validation | `glslangValidator` | Medium |
| 3 | cppcheck | `cppcheck` | Medium |
| 4 | clang-tidy | `clang-tidy` | Slow |

### Common invocations

```powershell
# Run all available linters
.\scripts\lint.ps1

# Run targeted lint on changed files
.\scripts\lint.ps1 -ChangedOnly

# Force clang-tidy to use the Ninja compile database
.\scripts\lint.ps1 -ChangedOnly -Generator Ninja

# Fast check: format + cppcheck only (skip shader validation + clang-tidy)
.\scripts\lint.ps1 -Quick

# Auto-fix formatting in place
.\scripts\lint.ps1 -Fix
```

### Direct stage scripts

```powershell
.\scripts\lint-shaders.ps1
.\scripts\lint-cppcheck.ps1
.\scripts\lint-clang-tidy.ps1 -Files src\BufferUtils.cpp
```

---

## Tool Discovery — Required Setup

Each tool is discovered with `Get-Command`. If a tool is not on PATH, its stage is skipped with no error. This means **a skipped stage does not mean a passing stage**.

| Tool | How to install |
|------|---------------|
| `clang-format` | Visual Studio C++ Clang tools component, or LLVM distribution |
| `glslangValidator` | Vulkan SDK (included automatically) |
| `cppcheck` | https://cppcheck.sourceforge.io/ or `choco install cppcheck` |
| `clang-tidy` | Visual Studio C++ Clang tools component, or LLVM distribution |

**clang-tidy and clang-format** are part of the Visual Studio "C++ Clang tools for Windows" optional component. Even after installing them through Visual Studio, they may not be on PATH in a normal PowerShell session — you may need to add the LLVM bin directory to PATH manually or load the VS developer shell.

---

## clang-tidy: Requires compile_commands.json

clang-tidy needs a `compile_commands.json` database to resolve includes correctly. This file is generated only by a **Ninja build**:

```powershell
.\scripts\build.ps1 -Generator Ninja
```

`build.ps1` passes `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` automatically when using Ninja. If `compile_commands.json` is missing, clang-tidy is skipped with a hint to run the Ninja build. `lint.ps1` accepts `-Generator Auto|Ninja|MSBuild` so targeted lint can follow the same build tree you are verifying.

If the existing `build_ninja` cache was configured without `CMAKE_EXPORT_COMPILE_COMMANDS`, `build.ps1` will detect the mismatch and reconfigure automatically.

---

## cppcheck: Required Flags

The repo's cppcheck invocation includes several flags that are essential for correct results:

```
--library=googletest
```
Required so cppcheck correctly parses `TEST` / `TEST_F` macros. Without this, all GoogleTest macros produce false syntax errors.

```
--suppress=ctuOneDefinitionRuleViolation
```
Required for repo-wide scans that include `examples/`. Multiple example executables legitimately reuse scene class names (e.g. `GameScene`). This suppression prevents bogus cross-TU ODR violations across unrelated standalone executables.

These flags are already built into `lint-cppcheck.ps1` — they are noted here so you don't remove them thinking they are unnecessary.

### Inline suppressions

To suppress a specific cppcheck warning on a line, use:

```cpp
// cppcheck-suppress <checkId>
```

The script passes `--inline-suppr`, so these are honoured. Use them sparingly and only for confirmed false positives.

---

## Failure Modes and Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| All stages show SKIPPED | Tools not on PATH | Install tools; verify with `Get-Command clang-tidy` |
| clang-tidy skipped, tool present | No `compile_commands.json` | Run `.\scripts\build.ps1 -Generator Ninja` |
| cppcheck syntax errors in tests | `--library=googletest` missing | Do not remove this flag from `lint-cppcheck.ps1` |
| False ctuOneDefinitionRuleViolation | Cross-example ODR scan | Suppression already in `lint-cppcheck.ps1`; do not remove |
| clang-format fails | Formatting doesn't match `.clang-format` | Run `.\scripts\lint.ps1 -Fix` to auto-fix |
| Windows PowerShell task fails before showing real clang-tidy results | Native stderr handling in the host shell | Use the dedicated `lint-clang-tidy.ps1` path through `lint.ps1`; it captures stdout/stderr safely |

---

## Targeted linting for regular verification

Regular local verification should use targeted lint:

```powershell
.\scripts\lint.ps1 -ChangedOnly
.\scripts\verify.ps1
```

`verify.ps1` now runs targeted lint by default at the end of the pipeline. Use full lint only when you intentionally want the slower repository-wide pass:

```powershell
.\scripts\lint.ps1
.\scripts\verify.ps1 -FullLint
```

For header changes, targeted clang-tidy expands to paired source files and direct includers in the same runnable area rather than scanning every translation unit in the repository.

---

## References

- `build-tool-workflows` — Ninja build and `compile_commands.json` generation
- `writing-code` — Naming conventions and code style rules enforced by clang-format
- `.clang-format` — Root-level formatting configuration
