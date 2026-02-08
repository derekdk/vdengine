#pragma once

/**
 * @file Core.h
 * @brief Main include header for Vulkan Display Engine (VDE).
 *
 * This header includes all commonly used VDE components.
 * For minimal builds, include only the specific headers you need.
 *
 * @code
 * #include <vde/Core.h>
 *
 * int main() {
 *     vde::Window window(1280, 720, "My Application");
 *     vde::VulkanContext context;
 *     context.initialize(&window);
 *
 *     while (!window.shouldClose()) {
 *         window.pollEvents();
 *         context.drawFrame();
 *     }
 *
 *     context.cleanup();
 *     return 0;
 * }
 * @endcode
 */

// Core system
#include <vde/VulkanContext.h>
#include <vde/Window.h>

// Rendering components
#include <vde/Camera.h>
#include <vde/ImageLoader.h>
#include <vde/Texture.h>
#include <vde/Types.h>

// Buffer management
#include <vde/BufferUtils.h>
#include <vde/DescriptorManager.h>
#include <vde/UniformBuffer.h>

// Shader system
#include <vde/ShaderCache.h>
#include <vde/ShaderCompiler.h>
#include <vde/ShaderStage.h>

// Geometry
#include <vde/HexGeometry.h>
#include <vde/HexPrismMesh.h>

// Vulkan helpers
#include <vde/QueueFamilyIndices.h>
#include <vde/SwapChainSupportDetails.h>

namespace vde {

/**
 * @brief VDE library version information.
 */
struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 1;
    static constexpr int PATCH = 0;

    static const char* getString() { return "0.1.0"; }
};

}  // namespace vde
