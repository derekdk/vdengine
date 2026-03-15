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

## Required: Tight test loop

When the bug is represented by a failing unit test, use the `test-fix-loop` skill for the inner red-green cycle. Reproduce with the narrowest `-Filter`, iterate there until green, and only then widen back out to the full unit and smoke verification required by this skill.

---

## Required Workflow

### Step 1 — Understand the bug before touching code

Before making any edits, gather enough context to understand:

- **What is the incorrect behavior?** Read the bug description carefully. If it references specific files or line numbers, read those sections.
- **What is the correct behavior?** Consult the API design doc (`API-DESIGN.MD`), the docs (`docs/`), or the relevant skill if the expected behavior is documented.
- **Where is the defect?** Use `grep_search`, `semantic_search`, or `read_file` to locate the relevant code. Do not guess at file locations.
- **Are there related tests?** Search for existing tests that cover the affected code. A failing or missing test is a signal to also fix or add test coverage.

### Step 2 — Apply the fix

Make the minimum change needed to correct the behavior. Avoid refactoring unrelated code in the same edit.

Refer to the `writing-code` skill for conventions (naming, file organization, CMake integration) that must be followed when editing or adding code.

### Step 3 — Verify and announce completion

**Follow the `completing-work` skill** for the mandatory verification sequence: build → unit tests → smoke tests → subagent review. Do not skip or reorder any gate. Do not announce completion until every gate passes.

If the subagent review or a build/test failure causes follow-up edits, re-run the full verification sequence from the beginning.

---

## Common Failure Modes

- **Fixing the symptom, not the root cause:** Always ask *why* the bad state occurred, not just what to do when it occurs.
- **Introducing a regression:** When fixing a shared function, verify the fix is correct for all call sites.
- **Not re-verifying after follow-up edits:** Re-run the full `completing-work` sequence after every round of edits.
- **Declaring success because the code "looks right":** Explicitly prohibited. See the `completing-work` skill.

## References

- `completing-work` — mandatory verification gates
- `test-fix-loop` — focused inner loop for filtered test iteration
- `build-tool-workflows` — build and test command reference
- `create-tests` — adding unit tests when coverage is missing
- `writing-code` — conventions for engine code
