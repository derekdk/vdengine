---
name: review-solution
description: 'Review the overall solution quality of all changes on the currently checked-out branch, focusing on architecture and design rather than low-level implementation details.'
agent: agent
---

Follow the shared [branch review workflow](../skills/branch-review/SKILL.md).

Review the branch exclusively from a high-level, solution-oriented perspective. First identify the overall goals and themes of the update, then evaluate whether the chosen approach solves the intended problem coherently and fits the project's architecture and direction.

Concentrate on:

- The soundness of the overall design and architectural approach.
- Whether responsibilities and abstraction boundaries are appropriate.
- The suitability and maturity of the algorithms, including meaningful complexity concerns.
- Reusability, maintainability, extensibility, and consistency with established project patterns.
- Important tradeoffs, systemic risks, missing capabilities, or simpler approaches that materially affect the solution.

Do not perform a line-by-line review. Ignore naming, formatting, local style, minor implementation details, and isolated code-level defects unless they reveal a broader design problem or materially undermine the solution. Findings must be solution-level and explain their impact on the update as a whole.

In `Focused Summary`, state the solution's themes and goals, assess the maturity of its design and algorithms, and conclude whether it demonstrates strong high-level engineering judgment.