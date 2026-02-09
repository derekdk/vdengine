#include <vde/Window.h>

#include <stdexcept>

namespace vde {

// Predefined resolutions
const Resolution Window::s_resolutions[] = {
    {800, 600, "800x600 (4:3)"},         {1024, 768, "1024x768 (4:3)"},
    {1280, 720, "1280x720 (HD)"},        {1366, 768, "1366x768"},
    {1920, 1080, "1920x1080 (Full HD)"}, {2560, 1440, "2560x1440 (QHD)"},
    {3840, 2160, "3840x2160 (4K)"}};

Window::Window(uint32_t width, uint32_t height, const char* title)
    : m_width(width), m_height(height), m_windowedWidth(width), m_windowedHeight(height) {
    glfwInit();

    // Don't create OpenGL context (we're using Vulkan)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Don't allow resizing for now
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

const Resolution& Window::getResolution(size_t index) {
    return s_resolutions[index];
}

size_t Window::getResolutionCount() {
    return sizeof(s_resolutions) / sizeof(Resolution);
}

const Resolution* Window::getResolutions() {
    return s_resolutions;
}

void Window::setResolution(uint32_t newWidth, uint32_t newHeight) {
    if (m_width == newWidth && m_height == newHeight) {
        return;  // No change needed
    }

    m_width = newWidth;
    m_height = newHeight;

    if (!m_isFullscreen) {
        // Update windowed mode size
        m_windowedWidth = newWidth;
        m_windowedHeight = newHeight;
        glfwSetWindowSize(m_window, static_cast<int>(newWidth), static_cast<int>(newHeight));
    } else {
        // In fullscreen, just update stored windowed size for when we exit fullscreen
        m_windowedWidth = newWidth;
        m_windowedHeight = newHeight;
    }

    notifyResize();
}

void Window::setFullscreen(bool fullscreen) {
    if (m_isFullscreen == fullscreen) {
        return;  // No change needed
    }

    m_isFullscreen = fullscreen;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (fullscreen) {
        // Save current windowed position and size
        glfwGetWindowPos(m_window, &m_windowedPosX, &m_windowedPosY);
        glfwGetWindowSize(m_window, reinterpret_cast<int*>(&m_windowedWidth),
                          reinterpret_cast<int*>(&m_windowedHeight));

        // Switch to fullscreen
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_width = mode->width;
        m_height = mode->height;
    } else {
        // Restore windowed mode
        glfwSetWindowMonitor(m_window, nullptr, m_windowedPosX, m_windowedPosY, m_windowedWidth,
                             m_windowedHeight, GLFW_DONT_CARE);
        m_width = m_windowedWidth;
        m_height = m_windowedHeight;
    }

    notifyResize();
}

float Window::getDPIScale() const {
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    // Return the maximum of x and y scale for consistent UI scaling
    return (xscale > yscale) ? xscale : yscale;
}

void Window::notifyResize() {
    if (m_resizeCallback) {
        m_resizeCallback(m_width, m_height);
    }
}

float Window::getPrimaryMonitorDPIScale() {
    // Check if GLFW is already initialized
    bool wasInitialized = (glfwGetCurrentContext() != nullptr);

    if (!wasInitialized) {
        glfwInit();
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetMonitorContentScale(primaryMonitor, &xscale, &yscale);
    float dpiScale = (xscale > yscale) ? xscale : yscale;

    if (!wasInitialized) {
        glfwTerminate();
    }

    return dpiScale;
}

}  // namespace vde
