# VDE (Vulkan Display Engine) - AI Coding Agent Instructions

## Project Overview
VDE is a lightweight Vulkan-based 3D rendering engine for rapid prototyping and game development. It abstracts Vulkan's complexity while maintaining flexibility for advanced use cases.

**Core Architecture:** 
Two-layer design with low-level rendering 
Engine uses modern C++20 with RAII for all Vulkan resources.

Always uses the provided build scripts and tools for building, testing, and running examples to ensure consistency and reliability.

Never use the CMake tools extension directly or bypass the build and tool scripts.

## Available Skills

The following skills provide domain-specific knowledge for working with VDE:

### using-api
**Purpose:** Guide for using the VDE Game API to create games, demos, applications, and examples.  
**Use when:** Creating applications with the high-level Game API, working with Scene/Entity/Resource system, or implementing gameplay features.  
**Skip when:** Making small edits to an existing scene where the API patterns are already established.  
**Location:** `.github/skills/using-api/SKILL.md`

### add-component
**Purpose:** Guide for adding new components to the VDE engine.  
**Use when:** Creating new classes, systems, or modules in the engine.  
**Skip when:** Modifying existing components or working only at the Game API level.  
**Location:** `.github/skills/add-component/SKILL.md`

### adding-features
**Purpose:** Guide for implementing new features in the VDE project, including mandatory build/test/smoke verification and subagent review before completion.  
**Use when:** Implementing enhancements or new capabilities in engine code, examples, tools, or APIs that change supported behavior.  
**Location:** `.github/skills/adding-features/SKILL.md`

### adding-skills
**Purpose:** Guide for creating new skills in the VDE project.  
**Use when:** Recognizing a repeatable process with expensive-to-acquire knowledge, or when the user identifies a deficiency in how the AI completed work — create a skill immediately to close the loop.  
**Location:** `.github/skills/adding-skills/SKILL.md`

### architecture
**Purpose:** Quick map of engine layout and key systems.  
**Use when:** You need to understand where files should be located, orient to the repo, or wire new features into the correct directories.  
**Skip when:** You already know the target directory from context.  
**Location:** `.github/skills/architecture/SKILL.md`

### ai-verification
**Purpose:** Canonical guide for AI agents running build/test/smoke verification using verify.ps1 and reading logs/verify-latest.log with read_file.  
**Use when:** Running end-to-end verification after a feature or fix, or any time logs need to be captured without workspace-external temp files.  
**Location:** `.github/skills/ai-verification/SKILL.md`

### build-tool-workflows
**Purpose:** Build and test workflows for the VDE project.  
**Use when:** Building the project or running tests.  
**Location:** `.github/skills/build-tool-workflows/SKILL.md`

### create-tests
**Purpose:** Guide on how to create tests in this project.  
**Use when:** Asked to create unit tests.  
**Location:** `.github/skills/create-tests/SKILL.md`

### fixing-bugs
**Purpose:** Guide for fixing bugs in the VDE project, including required build/test verification and mandatory subagent code review after every fix.  
**Use when:** Diagnosing or resolving any defect, regression, crash, or incorrect behavior in engine code, examples, or tools.  
**Location:** `.github/skills/fixing-bugs/SKILL.md`

### vulkan-patterns
**Purpose:** Vulkan patterns and common tasks for the VDE engine.  
**Use when:** Working with Vulkan resources, buffers, textures, or descriptors.  
**Skip when:** The change doesn't touch Vulkan-level code (e.g., pure gameplay logic or UI).  
**Location:** `.github/skills/vulkan-patterns/SKILL.md`

### writing-code
**Purpose:** Comprehensive guide for writing code in the VDE engine.  
**Use when:** Need information about coding conventions, file organization, CMake integration, or best practices.  
**Skip when:** Making edits to existing files where conventions are already established.  
**Location:** `.github/skills/writing-code/SKILL.md`

### writing-examples
**Purpose:** Guide for writing example programs in VDE.  
**Use when:** Creating demo or example applications that showcase engine features.  
**Location:** `.github/skills/writing-examples/SKILL.md`

### writing-tools
**Purpose:** Guide for creating asset creation tools in VDE.  
**Use when:** Creating tools that support both interactive GUI mode and scriptable batch mode.  
**Location:** `.github/skills/writing-tools/SKILL.md`

### imgui-integration
**Purpose:** Guide for integrating Dear ImGui with VDE applications.  
**Use when:** Adding debug UI, tools, or overlay interfaces to VDE games and examples.  
**Skip when:** The application already has ImGui set up and you are editing existing UI code.  
**Location:** `.github/skills/imgui-integration/SKILL.md`

### scripted-input
**Purpose:** Guide for using scripted input automation in VDE games, examples, and tools.  
**Use when:** Adding test automation, smoke tests, automated demos, or CI/CD integration.  
**Location:** `.github/skills/scripted-input/SKILL.md`

### smoke-testing
**Purpose:** Guide for running smoke tests and interpreting the results.  
**Use when:** Running, debugging, or extending automated smoke tests for examples and tools.  
**Skip when:** Only running unit tests or the change doesn't affect any runnable executable.  
**Location:** `.github/skills/smoke-testing/SKILL.md`

### render-verify
**Purpose:** Guide for VDE's render verification system — golden-image comparison using NVIDIA FLIP.  
**Use when:** Adding render verification to an example, running `render-verify.ps1`, capturing or updating golden images, or debugging a FLIP comparison failure.  
**Location:** `.github/skills/render-verify/SKILL.md`

### test-fix-loop
**Purpose:** Guide for reproducing failures and iterating quickly with VDE's build and test scripts.  
**Use when:** Running a tight test-fix-build-test loop for a failing test before widening back to full verification.  
**Location:** `.github/skills/test-fix-loop/SKILL.md`

### terminal-management
**Purpose:** Rules for running commands in the terminal and writing PowerShell scripts in the VDE project.  
**Use when:** Running long-running commands (smoke tests, full builds), handling terminal output or truncation, writing or editing PowerShell scripts, or diagnosing terminal session issues.  
**Location:** `.github/skills/terminal-management/SKILL.md`

### 2d-games
**Purpose:** Guide for creating 2D games, demos, and examples with the VDE API.  
**Use when:** Building 2D physics, sprite-based, or side-view applications. Setting up Camera2D, physics arenas, or choosing between manual and engine-powered collision detection.  
**Location:** `.github/skills/2d-games/SKILL.md`

### completing-work
**Purpose:** Checklist and rules for verifying that scripts, tools, and workflows actually work before announcing a task is complete.  
**Use when:** Finishing any task that involves writing or modifying scripts, build tools, automation, or any executable artifact — before telling the user the work is done.  
**Location:** `.github/skills/completing-work/SKILL.md`

### common-sense
**Purpose:** Decision-making principles for planning, designing, and implementing features on the user's behalf.  
**Use when:** Making design choices, planning features, evaluating tradeoffs, or reviewing your own work before presenting it.  
**Location:** `.github/skills/common-sense/SKILL.md`

## Testing Decision Tree

| Need | Skill |
|------|-------|
| Run full verification (all gates) | `ai-verification` |
| Run existing tests (interactive) | `build-tool-workflows` |
| Fix a failing test | `test-fix-loop` |
| Write a new unit test | `create-tests` |
| Write a smoke test | `scripted-input` + `smoke-testing` |
| Add render verification to an example | `render-verify` |
| Capture or update golden images | `render-verify` |
| Verify before declaring done | `completing-work` + `ai-verification` |
