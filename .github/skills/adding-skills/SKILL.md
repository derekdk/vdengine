---
name: adding-skills
description: Guide for creating new skills in the VDE project. Use this when recognizing a repeatable process with expensive-to-acquire knowledge, or when the user identifies a deficiency in how the AI is completing work.
---

# Adding Skills to VDE

Skills are the primary mechanism for encoding knowledge that an AI agent would otherwise have to rediscover every session. This skill defines when to create them, how to structure them, and what to do when the user identifies a gap.

## When to create a skill

### Trigger 1 — Repeatable process with expensive-to-acquire knowledge

Create a skill when you find yourself doing non-trivial research to complete a task that will recur. Signs that a skill is warranted:

- You read multiple files to understand a convention before being able to act
- You made mistakes on the first attempt that required correction (encoding bugs, wrong API, wrong file location)
- The task required understanding how several parts of the project fit together
- You had to look up project-specific patterns that are not obvious from the code

Examples from this project: knowing that `/d1reportTime` floods stdout (discovered through a failed run), knowing that PS 5.1 breaks on non-ASCII source characters, knowing that `configureInputScriptFromArgs` must be called before `setWorkingDirectoryToExecutablePath`.

**Rule:** If you had to learn something the hard way, encode it so you don't have to learn it again.

### Trigger 2 — User identifies a deficiency

When the user tells you that you completed work incorrectly, incompletely, or against their expectations, create a skill immediately to close the loop. Do not just fix the immediate issue and move on — capture the corrective rule so it applies to all future work.

Examples:
- User: "You didn't verify the scripts before telling me they worked" → create `completing-work` skill
- User: "You broke the build when you added that file" → add a rule to the relevant skill requiring build verification
- User: "That's not how we name things in this project" → update the `writing-code` skill

**Rule:** Every user correction is evidence of a missing or incomplete skill. Treat it as a requirement to update the knowledge base.

---

## How to create a skill

### Step 1 — Create the skill directory and file

```
.github/skills/<skill-name>/SKILL.md
```

The directory name must be lowercase with hyphens. The file is always named `SKILL.md`.

### Step 2 — Write the SKILL.md

Use this exact frontmatter format — the `---` block is required:

```markdown
---
name: <skill-name>
description: <one sentence: what this skill is about and when to use it>
---

# <Title>

<One paragraph explaining what this skill covers and why it exists.>

## When to use this skill

- <bullet list of situations>

## <Section>

<Content>
```

**What makes a good skill:**

- **Specific to this project.** General programming knowledge does not belong here. Only include things that are specific to VDE's conventions, structure, tooling, or history.
- **Actionable.** Each section should tell the agent what to *do*, not just what to know. Prefer step-by-step instructions over prose descriptions.
- **Includes failure modes.** If there are known ways to get this wrong, document them explicitly. A skill is most valuable when it prevents the mistakes that motivated creating it.
- **Does not duplicate other skills.** If relevant knowledge already lives in another skill, reference it rather than copying it.

### Step 3 — Register the skill in copilot-instructions.md

Open `.github/copilot-instructions.md` and add an entry to the `## Available Skills` section:

```markdown
### <skill-name>
**Purpose:** <same as the description field in the frontmatter>  
**Use when:** <one or two sentences describing the triggering situation>  
**Location:** `.github/skills/<skill-name>/SKILL.md`
```

Keep the list in alphabetical order.

### Step 4 — Verify the skill is reachable

Confirm both files exist:

```powershell
Test-Path .github\skills\<skill-name>\SKILL.md   # must be True
Test-Path .github\copilot-instructions.md         # check the entry was added
```

---

## What belongs in a skill vs. a memory

The project has two knowledge-persistence mechanisms. Use the right one:

| | Skill | Memory |
|---|---|---|
| **Scope** | Project-wide, applies to all future work | Single fact, typically file-specific |
| **Lifespan** | Permanent (committed to repo) | Retained across sessions but can expire |
| **Format** | Structured Markdown with sections | One short sentence with citation |
| **Best for** | Multi-step processes, conventions, failure modes, domain knowledge | Specific facts: file locations, command names, ordering constraints |
| **Triggered by** | Repeatable process OR user-identified deficiency | Useful fact discovered during a task |

When in doubt: if the knowledge would fill a paragraph and would be useful to re-read before starting a task, it belongs in a skill. If it is a single actionable fact (e.g., "always call X before Y"), it belongs in a memory — or in both.

---

## Updating existing skills

When a task reveals that an existing skill is incomplete or incorrect:

1. Read the existing skill with `read_file`.
2. Add or correct the relevant section.
3. Do not delete existing content unless it is factually wrong — add an "Additional notes" or "Known pitfalls" section if needed.

When the user explicitly corrects agent behavior, add a dedicated section to the relevant skill titled **"## Required: <topic>"** so the rule is unmissable.
