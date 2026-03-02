#pragma once

/**
 * @file Window.h
 * @brief Window management for Vulkan Display Engine
 *
 * Provides platform-independent window creation and input handling
 * using GLFW as the underlying implementation.
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <functional>

namespace vde {

/**
 * @brief Represents a display resolution with name
 */
struct Resolution {
    uint32_t width;
    uint32_t height;
    const char* name;
};

/**
 * @brief Window class for managing a GLFW window with Vulkan support
 *
 * Provides functionality for:
 * - Window creation and destruction
 * - Resolution changes and fullscreen toggle
 * - Event polling
 * - Resize callbacks
 */
class Window {
  public:
    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

    /**
     * @brief Construct a window with specified dimensions
     * @param width Window width in pixels
     * @param height Window height in pixels
     * @param title Window title
     */
    Window(uint32_t width, uint32_t height, const char* title);

    virtual ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Check if the window should close
     * @return true if close was requested
     */
    virtual bool shouldClose() const;

    /**
     * @brief Poll for window events
     */
    virtual void pollEvents();

    /**
     * @brief Get the underlying GLFW window handle
     * @return GLFWwindow pointer
     */
    GLFWwindow* getHandle() const { return m_window; }

    /**
     * @brief Get current window width
     * @return Width in pixels
     */
    uint32_t getWidth() const { return m_width; }

    /**
     * @brief Get current window height
     * @return Height in pixels
     */
    uint32_t getHeight() const { return m_height; }

    /**
     * @brief Get DPI scale factor for the window
     * @return DPI scale (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)
     *
     * Returns the content scale factor for the window's monitor,
     * which is useful for scaling UI elements on high-DPI displays.
     * On Windows with 150% scaling, this returns 1.5.
     */
    float getDPIScale() const;

    /**
     * @brief Set window resolution
     * @param width New width in pixels
     * @param height New height in pixels
     */
    virtual void setResolution(uint32_t width, uint32_t height);

    /**
     * @brief Toggle fullscreen mode
     * @param fullscreen true for fullscreen, false for windowed
     */
    virtual void setFullscreen(bool fullscreen);

    /**
     * @brief Check if window is in fullscreen mode
     * @return true if fullscreen
     */
    bool isFullscreen() const { return m_isFullscreen; }

    /**
     * @brief Set callback for window resize events
     * @param callback Function to call on resize
     */
    void setResizeCallback(ResizeCallback callback) { m_resizeCallback = callback; }

    /**
     * @brief Get a predefined resolution by index
     * @param index Resolution index (0-based)
     * @return Resolution struct
     */
    static const Resolution& getResolution(size_t index);

    /**
     * @brief Get the number of predefined resolutions
     * @return Count of available resolutions
     */
    static size_t getResolutionCount();

    /**
     * @brief Get array of all predefined resolutions
     * @return Pointer to resolution array
     */
    static const Resolution* getResolutions();

    /**
     * @brief Get DPI scale of primary monitor without creating a window
     * @return DPI scale (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)
     *
     * This is useful for determining appropriate window size before
     * creating the window. Temporarily initializes GLFW if needed.
     */
    static float getPrimaryMonitorDPIScale();

  protected:
    // Protected constructor for testing (allows mocks to avoid GLFW initialization)
    Window() : m_window(nullptr), m_width(800), m_height(600) {}

  private:
    GLFWwindow* m_window;
    uint32_t m_width;
    uint32_t m_height;

    bool m_isFullscreen = false;
    int m_windowedPosX = 0;
    int m_windowedPosY = 0;
    uint32_t m_windowedWidth = 800;
    uint32_t m_windowedHeight = 600;

    ResizeCallback m_resizeCallback;

    static const Resolution s_resolutions[];

    void notifyResize();
};

}  // namespace vde
