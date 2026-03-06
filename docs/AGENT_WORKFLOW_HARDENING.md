# Agent Workflow Issues, Fixes, and Hardening Recommendations

## Summary

During the OS stress demo task, a workflow gap was identified in how agent work was being completed and reported.

The expected workflow for runnable/code changes is:

1. Build the project or affected target.
2. Run unit tests.
3. Run smoke tests for the changed executable or broader runtime surface when applicable.
4. Confirm the expected artifact or runtime outcome exists.
5. Spawn a subagent to perform code review.
6. If the review finds issues and code is changed again, repeat the same verification sequence before re-review.

The agent initially completed work without following that sequence in the correct order.

## Issues Discovered

### 1. Verification happened too late in the workflow

The agent did not consistently treat build, unit tests, and smoke tests as mandatory gates before code review and final completion.

Impact:
- Runnable changes could be declared complete before full runtime verification.
- The workflow depended too much on the agent remembering the order instead of the skills enforcing it.

### 2. Example workflow guidance was incomplete

The example-writing guidance required building and running examples, but it did not explicitly hand work off to the final verification gate that also requires tests, smoke tests, and subagent review.

Impact:
- New example work could be treated as complete after a successful build or a manual run.
- Smoke-test and review requirements were easy to under-apply.

### 3. Re-verification after review-driven edits was underspecified

The workflow guidance did not consistently say that if the subagent review leads to more edits, the agent must re-run build, unit tests, and smoke tests before another review pass.

Impact:
- A passing build/test/smoke run from an earlier revision could be incorrectly treated as still valid after later edits.

### 4. Generic workflow wording was too easy to misread

Some skill text used broad phrases like "build and tests" or described verification in a generic way that did not explicitly name smoke tests or enforce the order strongly enough.

Impact:
- Agents could interpret the workflow loosely.
- Review could happen before runtime verification, or smoke tests could be skipped after follow-up edits.

## Proposed Solutions

### Solution 1. Make the required order explicit in the final verification skill

The final verification skill must define a strict sequence for code and executable changes:

1. Build
2. Unit tests
3. Smoke tests when applicable
4. Artifact/outcome confirmation
5. Subagent code review

This removes ambiguity and makes it clear that subagent review is a gate after verification, not a substitute for it.

### Solution 2. Add an explicit completion handoff for example work

The example-writing skill should explicitly instruct the agent to invoke the final verification skill before declaring completion.

That handoff should state that for example work the agent must:

1. Build and confirm the example executable exists.
2. Run the unit test suite.
3. Run the example smoke test, plus broader smoke coverage when shared runtime behavior changed.
4. Only then run subagent review.

### Solution 3. Require full re-verification after review findings

If the subagent review identifies issues and the agent changes code again, the workflow must require another full pass of:

1. Build
2. Unit tests
3. Smoke tests
4. Re-review

This prevents stale verification from being reused across revisions.

### Solution 4. Replace ambiguous phrases with explicit wording

Phrases like "build and tests" should be replaced with exact wording that names:

- build
- unit tests
- smoke tests
- subagent review

This makes the workflow auditable and reduces the chance that an agent skips runtime validation.

## Changes Applied

The following skill files were updated to enforce the correct behavior:

### `.github/skills/completing-work/SKILL.md`

Updated to:
- Define the required order for buildable and runnable changes.
- State that subagent review must happen only after build, unit tests, and smoke tests.
- Require the same sequence to be repeated after review-driven edits.
- Clarify that completion must not be declared after smoke tests but before review.

### `.github/skills/writing-examples/SKILL.md`

Updated to:
- Add a completion handoff section.
- Require example work to pass build, unit tests, smoke tests, and then subagent review.
- Explicitly route example tasks through the final verification skill before completion is announced.

### `.github/skills/fixing-bugs/SKILL.md`

Updated to:
- Explicitly require build, unit tests, and smoke tests before subagent review.
- Require build, unit tests, and smoke tests to be re-run after review-driven edits.

## Hardening Recommendations

The skill changes improve behavior, but they still rely on the agent loading and following the right skills. To make the workflow more robust, additional hardening is recommended.

### 1. Add a workspace-level instruction for executable/code changes

Add a workspace instruction that says any change affecting engine code, examples, tools, tests, launch flows, or runtime behavior must invoke the final verification workflow before the final answer is sent.

Expected benefit:
- Makes the requirement always-on instead of only skill-driven.
- Reduces the chance that a task bypasses the correct skill.

### 2. Add a guardrail that blocks completion without verification evidence

Introduce a hook, prompt gate, or agent-mode constraint that prevents the final completion message unless the agent has:

1. Built successfully
2. Run unit tests successfully
3. Run smoke tests successfully when required
4. Performed a subagent review after those checks

Expected benefit:
- Converts workflow from guidance into enforcement.
- Prevents premature completion messages.

### 3. Standardize when full smoke suite vs targeted smoke test is required

Document a stronger rule for smoke scope:

- Targeted smoke test is acceptable for isolated new example work.
- Full smoke suite is required when shared engine/runtime behavior is touched.

Expected benefit:
- Makes smoke-test coverage decisions more consistent.
- Reduces under-testing of shared behavior.

### 4. Consider a dedicated completion checklist prompt or custom agent

Create a reusable prompt or custom agent dedicated to final verification that always performs:

1. Build
2. Unit tests
3. Smoke tests
4. Artifact confirmation
5. Subagent review
6. Re-run loop if findings exist

Expected benefit:
- Reduces repeated workflow drift across tasks.
- Makes completion behavior uniform across agents and tasks.

## Recommended Standard Going Forward

For any runnable or buildable change in this repository, the standard should be:

1. Implement the change.
2. Build and read the output.
3. Run unit tests and read the output.
4. Run smoke tests and read the output.
5. Confirm the artifact or runtime result exists.
6. Run subagent code review on the verified diff.
7. If review requires edits, repeat from step 2.
8. Only then announce completion.

This should be treated as mandatory repository policy, not a best-effort suggestion.