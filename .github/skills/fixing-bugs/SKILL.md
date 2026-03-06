---
name: fixing-bugs
description: Guide for fixing bugs in the VDE project. Use this when diagnosing and resolving defects, regressions, or incorrect behavior in engine code, examples, or tools.
---

# Fixing Bugs in VDE

This skill defines the required process for resolving bugs in the VDE codebase. It ensures that fixes are correct, verified by the build and test suite, and reviewed before being declared complete.

## When to use this skill

- Fixing a defect, regression, or incorrect behavior reported by the user
- Correcting a crash, logic error, or API contract violation
- Resolving a failing unit test or smoke test
- Addressing code review feedback that identifies a bug

---

## Required Workflow

### Step 1 — Understand the bug before touching code

Before making any edits, gather enough context to understand:

- **What is the incorrect behavior?** Read the bug description carefully. If it references specific files or line numbers, read those sections.
- **What is the correct behavior?** Consult the API design doc (`API-DESIGN.MD`), the docs (`docs/`), or the relevant skill if the expected behavior is documented.
- **Where is the defect?** Use `grep_search`, `semantic_search`, or `read_file` to locate the relevant code. Do not guess at file locations.
- **Are there related tests?** Search for existing tests that cover the affected code. A failing or missing test is a signal to also fix or add test coverage.

Do not skip this step. Applying a fix without understanding the root cause often produces a patch that masks the symptom without resolving the defect, or introduces a new bug nearby.

### Step 2 — Apply the fix

Make the minimum change needed to correct the behavior. Avoid refactoring unrelated code in the same edit — that makes review harder and increases the risk of introducing new bugs.

For multiple independent fixes in the same session, use `multi_replace_string_in_file` to apply them in one batch rather than sequentially.

Refer to the `writing-code` skill for conventions (naming, file organization, CMake integration) that must be followed when editing or adding code.

### Step 3 — Build

Run the build immediately after applying the fix. A fix that does not compile is not a fix.

Use the VS Code task (preferred):
- **Task:** `scripts: build`

Or via terminal:
```powershell
.\scripts\build.ps1
```

Read the full build output. Look for:
- Compilation errors in the files you edited
- Warnings that were not present before your change
- Linker errors caused by missing symbols

If the build fails, fix the error before proceeding. Do not move on to testing with a broken build.

### Step 4 — Run unit tests

After a successful build, run the full unit test suite:

Use the VS Code task (preferred):
- **Task:** `scripts: test`

Or via terminal:
```powershell
.\scripts\test.ps1
```

Read the output and confirm:
- All previously passing tests still pass (no regressions)
- Any test that was failing due to the bug now passes
- The final line reads `All tests PASSED!`

If tests fail, diagnose and fix before proceeding. Do not declare the bug fixed while tests are red.

### Step 5 — Run smoke tests

After unit tests pass, run the smoke tests to confirm no runtime regressions in examples or tools:

Use the VS Code task (preferred):
- **Task:** `scripts: smoke-test`

Or via terminal:
```powershell
.\scripts\smoke-test.ps1
```

All 26 executables (23 examples + 3 tools) must pass. If any smoke test fails, treat it as a regression introduced by the fix.

### Step 6 — Spawn a subagent code review

After build, unit tests, and smoke tests pass, spawn a subagent to perform a code review of the changes. This is mandatory — do not skip it.

The subagent prompt must:
- Identify every file that was changed and the exact lines modified
- Ask the subagent to review for: correctness, edge cases, consistency with surrounding code, adherence to VDE conventions, and any new bugs introduced
- Request that the subagent return a written review with specific findings (or "no issues found" if clean)

Example subagent prompt structure:
```
You are performing a code review for a bug fix in the VDE (Vulkan Display Engine) project.

The following files were modified:
- <file path>: <description of change>

For each change, review:
1. Correctness — does the fix actually resolve the stated bug?
2. Edge cases — are there inputs or states that the fix does not handle?
3. Regressions — could the change break any adjacent functionality?
4. Consistency — does the change follow the conventions in surrounding code?
5. New bugs — does the fix introduce any new defects?

Return a written review with specific findings for each changed file, or "No issues found" if the change is clean.
```

Read the subagent's review output. If the subagent identifies issues, address them and re-run build, unit tests, and smoke tests before announcing completion.

### Step 7 — Announce completion

Only after all of the following are true:
- Build succeeded with zero errors
- All unit tests passed
- All smoke tests passed
- Subagent code review returned no unresolved issues

...may you tell the user the bug is fixed.

---

## Common Failure Modes

### Fixing the symptom, not the root cause

A silent `return` replaced with a warning log looks like a fix but the underlying invalid state was never addressed. Always ask: *why* did the bad state occur, not just *what* to do when it occurs.

### Introducing a regression in a related code path

When fixing a function shared by multiple callers, verify that the fix is correct for all call sites, not just the one reported in the bug.

### Forgetting to re-run tests after a follow-up edit

If the subagent review or a build error causes you to make a second round of edits, re-run build, unit tests, and smoke tests from scratch. Do not assume the first passing run covers the second set of changes.

### Declaring success based on "the code looks right"

This is explicitly prohibited. See the `completing-work` skill. The only acceptable verification is a successful build + test run that you have read the output of yourself.

---

## References

- `completing-work` skill — mandatory verification checklist before declaring any task done
- `build-tool-workflows` skill — detailed build and test command reference
- `create-tests` skill — how to add unit tests when test coverage is missing for the buggy code
- `writing-code` skill — conventions to follow when editing engine code
