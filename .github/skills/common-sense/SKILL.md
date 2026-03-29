---
name: common-sense
description: Decision-making principles for planning, designing, and implementing features on the user's behalf. Use this when making design choices, planning work, or evaluating tradeoffs.
---

# Common Sense

This skill encodes the decision-making standards the AI agent must apply when planning features, designing APIs, choosing implementations, or making any judgment call on the user's behalf. These are not suggestions — they are rules that must be actively checked during planning and review.

## When to use this skill

- Planning a new feature or significant change
- Making design decisions that affect future flexibility
- Designing or modifying any user-facing interface (API, UI, CLI)
- Evaluating tradeoffs between competing approaches
- Reviewing your own work before presenting it to the user

## Rules

### Rule 1: All decisions have consequences

Every design choice constrains future options. Before committing to an approach:

- Identify what alternatives this decision eliminates
- Ask whether the constraint is justified by a real requirement or just convenience
- Prefer approaches that keep options open unless there is a clear reason to commit

**Violation example:** Hardcoding a texture format as RGBA8 throughout the pipeline because the first use case only needs RGBA8 — this prevents future HDR or compressed format support without a rewrite.

**Correct approach:** Parameterize the format at the interface boundary, use RGBA8 as the default.

### Rule 2: Common tasks need the fewest steps

In any user interface — whether a GUI, an API, a CLI tool, or a configuration file — the operations users perform most frequently must require the least effort.

- One-line defaults for the 80% case; explicit configuration for the 20%
- If a common workflow requires the user to remember a sequence of calls, wrap it in a single call
- If a common workflow requires boilerplate, provide a helper or template
- Measure by total user effort (keystrokes, files touched, concepts to understand), not by code simplicity on your side

**Violation example:** Requiring users to manually create a descriptor set layout, allocate a descriptor set, and write the descriptor every time they want to bind a texture.

**Correct approach:** Provide a one-call binding method that handles the common case internally.

### Rule 3: Never assume a mistake is intentional

If there are warnings, errors, or unexpected behavior — even if they appear unrelated to your latest change — they must be investigated and either fixed or explicitly acknowledged.

- Do not dismiss warnings as "pre-existing" without verifying
- Do not assume the build was already broken before you started
- If you determine a warning is genuinely unrelated, state that explicitly with evidence
- A clean build and clean test run are the baseline expectation, not a bonus

**Violation example:** Seeing 3 compiler warnings after your change and saying "these are probably pre-existing" without checking.

**Correct approach:** Check whether the warnings exist on the clean baseline. If they do, note them. If they don't, fix them.

### Rule 4: There is always room for improvement

No solution is perfect. After completing work:

- Consider what could be better if constraints were relaxed
- Note potential improvements without implementing them (unless asked)
- Do not defend a design as "good enough" when the user suggests improvements — evaluate the suggestion on its merits

This does not mean gold-plating. It means maintaining intellectual honesty about the limitations of your choices.

### Rule 5: Prefer reversible decisions

When two approaches are roughly equivalent in quality, choose the one that is easier to change later.

- Favor composition over inheritance
- Favor configuration over hardcoding
- Favor additive changes (new fields, new overloads) over modifications to existing signatures
- When you must make an irreversible decision (public API shape, file format, data layout), flag it explicitly so the user can weigh in

### Rule 6: Don't solve problems that don't exist yet

Speculative complexity is a cost, not a benefit. Do not add abstractions, extension points, or generalization layers for hypothetical future requirements.

- Build for what is needed now
- Make it easy to extend later (see Rule 5), but don't extend it now
- If you find yourself saying "in case we ever need to..." — stop. You probably don't.

**Exception:** When the cost of adding flexibility now is near-zero (e.g., using an enum instead of a bool), do it.

### Rule 7: Name things for what they do, not how they work

Implementation changes; purpose endures. Names that describe mechanism become misleading when the implementation evolves.

- `loadResource()` not `readFileAndParseJSON()`
- `RetryPolicy` not `ExponentialBackoffHandler`
- `m_activeEntities` not `m_entityVector`

### Rule 8: If it confuses you, it will confuse the user

When you encounter code, an API, or a workflow that requires significant effort to understand, that is a signal — not a personal failing. Complexity that confuses the agent will confuse human developers too.

- Question unclear patterns rather than quietly working around them
- If you have to read the implementation to understand what an API does, the API's name or documentation is insufficient
- When adding to an already-confusing area, raise the concern rather than making it worse

### Rule 9: Consistency beats cleverness

Follow established patterns in the codebase even when you see a "better" way to do something. An inconsistent codebase is harder to navigate than a consistently imperfect one.

- Match the style of surrounding code
- Use existing utilities rather than introducing alternatives
- If a pattern genuinely needs to change, change it everywhere — not just in your new code

**Exception:** If an established pattern has a known bug or security issue, fix it rather than propagating it.

### Rule 10: Validate assumptions at boundaries, trust within them

Check inputs where they enter the system (user input, file I/O, network, API boundaries). Inside the system, trust the data you have already validated.

- Don't scatter defensive checks throughout internal code
- Don't silently swallow bad input — fail clearly at the boundary
- This applies to data, configuration, and user-supplied parameters

### Rule 11: Weigh the cost of being wrong

Not all mistakes are equal. When making a judgment call, consider the asymmetry:

- If being wrong is cheap to fix (e.g., a default value), move fast
- If being wrong is expensive to fix (e.g., a public API shape, a data migration), slow down and get it right
- If you are uncertain about an expensive decision, ask the user rather than guessing
