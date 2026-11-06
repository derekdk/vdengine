# Cross-Platform Development Workflow Recommendations

**Date:** 2026-10-30  
**Scope:** Developer experience for Windows (PC) and macOS. This review covers
build, test, lint, CI, AI-agent guidance, and docs. It does not review
engine features; see [`code-analysis/`](../code-analysis/) for code findings.

---

## 1. What was tested

> **Coverage scope:** Everything in 1.1 was **executed on macOS**. No Windows
> machine was available, so Windows/PC coverage (1.2) is **static only**:
> parsing and analyzing the PowerShell scripts and reviewing the Windows-only
> C++ code. **A Windows build, test, and smoke run is still required** before
> these findings can be called cross-platform verified. See 1.3 for the
> outstanding list, and 3.1 for making it automatic in CI.

### 1.1 macOS (executed)

The tests ran on macOS 27.0.1 (Apple Silicon) with Apple Clang 21, CMake 4.4.4,
Ninja, and Homebrew MoltenVK, starting from `main` at `d5fc9c4`.

| Check | Command | Result |
|---|---|---|
| Build (Debug, all targets) | `bash scripts/macos.sh build` | ✅ Pass (incremental) |
| Unit tests (before this workflow change) | `bash scripts/macos.sh test` | ✅ 1501/1501 pass, 2 skipped (COLR v1 emoji), **92 s serial** |
| Unit tests, parallel (manual CTest command before this workflow change) | `ctest --test-dir build_ninja -j8` | ✅ Same result in **20 s** |
| Quick smoke (launch/render/exit) | `bash scripts/macos.sh smoke` | ✅ 51/51 apps pass |
| Lint (macOS workflow) | `bash scripts/macos.sh lint` | ❌ **Failed: `clang-format` not installed** |
| clang-format (CI-pinned 21.1.8) | All `.cpp`/`.h` in `include src examples tests tools games` | ✅ 0 violations |
| Shader validation | `glslangValidator -V shaders/*` | ✅ 13/13 |
| ShellCheck | `shellcheck scripts/macos.sh` | ✅ Clean |
| Full `macos.sh verify` | Build, tests, smoke, and lint | ❌ Took 6m01s and failed only at lint |
| Post-fix full verification | `VDE_JOBS=8 bash scripts/macos.sh verify` | ✅ Build; 1501 tests in 19.18 s; 51 smoke checks; changed-file lint |

### 1.2 Windows / PC (static checks run from macOS)

These checks used a portable PowerShell 7.4.6 and PSScriptAnalyzer. They
validate the Windows scripts and code without running them on Windows.

| Check | Result |
|---|---|
| Parse all 28 tracked `.ps1` files with the PowerShell parser | ✅ 0 syntax errors |
| `PSUseCompatibleSyntax` for Windows PowerShell **5.1** and PowerShell **7** | ✅ 0 issues; scripts work in either host |
| PSScriptAnalyzer (Error + Warning) | ✅ 0 errors. 274 warnings, mostly style (`Write-Host` ×222). Notable ones are listed below |
| `.ps1`/`.bat`/`.sh` line endings in the index | ✅ All 31 are LF in the index and working tree (on this Mac) |
| Windows-only C++ (`#ifdef _WIN32`: 42 blocks in 19 files) | 🔍 Reviewed by reading only; **not compiled with MSVC** |
| Git history for MSVC/Windows fixes | No dedicated Windows CI, so MSVC breakage is only found on a developer's PC |

Notable findings:

- **Two different PowerShell hosts.** [`verify.ps1`](../scripts/verify.ps1)
  prefers `pwsh` (7), but [`lint.ps1`](../scripts/lint.ps1) prefers
  `powershell` (5.1), and
  [`vde-problems-only-helpers.ps1`](../scripts/vde-problems-only-helpers.ps1)
  hard-codes `powershell.exe`. The same script can therefore run under 5.1 or
  7 depending on how it was started. The scripts are currently compatible with
  both, but nothing enforces it. Pick one host (recommended: `pwsh` 7, which
  also runs on macOS), add `#Requires -Version 7` or a CI check with
  `PSUseCompatibleSyntax`, and stop calling `powershell.exe` by name.
