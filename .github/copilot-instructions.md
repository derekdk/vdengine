# VDE (Vulkan Display Engine) - AI Coding Agent Instructions

## Project Overview
VDE is a lightweight Vulkan-based 3D rendering engine for rapid prototyping and game development. It abstracts Vulkan's complexity while maintaining flexibility for advanced use cases.

**Core Architecture:** Two-layer design with low-level rendering ([VulkanContext.cpp](../src/VulkanContext.cpp), [Texture.cpp](../src/Texture.cpp)) + high-level game API ([src/api/Game.cpp](../src/api/Game.cpp), [src/api/Scene.cpp](../src/api/Scene.cpp)). Engine uses modern C++20 with RAII for all Vulkan resources.

