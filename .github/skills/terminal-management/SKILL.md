---
name: terminal-management
description: Rules for running commands in the terminal and writing PowerShell scripts in the VDE project. Use this when running long commands, handling terminal output, or writing PowerShell scripts.
---

# Terminal and PowerShell Management

This skill encodes hard-won lessons about running commands and writing PowerShell scripts in VDE. Every rule here exists because ignoring it caused a real failure — see `docs/history/PROBLEMS_ONLY_IMPLEMENTATION_RETROSPECTIVE.md` for the full account.

## When to use this skill

- Running build, test, or smoke-test commands in the terminal
- Writing or editing PowerShell scripts
- Checking the output of a long-running command
- Diagnosing why a terminal command appeared to succeed but didn't

## Required: Handling Long-Running Commands

Smoke tests, full test suites, and rebuild commands can take 1–4 minutes. Getting this wrong means acting on incomplete output.

### Use generous timeouts or no timeout

For any command that might run longer than 30 seconds, use `timeout: 0` (no timeout). Never guess a short timeout for:
- `smoke-test.ps1` (runs ~29 executables, 2–4 minutes)
- `rebuild.ps1` (full clean + build, 30–90 seconds)
- `test.ps1` without `-Filter` (full suite, 10–30 seconds)

### Look for the summary line — never assume completion from partial output

Every VDE script prints an explicit final line. Do not consider a command finished until you see it:

| Script | Success marker | Failure marker |
|--------|---------------|----------------|
| `test.ps1` | `All tests PASSED` | `TESTS FAILED` |
| `smoke-test.ps1` | `All smoke tests PASSED!` | `SMOKE TESTS FAILED` |
| `build.ps1` | `Build succeeded` | `Build FAILED` |

If you don't see the marker, the command is still running or was truncated. Read more output.

### Prefer the log-file pattern for verification runs

When verifying that a fix works, redirect to a temp file instead of relying on terminal output. This avoids truncation and is immune to terminal session problems:

```powershell
$logFile = Join-Path $env:TEMP 'vde-verify.log'
& .\scripts\test.ps1 -ProblemsOnly *> $logFile
Get-Content $logFile -Tail 30
```

For smoke tests:
```powershell
$logFile = Join-Path $env:TEMP 'vde-smoke-verify.log'
& .\scripts\smoke-test.ps1 -ProblemsOnly *> $logFile
Get-Content $logFile -Tail 40
```

## Required: Terminal Session Health

### Never reuse a terminal after cancellation or timeout

When a foreground terminal command is interrupted (cancelled, timed out, Ctrl+C), the shared terminal session may be corrupted — dangling child processes, broken pipes, or a dead shell. Symptoms:
- Subsequent commands return stale output
- Commands fail silently or hang
- Exit codes don't match the visible output

**Rule:** After any interruption, do not trust the shared terminal. Run the next command in a fresh invocation or use a background terminal with `isBackground: true`.

### Background terminals for fire-and-forget commands

Use `isBackground: true` only when you don't need to see output inline (e.g., starting a long build while you continue editing). For verification commands where you must read the result, use foreground with `timeout: 0` or the log-file pattern.

## Required: AI-Friendly Output Filtering

When running tests or smoke tests for verification, always use `-ProblemsOnly`:

```powershell
.\scripts\test.ps1 -ProblemsOnly
.\scripts\smoke-test.ps1 -ProblemsOnly
```

This suppresses passing-test noise and emits only:
- `WARNING:` lines
- `FAILURE:` lines
- A final `PASS:` or `FAIL:` summary

This keeps output small enough to read in full and parse quickly.

## Required: Reading Truncated Output

When terminal output exceeds ~60KB, it is written to a sidecar file. The terminal will show a message like:

```
Output was truncated. Full output written to <path>
```

**You must read that file.** Do not assume the run succeeded because the visible terminal output showed no errors — the errors may be in the truncated portion. Read at minimum the tail:

```powershell
Get-Content -Path '<sidecar-path>' -Tail 40
```

## PowerShell Script Pitfalls

These cause failures that are invisible to code review but break at runtime.

### Encoding

PowerShell 5.1 (the Windows default) uses CP1252. Non-ASCII characters in script source files (em dashes, arrows, Greek letters, fancy quotes) cause `ParserError` at startup. Stick to plain ASCII in all `.ps1` files.

### Unsupported operators

PowerShell 5.1 does not support `?.` (null-conditional) or `??` (null-coalescing). These throw parse errors, not runtime errors — the entire script fails to load. Use explicit `if ($null -ne $x)` checks instead.

### Reserved automatic variables

These names are taken by PowerShell and will silently conflict: `$matches`, `$input`, `$args`, `$error`, `$null`, `$true`, `$false`, `$_`, `$PSItem`. Never use them as local variable names.

### Collection mutation in loops

`foreach ($key in $hashtable.Keys)` throws `InvalidOperationException` if the loop body modifies the hashtable. Snapshot first:

```powershell
$keys = @($hashtable.Keys)
foreach ($key in $keys) {
    $hashtable.Remove($key)  # safe
}
```

### Parallel logic across scripts

When the same helper function appears in multiple scripts, write one version first, then copy it exactly. After copying, diff the two implementations to catch accidental divergence. If the shared logic is non-trivial, consider extracting it into a shared `.ps1` file that both scripts dot-source.

### Structured output contracts

When a script has a structured output mode (like `-ProblemsOnly`), audit **all** output paths — not just the main result, but also the script's own `Write-Warning`, `Write-Error`, and helper-function messages. Every visible line must conform to the contract.

## Evaluating Subagent Suggestions About Scripts

When a subagent review suggests changes to pattern matching, regex, or output filtering in scripts:

1. Ask: "What real failure does this catch that the existing pattern misses?" If the answer is hypothetical, reject the suggestion.
2. If you accept the suggestion, re-run the **full** test or smoke-test suite before committing — not just one or two tests. A filter that works on 3 tests may break on 29.
3. Prefer structural patterns (`^\s*error\b`, `\berror:`, `\bfailed to\b`) over bare words (`\bFailure\b`, `\berror\b`) when scanning program output. Bare words match help text, enum names, and informational messages.