- **No PowerShell lint in CI.** Add a PSScriptAnalyzer step (it runs on Linux
  and macOS runners) with the default rules, excluding `PSAvoidUsingWriteHost`.
  This would have flagged the leftover unused variables in
  [`test.ps1`](../scripts/test.ps1) and
  [`render-verify.ps1`](../scripts/render-verify.ps1) (`$failurePattern`,
  `$warningPattern`, `$problemPattern`), and that
  [`verify.ps1`](../scripts/verify.ps1) defines `Write-Log`, which overrides a
  built-in cmdlet in PowerShell 7 on Windows.
- `PSUseUsingScopeModifierInNewRunspaces` (20 warnings in `smoke-test.ps1` and
  `render-verify.ps1`) are **false positives**: the `Start-Job` blocks receive
  their values through `param()` and `-ArgumentList`.
- [`resource_editor_smoke.ps1`](../smoketests/resource_editor_smoke.ps1) is
  UTF-8 without a BOM and contains an em dash. Windows PowerShell 5.1 reads
  such files as ANSI. It is only in a comment, so it is harmless today, but
  non-ASCII characters in strings would be garbled under 5.1. This is another
  reason to standardize on `pwsh` 7.
- **Native file dialogs work only on Windows.**
  [`FileDialog.cpp`](../tools/geometry_repl/FileDialog.cpp) (used by
  `geometry_repl` and `hex_editor`) and
  [`FileOperations.cpp`](../tools/resource_editor/FileOperations.cpp) use COM
  `IFileOpenDialog`/`IFileSaveDialog`. On macOS they are stubs that return an
  empty string, so Open, Save, and Export do nothing and show no message. See
  3.12.
- The remaining Windows-specific code (storage paths, the vlauncher process
  launcher, emoji fonts, and `NOMINMAX`/`WIN32_LEAN_AND_MEAN` guards) has
  matching `__APPLE__` branches and looked consistent. It still needs an MSVC
  compile to confirm.

### 1.3 Still unverified on Windows (required follow-up)

Run these on a Windows PC (or in the CI job from 3.1) to complete the
cross-platform test:

1. `.\scripts\build.ps1` (Ninja + MSVC, `/W4 /WX`) and the MSBuild generator.
2. `.\scripts\test.ps1`: confirm the same 1501 pass / 2 skip count as macOS.
   The COLR emoji tests may *run* on Windows instead of skipping.
3. `.\scripts\smoke-test.ps1`, which uses the per-app `vde.toml` scripts that
   macOS does not run (see 3.6).
4. `.\scripts\lint.ps1` under both `powershell` and `pwsh`, and
   `.\scripts\render-verify.ps1` (golden images exist only for Windows).
5. A fresh clone with `core.autocrlf=true`, then run `bash scripts/macos.sh`
   from Git Bash or WSL and `.githooks/pre-commit`. This confirms the
   line-ending risk in 3.3.
6. `.\scripts\install-hooks.ps1` and `run-vlauncher.bat`/`.ps1`.

**Bottom line:** The code is in good shape on macOS, and the Windows scripts
pass every static check that can run without Windows. Most of the friction
comes from tooling: Windows and macOS use two separate workflows, a missing
tool did not show up until the last step, the docs and AI agents assume
Windows, and nothing automatically checks the PC build.

---

## 2. Top recommendations (prioritized)

Row numbers match the detail sections (row *n* → section 3.*n*).

