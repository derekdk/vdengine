# Lint Workflow Refactor Plan

## Goals

- Fix the current `clang-tidy` execution path so the local lint task stops failing because of shell behavior instead of actionable findings.
- Split the monolithic lint workflow into reusable per-linter scripts while keeping `lint.ps1` as the single entry point for the common case.
- Add targeted linting for changed files so regular local verification can run static analysis on the current delta instead of the full repository.
- Preserve a clear way to run the full lint suite across the entire repository on a slower cadence.

## Current Problems

1. `scripts/lint.ps1` owns tool discovery, file discovery, and linter execution in one script, so targeted runs and per-linter reuse are awkward.
2. `scripts/verify.ps1` does not run lint at all, so the regular verification path misses static analysis.
3. The current `clang-tidy` stage runs inside the VS Code `scripts: lint` task's Windows PowerShell host, which is brittle when `clang-tidy` writes warning text to stderr.
4. The current `clang-tidy` stage only scans `src/**/*.cpp`, which is too narrow for a deliberate full-lint pass and too broad for a fast inner loop.

## Decision

Split the lint workflow into dedicated user-facing sub-scripts and keep `scripts/lint.ps1` as the orchestrator.

### Why this is better than keeping one monolithic script

- The repo already has a separate `format.ps1`, so moving the other linter stages to dedicated scripts follows an established pattern instead of inventing a new one.
- The top-level `lint.ps1` can stay optimized for the common one-command workflow.
- `verify.ps1` can call `lint.ps1` in targeted mode without needing to understand individual linter details.
- Each heavy linter can evolve independently without making the top-level wrapper harder to read.

### Scope of the split

- Keep `format.ps1` as the formatting sub-script.
- Add dedicated scripts for shader linting, cppcheck, and clang-tidy.
- Add a shared helper module for changed-file resolution and compile database lookup.

## Implementation Plan

### 1. Shared lint helper module

Add a PowerShell module that provides:

- changed-file discovery from git working tree state
- file filtering by extension and repo area
- compile database lookup for `clang-tidy`
- path normalization helpers shared by the linter scripts

### 2. Dedicated linter scripts

Add:

- `scripts/lint-shaders.ps1`
- `scripts/lint-cppcheck.ps1`
- `scripts/lint-clang-tidy.ps1`

Each script should:

- run independently with sensible defaults for a full-repo pass
- accept an optional explicit file list so `lint.ps1` can drive targeted runs
- own its tool-specific flags and failure handling

### 3. Targeted format support

Extend `scripts/format.ps1` so it can check or format an explicit file list instead of always scanning the entire repository.

### 4. Orchestrator update

Refactor `scripts/lint.ps1` so it:

- keeps the existing common default of a full lint run when no targeting parameters are supplied
- adds a `-ChangedOnly` mode for local delta-based linting
- calls `format.ps1`, `lint-shaders.ps1`, `lint-cppcheck.ps1`, and `lint-clang-tidy.ps1` instead of embedding each stage directly
- preserves the current `-Quick` and `-Fix` behavior

### 5. Regular verification integration

Update `scripts/verify.ps1` so it runs lint after render verification.

- Default verify behavior should run targeted lint on changed files.
- Add an opt-in full-lint mode for slower, semi-regular full verification.
- Add a skip switch so users can intentionally omit lint when needed.

### 6. Documentation and discoverability

Update:

- `scripts/README.md`
- `scripts/help.ps1`
- `.vscode/tasks.json`
- `.github/skills/build-tool-workflows/SKILL.md`
- `.github/skills/linting/SKILL.md`

The docs should clearly distinguish between:

- targeted lint for regular verification and local iteration
- full lint for broader periodic checks

## Targeted Linting Rules

- Changed-file mode should inspect the current git delta in the working tree.
- If no relevant files changed for a linter stage, that stage should be skipped cleanly.
- `clang-tidy` should lint changed translation units directly and broaden header changes to a reasonable nearby translation-unit set instead of trying to solve whole-program dependency tracking.

## Full Lint Rules

- `scripts/lint.ps1` with no targeting flags remains the full lint entry point.
- The dedicated `clang-tidy` script should be able to lint all compile-database user translation units, not only `src/**/*.cpp`.
- This full path is the semi-regular, slower check intended for explicit runs rather than every quick local verification loop.

## Acceptance Criteria

The change is complete when all of the following are true:

1. `scripts/lint.ps1` still works as the single common entry point.
2. The lint workflow is split into dedicated sub-scripts for shaders, cppcheck, and clang-tidy.
3. `scripts/lint.ps1 -ChangedOnly` runs only the relevant lint work for changed files.
4. `scripts/verify.ps1` includes a lint stage that defaults to targeted linting.
5. There is still a clear, documented way to run a full-repo lint pass.
6. The updated scripts are registered in docs, help output, tasks, and workflow skills.
7. The modified scripts run successfully in the current environment, and the final diff passes review.