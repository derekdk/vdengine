---
name: review-code-organization
description: 'Review all changes on the currently checked-out branch only for architecture, ownership, dependency, API, and maintainability problems.'
agent: agent
---

Follow the shared [branch review workflow](../skills/branch-review/SKILL.md).

Review the branch exclusively from a code organization perspective. Evaluate whether responsibilities live in the correct module and abstraction layer; dependencies point in the intended direction; public API changes are coherent; ownership and lifetimes are clear; naming and file placement match repository conventions; duplicated logic creates divergence risk; and tests, CMake wiring, documentation, or registration are organized with the behavior they support.

Report only structural problems with a concrete maintenance, correctness, extensibility, or discoverability consequence. Do not report formatting, personal style preferences, hypothetical abstraction opportunities, security, runtime performance, or visual rendering concerns unless they directly demonstrate an organizational defect.

In `Focused Summary`, state the branch's organizational coherence, the most important dependency or ownership theme, and whether structural issues should block merge.