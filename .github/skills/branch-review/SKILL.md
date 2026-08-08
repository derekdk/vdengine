---
name: branch-review
description: 'Shared read-only workflow for reviewing all changes on the currently checked-out Git branch. Use when a focused review prompt needs branch-base discovery, diff scoping, evidence standards, severity ordering, and concise review output.'
user-invocable: false
---

# Branch Review

Use this workflow as the common foundation for a review prompt. The invoking prompt defines the only review perspective to apply.

## Rules

- Perform a read-only review. Do not edit files, format code, or fix findings.
- Review changed code, tests, build files, shaders, scripts, assets, and documentation only when relevant to the invoking perspective.
- Do not report concerns outside the invoking prompt's perspective. Do not pad the result with generic review advice.
- Treat generated files, vendored dependencies, build output, and binary assets as out of scope unless the branch intentionally changes them and they materially affect the review perspective.
- Inspect enough surrounding code and call sites to verify each finding. A suspicious diff alone is not evidence of a defect.
- Report only actionable findings introduced or exposed by the branch. Do not report unrelated pre-existing issues.
- Do not run commands that modify the worktree. Run focused validation only when it materially confirms or rejects a suspected finding.

## Establish the Review Range

1. Identify the current branch and worktree state with `git branch --show-current` and `git status --short`.
2. Select the comparison branch in this order:
   - A base branch explicitly supplied by the user.
   - The remote default branch referenced by `refs/remotes/origin/HEAD`.
   - An existing `origin/main`, `origin/master`, `main`, or `master`, in that order.
3. If no trustworthy base can be identified, stop and ask the user for it. Never guess from commit dates.
4. Compute the branch point with `git merge-base HEAD <base>`.
5. Review committed branch changes from `<merge-base>...HEAD`.
6. Also review staged, unstaged, and relevant untracked files in the current worktree. Keep these local changes distinct from the committed branch diff and avoid reporting the same change twice.
7. Summarize the effective base, merge base, commit count, changed files, and local worktree state before evaluating findings.

## Review Method

1. Inventory the changed files and diff statistics, then read the complete diff.
2. Prioritize production behavior and public contracts. Use tests and documentation to infer intent and identify missing protection for risky changes.
3. Trace changed symbols to nearby callers, ownership boundaries, and resource lifetimes when needed to determine actual impact.
4. For each candidate finding, identify a concrete failure mode and the conditions that trigger it.
5. Discard speculative, stylistic, preference-only, and unverified findings.
6. Assign severity by impact and likelihood:
   - `Critical`: immediate, severe impact in expected use or release.
   - `High`: likely serious failure, vulnerability, corruption, or major regression.
   - `Medium`: meaningful defect under plausible conditions.
   - `Low`: limited but concrete defect or maintainability risk with a clear consequence.
7. Order findings by severity, then by confidence and impact.

## Required Output

Start with `Findings` and list findings first. Every finding must include:

- Severity and a concise title.
- A clickable changed-file reference with the narrowest useful line range.
- The concrete impact and triggering conditions.
- Why the branch change causes or exposes the issue.
- A focused remediation direction, without implementing it.

Then provide:

- `Focused Summary`: a concise assessment only from the invoking prompt's perspective, including the highest-risk theme and whether the branch is acceptable from that perspective.
- `Review Scope`: the base branch, merge base, committed range, local changes included, and any exclusions or validation limits.

If there are no findings, write `No findings.` under `Findings`; still provide the focused summary, scope, and any meaningful residual risk or test gap. Do not add a generic change summary unless it is necessary to explain review coverage.