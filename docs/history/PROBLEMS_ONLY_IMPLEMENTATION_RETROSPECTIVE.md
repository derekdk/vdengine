# `-ProblemsOnly` Implementation Retrospective

Date: 2026-03-14

## Task

Add a `-ProblemsOnly` switch to `scripts/test.ps1` and `scripts/smoke-test.ps1` so that AI agents can quickly parse test results by receiving only failures, warnings, and a single PASS/FAILURE summary line.

## Problems Encountered

### 1. Smoke-Test Terminal Timeout (User-Reported)

**What happened:** After launching the full smoke-test suite via terminal, the agent checked output too soon and concluded work was done before the suite finished. The user had to intervene: *"The test is still running, you didn't wait long enough."*

**Root cause:** The smoke-test suite runs ~29 executables sequentially, each with a several-second timeout. Total wall-clock time is 2–4 minutes. The agent used a terminal timeout that was too short and misinterpreted truncated output as completion.

**Fix for the future:**
- For long-running scripts (smoke tests, full test suites), either use **no timeout** (`timeout: 0`) or a conservatively large timeout (300000+ ms).
- Alternatively, redirect output to a log file (`*> $logFile`) and poll the log after the process finishes, which avoids terminal-truncation ambiguity entirely.
- Never assume a test suite finished based on partial output — look for the script's explicit final summary line (e.g., `All smoke tests PASSED!` or `SMOKE TESTS COMPLETE`).

---

### 2. Shared Terminal Dying After Cancelled Runs

**What happened:** After cancelling or timing out a foreground terminal command, subsequent commands in the same shared terminal failed or returned stale output. Attempts to re-use the terminal or check process status were unreliable.

**Root cause:** The VS Code shared terminal (`isBackground: false`) is a single persistent session. When a long-running command is interrupted (timeout, Ctrl+C), the shell state can become inconsistent — dangling child processes, broken pipes, or a killed shell session.

**Fix for the future:**
- After any terminal timeout or cancellation, **do not trust the shared terminal**. Start fresh commands in a new invocation or use a background terminal.
- For verification runs that must complete fully, redirect to a temp log file and run non-interactively:
  ```powershell
  $logFile = Join-Path $env:TEMP 'vde-smoke-verify.log'
  & .\scripts\smoke-test.ps1 *> $logFile
  Get-Content $logFile -Tail 30
  ```
- This pattern decouples "did the command finish?" from terminal session health.

---

### 3. Over-Broad Failure Detection Regex (Review-Induced Regression)

**What happened:** The initial `$outputFailurePattern` regex checked for specific failure markers like `ASSERT FAILED` and `TEST FAILED`. A subagent code review recommended broadening it to also match `\bFailure\b` and `\berror\b`. After applying this, 7 of 29 smoke tests became false-positive failures because:
- VDE executables print help text like `"F - Report failure"` which matched `\bFailure\b`
- Vulkan validation layer info messages contained `"error"` in non-error contexts

**Root cause:** Bare English words like "error" and "failure" appear frequently in non-error output (help text, enum names, informational messages). A word-boundary regex is not selective enough for stdout/stderr scanning.

**Fix for the future:**
- When scanning captured program output for failures, use **structural patterns** rather than bare words:
  - `^\s*error\b` (error at line start — compiler/tool convention)
  - `\berror:` (error followed by colon — diagnostic format)
  - `\bfailed to\b` (action failure phrase)
  - `assert failed` / `test failed` (explicit test markers)
- Always **test regex changes against the full suite** before committing. A filter that works on 3 tests may break on 29.
- Be skeptical of review suggestions that broaden match patterns — ask "what real failure would this catch that the existing pattern misses?" If the answer is hypothetical, don't broaden.

The final working pattern:
```powershell
$outputFailurePattern = '(?i)(assert failed|test failed|unknown file: Failure|\[\s*failed\s*\]|^\s*error\b|\berror:|\bfailed to\b|\bfatal\b|\bexception\b)'
```

---

### 4. Unprefixed Warning/Error Messages in ProblemsOnly Mode

**What happened:** In `-ProblemsOnly` mode, the script's own warning messages (e.g., "No executables found matching filter") were printed by `Write-Warn` without the `WARNING:` prefix. This broke the structured-output contract that every visible line would start with `WARNING:`, `FAILURE:`, or `PASS:`.

**Root cause:** The initial implementation only added prefix logic to the problem-line extraction from test output, but forgot that the script itself emits messages through `Write-Warn` and `Write-Err` helper functions.

**Fix for the future:**
- When designing a structured output mode, identify **all output paths** — not just the main test output, but also the script's own diagnostic/warning/error messages.
- The fix was to modify `Write-Warn` and `Write-Err` to auto-prefix with `WARNING:` / `FAILURE:` when `$ProblemsOnly` is active, ensuring no untagged lines escape.

---

### 5. Inconsistent Function Signatures Between Scripts

**What happened:** The same helper functions (`Write-ProblemLines`, `Get-ProblemLines`) were implemented in both `test.ps1` and `smoke-test.ps1` but with slightly different signatures and defaults:
- `Write-ProblemLines` in smoke-test.ps1 accepted a `-Prefix` parameter; test.ps1 did not.
- `Get-ProblemLines` used `$MaxLines = 40` in one script and `$MaxLines = 20` in the other.

**Root cause:** The functions were written independently for each script rather than extracted from a shared template. Slight differences crept in during implementation.

**Fix for the future:**
- When adding the same logic to multiple scripts, write one version first, then **copy it exactly** to the second script, making only intentional differences.
- If the shared logic grows, consider extracting it into a shared `.ps1` module file that both scripts dot-source.
- During review, explicitly diff the parallel implementations to catch accidental divergence.

---

### 6. Pre-Existing Documentation Bug Found During Review

**What happened:** The review subagent discovered that `scripts/README.md` had 4 instances stating `MSBuild (default)` as the generator, when the actual default is Ninja. This was a pre-existing documentation bug, not caused by the current changes.

**Root cause:** The README was written or updated when MSBuild was the default, and was never corrected after the default changed to Ninja.

**Impact:** Minor — documentation inaccuracy. Fixed as part of this PR since the README was already being edited.

**Lesson:** When editing documentation files, scan for adjacent inaccuracies — they're cheap to fix and improve overall doc quality.

---

## Summary of Mitigation Strategies

| Problem | Strategy |
|---|---|
| Terminal timeout too short | Use `timeout: 0` or redirect to log file |
| Shared terminal corruption | Don't reuse terminals after cancellation; use log files |
| Over-broad regex | Use structural patterns, test against full suite |
| Missing output prefixes | Audit all output paths when adding structured mode |
| Inconsistent parallel code | Write once, copy exactly, diff to verify |
| Stale documentation | Scan adjacent content when editing docs |