| # | Priority | Recommendation | Effort | Payoff |
|---|---|---|---|---|
| 1 | P0 | Add a **Windows CI job** | S | Catches PC breakage before merge |
| 2 | P0 | Add a **`doctor` preflight** command and pin tool versions | S | Missing tools fail in seconds, not at minute 6 |
| 3 | P0 | Add **EOL rules** to `.gitattributes` | XS | Prevents CRLF-broken `.sh`/hooks on Windows |
| 4 | P1 | Add **`CMakePresets.json`** as the single source of build configuration | M | One set of flags for scripts, IDEs, and CI |
| 5 | P1 | Run **CTest in parallel** | XS | Unit tests run ~4.5× faster (92 s → 20 s) |
| 6 | P1 | Register **smoke tests in CTest** using `vde.toml` | M | One smoke system for both platforms |
| 7 | P1 | Make **AI skills/instructions platform-aware** | S | Agents on Mac stop calling `.ps1` scripts |
| 8 | P2 | Make **dependency fetching** smaller, cached, and hash-verified | S | Cuts ~800 MB per build directory and speeds up clean builds |
| 9 | P2 | Make **hooks install** part of setup on both OSes | XS | The protected-`main` hook applies to every developer |
| 10 | P2 | Add **ccache/sccache** support | S | Faster rebuilds after branch switches |
| 11 | P3 | **Tidy up** the repo root, stray scripts, and docs | S | Easier for new developers to find their way around |
| 12 | P2 | **Cross-platform file dialogs** for tools | S | Open/Save/Export work on Mac instead of doing nothing |
| 13 | P1 | **One PowerShell host (`pwsh` 7) + PSScriptAnalyzer in CI** | XS | Windows scripts behave the same however they are started |

---

## 3. Details

### 3.1 Add a Windows CI job (P0)

[`build-and-test.yml`](../.github/workflows/build-and-test.yml) runs macOS,
Ubuntu, and sanitizer jobs, but **no Windows job**, even though Windows is a
primary target. MSVC-only problems therefore reach `main` unchecked. Examples
include `/W4 /WX` warnings, `NOMINMAX`/`WIN32_LEAN_AND_MEAN` interactions, COLR
emoji code, and `.exe` handling in the scripts.

```yaml
windows:
  name: Build & Test (Windows MSVC)
  runs-on: windows-latest
  steps:
    - uses: actions/checkout@v4
      with: { lfs: true }
    - uses: humbletim/install-vulkan-sdk@v1.2 # or the LunarG installer
      with: { version: 1.4.309.0, cache: true }
    - uses: ilammy/msvc-dev-cmd@v1
    - run: cmake --preset windows-debug        # see 3.4
    - run: cmake --build --preset windows-debug
    - run: ctest --preset windows-debug
```

Also consider these changes:
- Add `games` to the Linux build (`-DVDE_BUILD_GAMES=ON` is not passed
  explicitly) and to the clang-format `find` list in
  [`lint.yml`](../.github/workflows/lint.yml). `games/` currently passes
  formatting, but CI does not enforce it.
- Pin the runner images (`macos-15`, `windows-2022`) so they do not change
  without warning.

### 3.2 Add a preflight `doctor` command and pin tool versions (P0)

During this review, `macos.sh verify` spent 6 minutes building, testing, and
smoke-testing before it failed with `Missing clang-format`. `clang-format` is
listed in [`Brewfile.macos`](../scripts/Brewfile.macos), but this machine was
set up before that entry was added, and nothing detected the drift.

- Add `bash scripts/macos.sh doctor` and `.\scripts\doctor.ps1`. Each should
  check every required tool and version, the Vulkan loader and ICD, the Git LFS
  pull state, and the hooks path. Print one line per check (✅/❌) followed by
  the command that fixes it. On macOS, `brew bundle check --file=scripts/Brewfile.macos`
  covers most of this.
- Run `doctor` (or the relevant `require` checks) at the **start** of
  `verify`, before the build.
- **Pin clang-format to one version everywhere.** CI installs
  `clang-format==21.1.8` from pip, Homebrew installs whatever version is
  current, and the Windows docs say "Visual Studio C++ clang tools or LLVM." A
  different major version can produce different formatting, which leads to
  "passes locally, fails in CI." Make `pip install clang-format==21.1.8` (in a
  repo-local venv) the standard method on both OSes, and have `format`/`lint`
  check `clang-format --version`.

