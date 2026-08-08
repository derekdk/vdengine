---
name: graphics-branch-review
description: 'Review all changes on the currently checked-out branch only for Vulkan rendering correctness, visual regressions, GPU resource use, and shader or pipeline issues.'
agent: agent
---

Follow the shared [branch review workflow](../skills/branch-review/SKILL.md).

Review the branch exclusively from a graphics perspective. Examine Vulkan synchronization, barriers and layouts, render-pass and dynamic-rendering contracts, descriptor and pipeline compatibility, attachment formats, image and buffer lifetimes, coordinate and color-space handling, depth and blending behavior, shader interfaces and math, camera and transform behavior, resize and swapchain handling, batching or draw ordering when visually significant, and asset sampling or texture correctness.

For each finding, identify the affected render path, the GPU or scene conditions that trigger it, and the expected visual artifact, validation-layer error, device-dependent behavior, or rendering failure. Use existing render verification, smoke tests, validation output, or focused runtime checks when they can materially confirm a concern. Do not report general architecture, security, or performance issues unless they directly cause rendering incorrectness or a visible graphics regression.

In `Focused Summary`, state the graphics correctness risk, affected rendering stages or features, device or scene sensitivity, and whether visual or validation evidence should block merge.