---
name: security-branch-review
description: 'Review all changes on the currently checked-out branch only for security vulnerabilities, unsafe trust boundaries, and exploitable misuse.'
agent: agent
---

Follow the shared [branch review workflow](../skills/branch-review/SKILL.md).

Review the branch exclusively from a security perspective. Look for concrete vulnerabilities involving untrusted input, file and path handling, command or process execution, memory safety, integer overflow, resource exhaustion, permissions, secrets, dependency changes, unsafe deserialization, race conditions with security impact, and Vulkan or native API misuse that can corrupt memory or cross a trust boundary.

For each finding, describe the attack or misuse path, required attacker capability, affected boundary, and likely confidentiality, integrity, or availability impact. Distinguish exploitable vulnerabilities from defense-in-depth opportunities. Do not report general correctness, style, organization, graphics quality, or performance concerns unless they create a concrete security impact.

In `Focused Summary`, state the branch's security risk level, the trust boundaries reviewed, and whether any finding should block merge.