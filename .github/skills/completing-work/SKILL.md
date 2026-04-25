---
name: completing-work
description: Checklist and rules for completing work in the VDE project. Use this when finishing any task that involves scripts, tools, or automated workflows to ensure they are verified before declaring success.
---

# Completing Work in VDE

This skill defines the required verification steps before an AI agent may tell the user that a task is done. Skipping verification and announcing completion based only on "the code looks right" is a common failure mode — this skill exists to prevent it.

## When to use this skill

- After writing or modifying any script (PowerShell, batch, shell, Python, etc.)
- After writing or modifying any build, test, or automation workflow
- After adding a new tool, game, example, or utility that can be executed
- Any time you are about to tell the user "it's done" or "it's working"

## The Core Rule

**Do not tell the user that something is working unless you have run it and read the output yourself.**

Reasoning about whether code is correct is not the same as running it. Scripts can fail for many reasons that are invisible to static inspection: encoding issues, PowerShell version incompatibilities, environment assumptions, file path problems, race conditions, or logic errors that only appear at runtime. The only way to know a script works is to execute it and confirm the output.

## Required Verification Workflow

### Required order for code and executable changes

If you changed code that affects a buildable or runnable artifact (engine code, tests, examples, games, tools, launch flows, or build/test workflows), the verification order is:

1. Build the project or affected target and read the output.
2. Run unit tests and confirm they pass.
3. Run smoke tests when the change affects examples, games, tools, rendering, input, windowing, launch flows, or other runtime behavior.
4. Confirm the expected artifact or runtime outcome exists.
5. Run all linters and confirm no new failures.
6. Only then run a subagent code review.
7. If the review causes more edits, repeat the same verification sequence before re-review.

Do not reverse this order. A subagent review is not a substitute for build/test/smoke/lint verification, and verification is not a substitute for code review.

### Preferred: run all gates via VS Code tasks

For final verification, use the `scripts: verify` task — never call `verify.ps1` directly. The task ensures consistent environment setup and matches what CI runs:

```
run_task("scripts: verify", workspaceFolder="c:\\...\\vdengine")
```

Then read the results with `read_file` (do **not** rely on terminal streaming output):
```
read_file("logs/verify-latest.log", startLine=1, endLine=80)
```

This writes to `logs/verify-latest.log` inside the workspace, so `read_file` works directly without permission prompts or temp-file management. See the `ai-verification` skill for the full workflow and parameter reference.

> **Why tasks, not scripts?** `build-tool-workflows` designates VS Code tasks as the canonical invocation method for AI agents. Calling scripts directly bypasses the task runner and produces inconsistent behavior. Always use `run_task` for standard verification gates.

### Step 1 — Run the script or command

Execute the script using the terminal. Do not skip this step even if you are confident the code is correct.

```powershell
# Example: always run after creating or editing a script
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\my-script.ps1
```

### Step 2 — Check the exit code

A zero exit code is necessary but not sufficient. Always check it explicitly.

```powershell
# Run and capture exit code
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\my-script.ps1
echo "Exit code: $LASTEXITCODE"
```

If the exit code is non-zero, treat the task as **not done** and diagnose the failure before proceeding.

### Step 3 — Read and verify the output

Read the actual terminal output. Look for:
- Error messages or warnings that indicate failure
- Missing expected output (e.g., a "Report saved to:" line that never appeared)
- Partial execution (e.g., the build ran but the post-build analysis did not)
- Output that was truncated to a file — use `read_file` to inspect it

When output is large (>60KB), the tool writes it to a sidecar file. **Read that file.** Do not assume the run succeeded because the terminal didn't show an error; the actual error may be in the portion of output that was truncated.

### Step 4 — Confirm the expected artifact or outcome exists

If the script is supposed to produce a file, list the directory and confirm the file is present. If it is supposed to produce output in a specific format, spot-check the content.

```powershell
# Example: confirm a benchmark JSON was written
Get-ChildItem benchmarks\*.json | Select-Object Name
```

### Step 5 — Run all linters

Run the `scripts: lint` task and confirm no new failures are introduced:

```
run_task("scripts: lint", workspaceFolder="c:\\...\\vdengine")
```