### 3.3 Line-ending rules (P0) — implemented

[`.gitattributes`](../.gitattributes) now normalizes text files, keeps shell
scripts and hooks LF, and sets Windows batch files to CRLF. This prevents
`core.autocrlf` from breaking shell scripts and hooks on Windows checkouts.
PowerShell scripts are also kept LF, which both Windows PowerShell and `pwsh`
support.

```gitattributes
* text=auto
*.sh        text eol=lf
.githooks/* text eol=lf
*.ps1       text eol=lf    # PowerShell 5.1 and pwsh both support LF
*.bat       text eol=crlf
*.cmd       text eol=crlf
```

### 3.4 Use CMake presets as the single source of build configuration (P1)

The same CMake flags are currently written out in four places that already
disagree:

| Location | `CMAKE_POLICY_VERSION_MINIMUM` | `VDE_BUILD_GAMES` | Wayland |
|---|---|---|---|
| [`scripts/macos.sh`](../scripts/macos.sh) | 3.5 | ON | n/a |
| [`scripts/build.ps1`](../scripts/build.ps1) (Ninja) | not set | default | n/a |
| Linux CI | not set | not set | OFF |
| macOS sanitizer CI | 3.5 | OFF | n/a |

Add a committed `CMakePresets.json` with `windows-debug/release` (Ninja +
MSVC), `windows-msbuild`, `macos-debug/release`, `linux-debug`, and `asan`
presets, each with its **own binary directory** (`build/<preset>`). This
provides:

- Switching between Debug and Release no longer reconfigures and invalidates the
  shared `build_ninja` directory, which `macos.sh` does today.
- VS Code CMake Tools, Visual Studio, CLion, and Xcode can open the project
  using the same flags as the scripts.
- The scripts get shorter: `build.ps1` and `macos.sh` call
  `cmake --preset <x>` / `cmake --build --preset <x>` / `ctest --preset <x>`.
- Move `CMAKE_POLICY_VERSION_MINIMUM` into the presets, or better, update the
  dependencies that need it so that Windows developers with CMake 4.x do not
  hit the same failure.

> The project rule "never use CMake Tools directly" exists because the scripts
> perform environment setup. With presets, the scripts and IDEs share the same
> configuration, so that rule can be relaxed or kept as a guideline.

### 3.5 Run CTest in parallel (P1) — implemented on macOS

`macos.sh test` now passes the existing `VDE_JOBS` limit to CTest. Each of the
1501 gtest-discovered tests starts its own process, so parallel execution
reduces process startup overhead; a manual `-j8` run took 20 s instead of 92 s
serially, with the same results. The Windows `test.ps1` still launches the
GoogleTest executable directly and remains serial. If GPU-backed tests need
exclusive access, mark them with the CTest `RUN_SERIAL` or `RESOURCE_LOCK`
property instead of serializing the whole suite. The build also prints
`ld: warning: ignoring duplicate libraries: 'lib/libgtest.a'`: remove the
duplicate `gtest` from the test target's link list.

### 3.6 One smoke-test system for both OSes (P1)

Windows smoke testing reads 50 `vde.toml` files (per-app `scripts`,
`priority`, `source_paths` for changed-only selection) through
[`smoke-test.ps1`](../scripts/smoke-test.ps1). macOS **ignores all of this**:
it runs `smoke_quick.vdescript` for every app and special-cases only the audio
demo. As a result, Mac coverage is much weaker: per-app interaction scripts
such as breakout, asteroids, and level-builder never run on Mac, even though
the macOS smoke harness itself works well (51/51 pass).

Recommended: register smoke tests in **CMake/CTest**. At configure time, read
each app's `vde.toml` (toml++ is already a dependency, or use a small CMake
parser) and emit one `add_test()` per `(app, script)` pair:

