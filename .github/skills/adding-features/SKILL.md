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

## Required Workflow

### Step 1 — Define the feature before touching code

Before making edits, gather enough context to answer:

- **What problem is the feature solving?** Read the user request carefully and identify the concrete behavior that needs to exist when the work is done.
- **What is the acceptance criteria?** Check `API-DESIGN.MD`, `docs/`, existing examples, or the relevant skill for the expected user-facing or API-facing behavior.
- **Where should the change live?** Use `grep_search`, `semantic_search`, and `read_file` to find the existing ownership boundaries. Do not create a parallel pattern if the repo already has one.
- **What else must change with it?** Identify required tests, smoke coverage, examples, docs, or launch workflows before writing code.

If the requested feature is underspecified, implement the smallest coherent slice that satisfies the request instead of inventing extra surface area.

### Step 2 — Design the smallest correct implementation

- Reuse existing patterns, resource lifetimes, and ownership models.
- Prefer extending the correct subsystem over introducing one-off helpers.
- If you are adding a public API, match surrounding naming, error handling, and lifecycle conventions.
- If you are adding a new component or subsystem, also consult the `add-component` skill.
- If you are changing examples or tools, also consult `writing-examples` or `writing-tools` as applicable.

### Step 3 — Implement the feature

Make focused edits that directly support the requested behavior.

- Add or update unit tests alongside the implementation when the behavior can be validated there.
- Update docs, examples, or tool UI text when the feature changes supported behavior or discoverability.
- If you add a new runnable path, executable behavior, or user workflow, make sure it is represented in smoke coverage.
- Avoid unrelated refactors in the same change unless they are required to land the feature safely.

Refer to the `writing-code` skill for code organization, naming, and CMake integration expectations.

### Step 4 — Verify and announce completion

**Follow the `completing-work` skill** for the mandatory verification sequence: build → unit tests → smoke tests → subagent review. Do not skip or reorder any gate. Do not announce completion until every gate passes.

---

## Common Failure Modes

- **Over-scoping:** Do not turn a small enhancement into a redesign. Implement the narrowest complete behavior that satisfies the request.
- **Missing test/doc updates:** A feature that only exists in code but is not covered by tests, examples, or docs is not integrated.
- **Stopping after a targeted check:** A single manual run or one targeted test is not enough. The full `completing-work` sequence is required.
- **Not re-verifying after follow-up edits:** If you make more edits after review feedback, re-run the full verification sequence.
- **Declaring success because the code "looks right":** Explicitly prohibited. See the `completing-work` skill.

## References

- `completing-work` — mandatory verification gates
- `build-tool-workflows` — build and test command reference
- `writing-code` — conventions for engine code
- `add-component` — adding new engine subsystems or classes
- `create-tests` — adding unit tests
- `writing-examples` / `writing-tools` — feature work in demos and tools
