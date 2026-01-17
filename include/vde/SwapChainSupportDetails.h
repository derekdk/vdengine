#pragma once

/**
 * @file SwapChainSupportDetails.h
 * @brief Swap chain capability information structure.
 */

#include <vulkan/vulkan.h>
#include <vector>

namespace vde {

/**
 * @brief Contains swap chain support information for a physical device.
 * 
 * Queried during physical device selection and swap chain creation
 * to determine supported formats, present modes, and capabilities.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;           ///< Surface capabilities
    std::vector<VkSurfaceFormatKHR> formats;         ///< Supported surface formats
    std::vector<VkPresentModeKHR> presentModes;      ///< Supported present modes
};

} // namespace vde