```cmake
add_test(NAME smoke.breakout.smoke_breakout
         COMMAND vde_breakout_demo --input-script ${VDE_SMOKE_DIR}/smoke_breakout.vdescript)
set_tests_properties(... PROPERTIES LABELS "smoke;p1" TIMEOUT 45
    PASS_REGULAR_EXPRESSION "\\[VDE:InputScript\\] exit"
    FAIL_REGULAR_EXPRESSION "Validation Error;ASSERT FAILED")
```

Then both OSes run `ctest -L smoke` (and `-L p1`). CTest also provides
timeouts, so neither the Bash watchdog nor the PowerShell job logic is needed.
The scripts can keep the changed-only selection by mapping changed paths to
labels.

### 3.7 Platform-aware AI-agent guidance (P1)

[`.github/copilot-instructions.md`](../.github/copilot-instructions.md) and
**26 of the 27** skills mention only PowerShell; there are 236 `.ps1`
references. `ai-verification` tells agents to run `.\scripts\verify.ps1` and
read `logs/verify-latest.log`. On a Mac, neither works: there is no `pwsh`, and
the Mac log is `logs/verify-macos-latest.log`. Recommended changes:

- Add a short "Platform commands" table near the top of
  `copilot-instructions.md`:

  | Task | Windows | macOS |
  |---|---|---|
  | Build | `.\scripts\build.ps1` | `bash scripts/macos.sh build` |
  | Unit tests | `.\scripts\test.ps1 -Filter X` | `bash scripts/macos.sh test X` |
  | Smoke | `.\scripts\smoke-test.ps1` | `bash scripts/macos.sh smoke '*x*'` |
  | Verify | `.\scripts\verify.ps1` → `logs/verify-latest.log` | `bash scripts/macos.sh verify` → `logs/verify-macos-latest.log` |

- Update `ai-verification`, `build-tool-workflows`, `test-fix-loop`,
  `completing-work`, and `fixing-bugs` to refer to that table instead of
  hard-coding `.ps1` commands.
- Have `macos.sh verify` print the same `VERIFICATION SUMMARY` block as
  `verify.ps1`, so agents and people can read results the same way on both
  platforms. Currently the Mac log ends with only the first `ERROR:` line.

### 3.8 Dependency fetching (P2)

`build_ninja/_deps` contains ~820 MB of sources, mostly from full git clones:

| Dependency | Size | Better source |
|---|---|---|
| miniaudio | 326 MB | Single-header release archive with `URL_HASH` |
| nlohmann_json | 188 MB | `json.tar.xz` release asset (a few hundred KB) |
| glslang | 127 MB | `GIT_SHALLOW TRUE` or a release tarball |
| glm | 99 MB | Release zip |

Recommendations:
- ✅ Add `URL_HASH SHA256=...` to the SQLite download. The archive is now
  checked against its SHA-256 digest.
- Set `FETCHCONTENT_BASE_DIR` (or `CPM_SOURCE_CACHE` if you adopt CPM) to a
  per-user cache, so that new build directories and worktrees do not download
  everything again. Cache that directory in CI with `actions/cache`.
- The GLFW/GLM checks look in `${CMAKE_CURRENT_SOURCE_DIR}/../third_party/...`,
  which is **outside the repository**. If this was intended to support
  vendoring, use `${PROJECT_SOURCE_DIR}/third_party`. Otherwise, remove those
  checks.
- Silence the CMake policy warning at `CMakeLists.txt:77` and the glm
  deprecation noise. These warnings appear on every configure and make real
  warnings harder to notice.

### 3.9 Hooks installation (P2) — implemented on macOS

`.githooks/pre-commit` blocks direct commits to `main`. `macos.sh setup` now
sets this clone's `core.hooksPath` to `.githooks`, matching
[`install-hooks.ps1`](../scripts/install-hooks.ps1). A preflight `doctor`
command could still report when hooks have not been enabled.

### 3.10 Compiler cache (P2)

