---
name: performance-branch-review
description: 'Review all changes on the currently checked-out branch only for CPU, GPU, memory, I/O, synchronization, and scalability regressions.'
agent: agent
---

Follow the shared [branch review workflow](../skills/branch-review/SKILL.md).

Review the branch exclusively from a performance perspective. Look for regressions in hot paths, per-frame work, algorithmic complexity, allocations and copies, cache behavior, I/O, startup or loading, synchronization and lock contention, CPU-GPU stalls, redundant Vulkan state or resource updates, excessive command submission, avoidable shader cost, memory growth, and behavior that scales poorly with scene or resource counts.

Require a plausible workload and cost mechanism for every finding. Quantify frequency, complexity, allocation volume, transfer size, synchronization point, or expected scaling where the diff provides enough evidence. Distinguish measured regressions from evidence-backed risks and recommend the smallest useful benchmark or profiler check when measurement is needed. Do not report micro-optimizations, style, security, architecture, or graphics-quality concerns without a material performance consequence.

In `Focused Summary`, state the expected performance risk, affected CPU/GPU/I/O domains, likely scaling limits, and whether measurement or remediation should block merge.