---
name: adding-features
description: Guide for implementing new features in the VDE project, including mandatory build/test/smoke verification and subagent review before completion.
---

# Adding Features in VDE

This skill defines the required process for implementing new functionality in the VDE codebase. Feature work must fit the existing architecture, include the right tests and docs, and pass the same verification gates as bug fixes before it can be declared complete.

## When to use this skill

- Adding a new engine, API, rendering, input, audio, physics, tooling, or editor capability
- Extending an example or demo with new user-visible behavior
- Implementing an enhancement request that changes behavior rather than only correcting incorrect behavior
- Adding a new component, subsystem hook, or workflow that creates new supported functionality

## Required: Verification Gates

For feature work that changes code, the verification order is mandatory:

1. Build the project and read the output.
2. Run the full unit test suite and confirm it passes.
3. Run smoke tests and confirm the runtime surface still works.
4. Run a subagent code review on the verified diff.

Do not skip or reorder these steps. A clean review is not a substitute for build/test/smoke verification, and a green build is not a substitute for review.

---

## Required Workflow

All steps are mandatory and must be followed in order.

### Step 1 - Define the feature before touching code

Before making edits, gather enough context to answer:

- **What problem is the feature solving?** Read the user request carefully and identify the concrete behavior that needs to exist when the work is done.
- **What is the acceptance criteria?** Check `API-DESIGN.MD`, `docs/`, existing examples, or the relevant skill for the expected user-facing or API-facing behavior.
- **Where should the change live?** Use `grep_search`, `semantic_search`, and `read_file` to find the existing ownership boundaries. Do not create a parallel pattern if the repo already has one.
- **What else must change with it?** Identify required tests, smoke coverage, examples, docs, or launch workflows before writing code.

If the requested feature is underspecified, implement the smallest coherent slice that satisfies the request instead of inventing extra surface area.

### Step 2 - Design the smallest correct implementation

Decide how the feature will fit the existing architecture before editing.

- Reuse existing patterns, resource lifetimes, and ownership models.
- Prefer extending the correct subsystem over introducing one-off helpers.
- If you are adding a public API, match surrounding naming, error handling, and lifecycle conventions.
- If you are adding a new component or subsystem, also consult the `add-component` skill.
- If you are changing examples or tools, also consult `writing-examples` or `writing-tools` as applicable.

### Step 3 - Implement the feature

Make focused edits that directly support the requested behavior.

- Add or update unit tests alongside the implementation when the behavior can be validated there.
- Update docs, examples, or tool UI text when the feature changes supported behavior or discoverability.
- If you add a new runnable path, executable behavior, or user workflow, make sure it is represented in smoke coverage.
- Avoid unrelated refactors in the same change unless they are required to land the feature safely.

Refer to the `writing-code` skill for code organization, naming, and CMake integration expectations.

### Step 4 - Build

Run the build immediately after applying the feature.

Use the VS Code task (preferred):
- **Task:** `scripts: build`

Or via terminal:
```powershell
.\scripts\build.ps1
```

Read the full build output. Look for:
- Compilation errors in the files you edited
- Warnings newly introduced by your change
- Linker errors caused by missing registrations, declarations, or sources

If the build fails, fix it before proceeding.

### Step 5 - Run unit tests

After a successful build, run the full unit test suite:

Use the VS Code task (preferred):
- **Task:** `scripts: test`

Or via terminal:
```powershell
.\scripts\test.ps1
```

Read the output and confirm:
- All previously passing tests still pass
- Any new or updated tests covering the feature pass
- The final line reads `All tests PASSED!`

If tests fail, fix the issue before proceeding.

### Step 6 - Run smoke tests

After unit tests pass, run the smoke tests to confirm the new feature did not break examples or tools at runtime:

Use the VS Code task (preferred):
- **Task:** `scripts: smoke-test`

Or via terminal:
```powershell
.\scripts\smoke-test.ps1
```

All 26 executables (23 examples + 3 tools) must pass. If any smoke test fails, treat it as a regression introduced by the feature work.

### Step 7 - Spawn a subagent code review

After build, unit tests, and smoke tests pass, spawn a subagent to review the changes. This is mandatory.

For this repo, prefer the `Explore` subagent and ask it to review the verified diff for:
- Correctness of the new behavior
- Edge cases and incomplete handling
- Regressions in adjacent systems
- API and architecture consistency with surrounding code
- Missing tests, smoke coverage, or docs

The prompt must identify every changed file and describe the change in each file. Request written findings, or `No issues found` if the change is clean.

Example subagent prompt structure:
```
You are performing a code review for feature work in the VDE (Vulkan Display Engine) project.

The following files were modified:
- <file path>: <description of change>

For each change, review:
1. Correctness - does the implementation provide the requested behavior?
2. Edge cases - are there states, inputs, or lifecycle cases not handled?
3. Regressions - could the change break adjacent functionality?
4. Consistency - does the change follow surrounding VDE conventions and architecture?
5. Coverage - are tests, smoke coverage, and docs updated where needed?

Return a written review with specific findings for each changed file, or `No issues found` if the change is clean.
```

If the review identifies issues, address them and then re-run build, unit tests, and smoke tests before launching the review again.

### Step 8 - Announce completion

Only after all of the following are true:
- Build succeeded with zero errors
- All unit tests passed
- All smoke tests passed
- The feature's tests/docs/examples were updated where needed
- Subagent code review returned no unresolved issues

...may you tell the user the feature is complete.

---

## Common Failure Modes

### Building extra scope that was never requested

Do not turn a small enhancement into a redesign. Extra API surface, UI, or toggles create review and maintenance cost. Implement the narrowest complete behavior that satisfies the request.

### Adding behavior without updating tests or docs

A feature that only exists in code but is not covered by tests, examples, or docs is not actually integrated into the project.

### Stopping after a targeted check instead of the required verification sequence

Running one example manually or executing one targeted test is not enough. Feature work must still go through build, full unit tests, smoke tests, and subagent review.

### Forgetting to repeat verification after follow-up edits

If you make more edits after a build failure, a test failure, or subagent review feedback, re-run the full verification sequence. Earlier green runs do not cover later edits.

### Declaring success because the code "looks right"

This is explicitly prohibited. See the `completing-work` skill. The only acceptable completion criteria are a successful verification run plus a clean subagent review.

---

## References

- `completing-work` skill - mandatory verification checklist before declaring work done
- `build-tool-workflows` skill - detailed build and test command reference
- `create-tests` skill - how to add unit tests when coverage is missing
- `writing-code` skill - conventions to follow when editing engine code
- `add-component` skill - guidance for adding new engine subsystems or classes
- `writing-examples` and `writing-tools` skills - guidance for feature work in demos and tools