When `ccache` or `sccache` is on `PATH`, set `CMAKE_CXX_COMPILER_LAUNCHER` to it
(in presets or `CMakeLists.txt`). sccache works with MSVC, and ccache works
with Apple Clang. With `-Werror`/`/WX` and a large test target, rebuilds after
branch switches take much longer without a compiler cache.

### 3.11 Tidy-up (P3)

- `scripts/tmp_run_examples_check.ps1` is tracked in git but looks like a
  temporary script: it has a hard-coded `.exe` map that duplicates `vde.toml`.
  Delete it or make it a supported script.
- Move `API-DESIGN.MD`, `API-DOC.md`, `CLANG_FORMAT_SETUP.md`, and
  `DEVELOPMENT_TIMELINE.md` from the repository root into `docs/`. The `.MD`
  extension casing is inconsistent and can cause problems with case-sensitive
  tools.
- `docs/` contains 31 files, many of them completed plans. Move finished plans
  to `docs/history/` and keep `docs/README.md` as the index, so that new
  contributors, people and AI agents, can find the current guidance.
- `.gitignore` lists `.vscode/`, but `.vscode/tasks.json` is tracked. Change
  the entry to `.vscode/*` + `!.vscode/tasks.json`, and consider committing
  `extensions.json`/`settings.json`. Tasks also use `"problemMatcher": []`; use
  `$msCompile` on Windows and `$gcc` on macOS so build errors appear in the
  Problems panel.
- Render verification (golden images) is Windows-only because of the MoltenVK
  swapchain readback validation errors documented in
  [`scripts/README.md`](../scripts/README.md). Render to an offscreen image
  with `TRANSFER_SRC` usage instead of reading back the swapchain image. This
  fixes the readback on MoltenVK and lets Mac changes get visual regression
  coverage.

### 3.12 Cross-platform native file dialogs (P2)

There are two separate Windows-only dialog implementations
([`FileDialog.cpp`](../tools/geometry_repl/FileDialog.cpp) and
[`FileOperations.cpp`](../tools/resource_editor/FileOperations.cpp)), and both
are no-op stubs on macOS. Replace both with a single shared helper, for example
in `tools/common/`, backed by
[nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended).
It is available through FetchContent and supports Win32 COM, Cocoa, and
GTK/portal. Until then, show a "not supported on this platform" message, or
fall back to a path text field, so that Mac users are not left with buttons
that do nothing.

### 3.13 One PowerShell host and script linting (P1)

- Standardize on `pwsh` 7: `winget install Microsoft.PowerShell` on Windows,
  and `brew install powershell` if Mac developers ever need to run a `.ps1`.
  Replace `powershell.exe`/`powershell` calls with `pwsh` (or with
  `(Get-Process -Id $PID).Path` so child processes use the current host), and
  add `#Requires -Version 7.0`.
- Add a CI job that runs `Invoke-ScriptAnalyzer -Recurse -Severity Error,Warning
  -ExcludeRule PSAvoidUsingWriteHost` together with the
  `PSUseCompatibleSyntax` settings used in 1.2. It finishes in under a minute
  and catches Windows script problems before a PC developer runs into them.

---

## 4. Suggested order of work

1. **Day 1 (quick fixes):** run the Windows checklist in 1.3 on a PC,
   fail-fast tool checks at the start of `verify`, and PSScriptAnalyzer in CI.
   The EOL rules, parallel macOS CTest, setup-time hooks, and SQLite archive
   hash have been implemented.
2. **Week 1:** `CMakePresets.json`, the Windows CI job, a clang-format version
   pinned in both workflows, and a platform command table in the AI
   instructions and skills.
3. **Week 2+:** smoke tests registered in CTest from `vde.toml`, dependency
   slimming and caching, compiler cache, offscreen render verification on
   macOS, and documentation cleanup.

Once presets and CTest-based smoke tests are in place, `build.ps1`,
`smoke-test.ps1`, and `macos.sh` become thin wrappers around the same
`cmake`/`ctest` commands, so the Windows and Mac workflows stop drifting
apart.
