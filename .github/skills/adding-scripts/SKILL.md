---
name: adding-scripts
description: Guide for adding new PowerShell scripts to the VDE project. Use this when creating any new script in scripts/ to ensure every required documentation and registration step is completed.
---

# Adding Scripts to VDE

Every new script added to `scripts/` must be registered in four places so that it is discoverable by developers, AI agents, and VS Code tasks. Missing any of these steps leaves the script invisible to tooling and inconsistent with the rest of the project.

## When to use this skill

- Creating a new `.ps1` script in the `scripts/` directory
- Wrapping a new workflow in a script (orchestration, scaffolding, utility)
- Being asked to make an existing workflow scriptable

---

## Checklist — complete every step in order

| # | Location | What to do |
|---|----------|-----------|
| 1 | `scripts/<name>.ps1` | Write the script with standard header and conventions |
| 2 | `scripts/README.md` | Add to the overview table + add a detailed usage section |
| 3 | `scripts/help.ps1` | Add to the relevant section(s) |
| 4 | `.vscode/tasks.json` | Add a VS Code task (for scripts with no required args) |
| 5 | `.github/skills/build-tool-workflows/SKILL.md` | Add to the task table, script overview, and parameters reference |

---

## Step 1 — Write the script

### Required header block

Every script must begin with a comment block that follows this pattern exactly:

```powershell
# VDE <Title> Script
# <One-line description of what it does.>
#
# AI AGENTS: <Any special guidance for AI agents using this script. Omit section if not applicable.>
#
# Usage:
#   .\scripts\<name>.ps1                          # Default usage
#   .\scripts\<name>.ps1 -Param Value            # Common variant
#
# Key output files (if any):
#   logs/...   -- description
```

### Required param block conventions

```powershell
param(
    [ValidateSet("MSBuild", "Ninja")]
    [string]$Generator = "Ninja",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
    # ... additional params
)

$ErrorActionPreference = "Stop"
```

**Rules:**
- `$ErrorActionPreference = "Stop"` must appear immediately after the `param` block — never omit it.
- Always include `-Generator` and `-Config` if the script invokes build artifacts or other scripts that accept them, so callers can control the build system and configuration.
- Use `[switch]` for boolean flags rather than `[bool]` parameters.
- Use `[ValidateSet(...)]` for enum-style string parameters.
- Paths must always be constructed from `$PSScriptRoot` or `$MyInvocation.MyCommand.Path`, never hard-coded.

### Output conventions

- Scripts that produce pass/fail results should emit a clear `PASS:` or `FAILURE:` final line so callers can parse them with `-ProblemsOnly` patterns.
- Scripts that produce log files should write to `logs/` in the repo root (not temp directories).
- Use `Write-Host` with `-ForegroundColor` for user-facing output. Never use `Write-Output` for status messages — it pollutes pipeline output.

### Repo root resolution

```powershell
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
```

Or equivalently (when `[CmdletBinding()]` is present):

```powershell
$RepoRoot = Split-Path -Parent $PSScriptRoot
```

---

## Step 2 — Update scripts/README.md

Add a row to the **Scripts Overview** table near the top:

```markdown
| **<name>.ps1** | <Short description> | `.\scripts\<name>.ps1` |
```

Then add a full **detailed usage section** after the other script entries (maintain alphabetical or logical order):

```markdown
### <name>.ps1

<One paragraph description.>

**Syntax:**
```powershell
.\scripts\<name>.ps1 [-Param Value] [...]
```

**Parameters:**
- `-Param` - Description (default: value)

**Examples:**
```powershell
# Common use case
.\scripts\<name>.ps1

# Variant
.\scripts\<name>.ps1 -Param Value
```
```

---

## Step 3 — Update scripts/help.ps1

Add the script to the `Write-Title` section that best matches its purpose:

| Script type | Section |
|-------------|---------|
| Build, test, or verification | `BUILD SCRIPTS` |
| Code quality / linting | `LINT & FORMAT SCRIPTS` |
| Profiling / benchmarking | `BENCHMARK SCRIPTS` |
| Scaffolding / code generation | under `EXAMPLES` heading |

```powershell
Write-Cmd '.\scripts\<name>.ps1' '<Short description>'
```

Also add one or two lines to the `COMMON TASKS` section if the script is frequently used:

```powershell
Write-Cmd '.\scripts\<name>.ps1' '<Typical use case description>'
Write-Cmd '.\scripts\<name>.ps1 -Param Value' '<Variant description>'
```

---

## Step 4 — Add a VS Code task to .vscode/tasks.json

Add a task only when the script can usefully run with default parameters (no required args). Insert it near other related tasks, following the existing format:

```json
{
  "label": "scripts: <name>",
  "type": "shell",
  "command": "powershell",
  "args": [
    "-NoProfile",
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    "${workspaceFolder}/scripts/<name>.ps1"
  ],
  "group": "<test|build>",
  "presentation": {
    "reveal": "always",
    "panel": "shared"
  },
  "problemMatcher": []
},
```

**Group conventions:**

| Script purpose | `group` value |
|----------------|--------------|
| Builds the project | `"build"` (or `{"kind": "build", "isDefault": true}`) |
| Runs tests or verification | `"test"` |
| Other (clean, format, tools) | omit `group` |

> **Scaffolding scripts** (`new-example.ps1`, `new-tool.ps1`, `new-game.ps1`) use `"${input:...}"` variables and do not follow the pattern above. Do not use that pattern for non-scaffolding scripts.

---

## Step 5 — Update .github/skills/build-tool-workflows/SKILL.md

Add the script in three places inside the skill:

### 5a — Task table (Quick Start section)

```markdown
| `scripts: <name>` | <One-line description> |
```

### 5b — Script overview table (Script Commands section)

```markdown
| `<name>.ps1` | <Short description> | `.\scripts\<name>.ps1` |
```

### 5c — Script parameters reference

Add a block after the nearest related script's parameter block:

```markdown
**<name>.ps1**
- `-Param` - Description (default: value)
- `-Generator` - Ninja (default) or MSBuild
- `-Config` - Debug (default) or Release
```

---

## Failure modes to avoid

- **Forgetting `$ErrorActionPreference = "Stop"`** — causes silent failures when called as a child process (e.g. from `verify.ps1`).
- **Hard-coding paths** — breaks when the repo is cloned to a different location.
- **Skipping the VS Code task** — AI agents use `run_task` with task IDs; a script without a task cannot be invoked by agents without running raw PowerShell.
- **Skipping `build-tool-workflows` skill update** — AI agents consult this skill to discover what scripts exist; an unregistered script will be overlooked.
- **Skipping `scripts/README.md`** — human developers rely on this for documentation; missing entries make the script invisible to them.
- **Skipping `scripts/help.ps1`** — `.\scripts\help.ps1` is the quick reference that both humans and AI agents run; missing entries make the script undiscoverable at the terminal.
