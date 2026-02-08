#pragma once

/**
 * @file QueueFamilyIndices.h
 * @brief Queue family indices structure for Vulkan device selection.
 */

#include <cstdint>
#include <optional>

namespace vde {

/**
 * @brief Holds indices to Vulkan queue families.
 *
 * Used during physical device selection to ensure the device
 * supports required queue types (graphics and present).
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;  ///< Graphics queue family index
    std::optional<uint32_t> presentFamily;   ///< Present queue family index

    /**
     * @brief Check if all required queue families have been found.
     * @return true if both graphics and present families are set
     */
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

}  // namespace vde
