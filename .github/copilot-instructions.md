# VDE (Vulkan Display Engine) - AI Coding Agent Instructions

## Project Overview
VDE is a lightweight Vulkan-based 3D rendering engine for rapid prototyping and game development. It abstracts Vulkan's complexity while maintaining flexibility for advanced use cases.

**Core Architecture:** Two-layer design with low-level rendering ([VulkanContext.cpp](../src/VulkanContext.cpp), [Texture.cpp](../src/Texture.cpp)) + high-level game API ([src/api/Game.cpp](../src/api/Game.cpp), [src/api/Scene.cpp](../src/api/Scene.cpp)). Engine uses modern C++20 with RAII for all Vulkan resources.

---
**Skill System:** Detailed guides available in [.github/skills/](.github/skills/):
- [writing-code/SKILL.md](.github/skills/writing-code/SKILL.md) - Conventions, naming, file organization, include order, Doxygen docs
- [add-component/SKILL.md](.github/skills/add-component/SKILL.md) - Complete workflow for adding classes/systems
- [vulkan-patterns/SKILL.md](.github/skills/vulkan-patterns/SKILL.md) - RAII patterns, buffer management, descriptors
- [create-tests/SKILL.md](.github/skills/create-tests/SKILL.md) - Unit test creation and GoogleTest usage
- [build-tool-workflows/SKILL.md](.github/skills/build-tool-workflows/SKILL.md) - CMake, Visual Studio, Ninja build commands
- [architecture/SKILL.md](.github/skills/architecture/SKILL.md) - Directory structure and component locations
