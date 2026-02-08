#pragma once

/**
 * @file ViewportRect.h
 * @brief Normalized viewport rectangle for split-screen rendering
 *
 * Provides the ViewportRect struct that describes a sub-region of the
 * window in normalized [0,1] coordinates.  Used by scenes to define
 * where they render within the window, enabling true split-screen
 * with independent cameras per viewport.
 */

#include <vulkan/vulkan.h>

#include <cstdint>

namespace vde {

/**
 * @brief A normalized viewport rectangle (origin top-left, [0,1] range).
 *
 * ViewportRect describes a sub-region of the window using normalized
 * coordinates where (0,0) is the top-left and (1,1) is the bottom-right.
 * This allows viewport definitions to be resolution-independent.
 *
 * Use the static factory methods for common layouts:
 * @code
 * auto full   = ViewportRect::fullWindow();    // whole window
 * auto left   = ViewportRect::leftHalf();      // left 50%
 * auto topR   = ViewportRect::topRight();      // top-right quadrant
 * @endcode
 *
 * Convert to Vulkan types for rendering:
 * @code
 * VkViewport vp = rect.toVkViewport(swapW, swapH);
 * VkRect2D   sc = rect.toVkScissor(swapW, swapH);
 * @endcode
 */
struct ViewportRect {
    float x = 0.0f;       ///< Left edge in normalized [0,1] coordinates
    float y = 0.0f;       ///< Top edge in normalized [0,1] coordinates
    float width = 1.0f;   ///< Width in normalized [0,1] coordinates
    float height = 1.0f;  ///< Height in normalized [0,1] coordinates

    // =====================================================================
    // Static Factories
    // =====================================================================

    /** @brief Full window viewport (default). */
    static ViewportRect fullWindow() { return {0.0f, 0.0f, 1.0f, 1.0f}; }

    /** @brief Top-left quadrant. */
    static ViewportRect topLeft() { return {0.0f, 0.0f, 0.5f, 0.5f}; }

    /** @brief Top-right quadrant. */
    static ViewportRect topRight() { return {0.5f, 0.0f, 0.5f, 0.5f}; }

    /** @brief Bottom-left quadrant. */
    static ViewportRect bottomLeft() { return {0.0f, 0.5f, 0.5f, 0.5f}; }

    /** @brief Bottom-right quadrant. */
    static ViewportRect bottomRight() { return {0.5f, 0.5f, 0.5f, 0.5f}; }

    /** @brief Left half of the window. */
    static ViewportRect leftHalf() { return {0.0f, 0.0f, 0.5f, 1.0f}; }

    /** @brief Right half of the window. */
    static ViewportRect rightHalf() { return {0.5f, 0.0f, 0.5f, 1.0f}; }

    /** @brief Top half of the window. */
    static ViewportRect topHalf() { return {0.0f, 0.0f, 1.0f, 0.5f}; }

    /** @brief Bottom half of the window. */
    static ViewportRect bottomHalf() { return {0.0f, 0.5f, 1.0f, 0.5f}; }

    // =====================================================================
    // Hit Testing
    // =====================================================================

    /**
     * @brief Test if a normalized screen position is inside this viewport.
     * @param normalizedX X position in [0,1] (0 = left edge of window)
     * @param normalizedY Y position in [0,1] (0 = top edge of window)
     * @return true if the point is inside (inclusive of edges)
     */
    bool contains(float normalizedX, float normalizedY) const {
        return normalizedX >= x && normalizedX <= x + width && normalizedY >= y &&
               normalizedY <= y + height;
    }

    // =====================================================================
    // Vulkan Conversions
    // =====================================================================

    /**
     * @brief Convert to a VkViewport for the given swapchain dimensions.
     * @param swapchainWidth  Swapchain width in pixels
     * @param swapchainHeight Swapchain height in pixels
     * @return VkViewport with pixel coordinates and standard depth range
     */
    VkViewport toVkViewport(uint32_t swapchainWidth, uint32_t swapchainHeight) const {
        VkViewport vp{};
        vp.x = x * static_cast<float>(swapchainWidth);
        vp.y = y * static_cast<float>(swapchainHeight);
        vp.width = width * static_cast<float>(swapchainWidth);
        vp.height = height * static_cast<float>(swapchainHeight);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        return vp;
    }

    /**
     * @brief Convert to a VkRect2D scissor for the given swapchain dimensions.
     * @param swapchainWidth  Swapchain width in pixels
     * @param swapchainHeight Swapchain height in pixels
     * @return VkRect2D with pixel coordinates
     */
    VkRect2D toVkScissor(uint32_t swapchainWidth, uint32_t swapchainHeight) const {
        VkRect2D scissor{};
        scissor.offset.x = static_cast<int32_t>(x * static_cast<float>(swapchainWidth));
        scissor.offset.y = static_cast<int32_t>(y * static_cast<float>(swapchainHeight));
        scissor.extent.width = static_cast<uint32_t>(width * static_cast<float>(swapchainWidth));
        scissor.extent.height = static_cast<uint32_t>(height * static_cast<float>(swapchainHeight));
        return scissor;
    }

    /**
     * @brief Get the aspect ratio of the viewport.
     * @param swapchainWidth  Swapchain width in pixels
     * @param swapchainHeight Swapchain height in pixels
     * @return Aspect ratio (width / height in pixels)
     */
    float getAspectRatio(uint32_t swapchainWidth, uint32_t swapchainHeight) const {
        float pw = width * static_cast<float>(swapchainWidth);
        float ph = height * static_cast<float>(swapchainHeight);
        return (ph > 0.0f) ? (pw / ph) : 1.0f;
    }

    /**
     * @brief Equality comparison.
     */
    bool operator==(const ViewportRect& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }

    /**
     * @brief Inequality comparison.
     */
    bool operator!=(const ViewportRect& other) const { return !(*this == other); }
};

}  // namespace vde
