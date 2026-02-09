# VDE (Vulkan Display Engine) - AI Coding Agent Instructions

## Project Overview
VDE is a lightweight Vulkan-based 3D rendering engine for rapid prototyping and game development. It abstracts Vulkan's complexity while maintaining flexibility for advanced use cases.

**Core Architecture:** 
Two-layer design with low-level rendering 
Engine uses modern C++20 with RAII for all Vulkan resources.

## Available Skills

The following skills provide domain-specific knowledge for working with VDE:

### using-api
**Purpose:** Guide for using the VDE Game API to create games, demos, applications, and examples.  
**Use when:** Creating applications with the high-level Game API, working with Scene/Entity/Resource system, or implementing gameplay features.  
**Location:** `.github/skills/using-api/SKILL.md`

### add-component
**Purpose:** Guide for adding new components to the VDE engine.  
**Use when:** Creating new classes, systems, or modules in the engine.  
**Location:** `.github/skills/add-component/SKILL.md`

### build-tool-workflows
**Purpose:** Build and test workflows for the VDE project.  
**Use when:** Building the project or running tests.  
**Location:** `.github/skills/build-tool-workflows/SKILL.md`

### create-tests
**Purpose:** Guide on how to create tests in this project.  
**Use when:** Asked to create unit tests.  
**Location:** `.github/skills/create-tests/SKILL.md`

### vulkan-patterns
**Purpose:** Vulkan patterns and common tasks for the VDE engine.  
**Use when:** Working with Vulkan resources, buffers, textures, or descriptors.  
**Location:** `.github/skills/vulkan-patterns/SKILL.md`

### writing-code
**Purpose:** Comprehensive guide for writing code in the VDE engine.  
**Use when:** Need information about coding conventions, file organization, CMake integration, or best practices.  
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
**Location:** `.github/skills/imgui-integration/SKILL.md`