Linters run in order: clang-format, GLSL shader validation, cppcheck, clang-tidy. Each linter is skipped if its tool is not installed, so a clean run on a workstation with partial tooling is still useful. A lint failure is a blocking issue — do not proceed to subagent review until lint is clean.

### Step 6 — Run a subagent review (mandatory)

Before declaring completion, launch a subagent to review the full set of changes.

Required behavior:
- Run a review-focused subagent (for this repo, use `Explore`) against the current diff.
- Ask it to prioritize bugs, regressions, risky behavior changes, and missing tests.
- Treat this as a required gate, not an optional best practice.

### Step 7 — Fix findings and re-review until clean

If the subagent reports issues:

1. Fix the issues.
2. Re-run the relevant verification commands in the same order: build, unit tests, smoke tests when applicable, lint, then read the outputs.
3. Launch the subagent review again on the updated diff.

Repeat until the subagent reports no material issues.

### Step 8 — Only then announce completion

Once you have: run the command, seen a zero exit code, read the output, confirmed the expected artifact, linters passed, and completed a clean subagent review loop, you may tell the user the task is complete.

For executable/code changes in VDE, interpret this as: build passed, unit tests passed, smoke tests passed when applicable, linters passed, artifacts were confirmed, and only then was a clean subagent review completed on the verified diff.

Acceptable:
> "Both benchmark runs completed. The JSON files are in `benchmarks/`. The comparison shows a 3.2% variance between runs, which is expected noise."

Not acceptable:
> "I've fixed the iterator bug. The script should work now."  
> "The script looks correct. It's ready to use."

Also not acceptable:
> "Build and tests passed, so I'm done." (without lint or subagent review)  
> "Build, tests, and lint passed, so I'm done." (without subagent review)

---

## Special Cases

### Iterative debugging

When fixing a bug in a script:

1. Apply the fix.
2. **Re-run the script from scratch.** Do not assume one fix addresses all remaining problems. A script that was failing for reason A may also fail for reason B once A is resolved.
3. Continue re-running until you observe a clean, successful execution with the expected output.
4. Only after a clean run may you say the bug is fixed.

The failure mode to avoid: applying several fixes across multiple iterations, then announcing "fixed" after the last edit without running the script one final time.

### Subagent review loop

Subagent review is mandatory before completion and may require multiple passes.

Minimum loop:
1. Run subagent review on the current changes.
2. Fix reported issues.
3. Re-run verification commands.
4. Run subagent review again.
5. Stop only when review is clean.

Do not skip this loop just because manual inspection looks correct.

### Scripts that take a long time

For scripts with long runtimes (e.g., full rebuild benchmarks that take 20-30 seconds):

- Use `isBackground: false` with a generous `timeout` (e.g., 300000ms = 5 minutes) so the run completes before you read the result.
- If the output is too large for inline display, it will be written to a file — read the tail of that file to find the completion summary and any error messages.
- Check for the summary/completion block that appears at the end of the output rather than assuming the script finished because the timeout did not trigger.

### Multiple scripts in sequence

If a task involves creating multiple scripts (e.g., a benchmark script and a comparison script), each must be run and verified individually before the task as a whole is announced as complete.

Do not:
- Run script A, confirm it works, then write script B and announce both are done without running B.
- Run a simplified version of a script to verify part of it while skipping the full end-to-end path.

### Examples, games, tools, and other runnable artifacts

When you add or modify an example, game, tool, launcher flow, or any other runnable executable behavior:

1. Build the project and confirm the target was produced.
2. Run the unit test suite unless the task is explicitly docs-only.
3. Run smoke tests. At minimum, run the smoke test that covers the changed executable. If the change touches shared engine/runtime behavior, prefer the broader smoke suite.
4. After those checks are green, run the subagent review.
5. If the review causes more edits, repeat build, unit tests, smoke tests, and then review again.

This ordering is mandatory. Do not declare completion after a successful smoke test, and do not run the review before runtime verification is complete.

### New PowerShell scripts

See the `terminal-management` skill for PowerShell 5.1 encoding pitfalls, unsupported operators, reserved variables, and collection mutation traps.

Always run a new PowerShell script at least once end-to-end before declaring it complete.
