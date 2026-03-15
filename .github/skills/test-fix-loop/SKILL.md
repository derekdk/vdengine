---
name: test-fix-loop
description: Guide for reproducing failures and iterating quickly with VDE's build and test scripts. Use this when running a tight test-fix-build-test loop before widening back to full verification.
---

# Running the Test-Fix Loop in VDE

This skill exists so the agent can move through VDE's red-green cycle efficiently without skipping the repo's required verification gates. It captures the repo-specific defaults, filtered test workflow, and the point where a targeted loop must widen back out to the full unit and smoke suites.

## When to use this skill

- A unit test is failing and you need the smallest reproducible run
- You are fixing code and need a fast rebuild-and-rerun loop
- You need to decide between a VS Code task, `build.ps1`, and `test.ps1 -Build`
- You need the exact GoogleTest suite/test name before iterating

## Required: Default choices

- Default to `Ninja` + `Debug`. That matches the repo scripts and the existing VS Code tasks.
- Use `.\scripts\test.ps1` for unit tests. Do not use `.\scripts\build-and-test.ps1` unless the user explicitly asks for the legacy wrapper.
- Use the `scripts: build` and `scripts: test` tasks for full runs.
- Use `.\scripts\test.ps1 -Filter ...` for the inner loop. The stock VS Code `scripts: test` task does not accept an ad-hoc filter.

## Required: Tight loop

1. Reproduce the failure with the narrowest filter you can name.
2. If the test executable is missing or your edits changed compiled code, rebuild.
3. Re-run the same filtered test after each edit until it passes.
4. Expand to the surrounding suite or related pattern.
5. Run the full unit suite.
6. Run smoke tests when the change touches runtime behavior.

### Reproduce one test or suite

```powershell
.\scripts\test.ps1 -Filter "Suite.TestName"
.\scripts\test.ps1 -Filter "Suite.*"
.\scripts\test.ps1 -Filter "Suite.TestA:Suite.TestB"
```

Use the exact failing test name when possible. Colon-separated filters are the fastest way to keep a small repro for multiple failures.

### Rebuild + rerun in one command

```powershell
.\scripts\test.ps1 -Build -Filter "Suite.TestName"
```

Use this when you changed C++ code and want the shortest correct loop. `test.ps1` will call `build.ps1` first and then run only the filtered tests.

### Separate build from test when compiler output matters

```powershell
.\scripts\build.ps1
.\scripts\test.ps1 -Filter "Suite.TestName"
```

Use separate commands when you expect compile or link failures and need clean build output instead of interleaving build and test logs.

## Getting exact test names

`test.ps1` does not provide a list-tests flag. After a successful build, invoke the test executable directly when you need exact suite or test names.

For Ninja:

```powershell
.\build_ninja\tests\vde_tests.exe --gtest_list_tests
```

For MSBuild Debug:

```powershell
.\build\tests\Debug\vde_tests.exe --gtest_list_tests
```

Use the listed names to build an exact `-Filter`.

## When to widen back out

A filtered pass is only the inner loop. Do not stop there.

- After the targeted test passes, run the surrounding suite or a related pattern.
- Before declaring a bug fix or feature complete, run the full unit suite with `.\scripts\test.ps1`.
- If the change affects examples, tools, rendering, input, windowing, audio, physics, launcher flows, or other runtime behavior, follow with `.\scripts\smoke-test.ps1`.
- If follow-up edits are required after a build failure, test failure, or review finding, restart the loop from the filtered repro and widen back out again.

## Common failure modes

- Running the full suite on every edit instead of using `-Filter`, which slows iteration and discourages frequent reruns.
- Running only the filtered test and skipping the final full unit suite.
- Using the legacy `build-and-test.ps1` wrapper for new work instead of the current `build.ps1` and `test.ps1` scripts.
- Falling back to raw `cmake` or `ctest` even though the repo scripts already pick the correct build directory and load the VS Developer environment for Ninja builds.
- Using the `scripts: test` task for filtered runs even though it has no filter input.

## References

- `build-tool-workflows` skill - command reference and script parameters
- `fixing-bugs` skill - required verification sequence after a bug fix
- `adding-features` skill - required verification sequence after feature work
- `completing-work` skill - completion gates before telling the user a change is done
