/**
 * @file ToolBase.h
 * @brief Shared base classes and utilities for VDE asset creation tools.
 *
 * This header provides common functionality for all VDE tools:
 * - Scriptable command execution (run from files or interactive REPL)
 * - Interactive GUI mode with ImGui
 * - Headless/batch mode for automation
 * - Console logging and error reporting
 * - File I/O helpers
 *
 * Usage:
 * 1. Inherit from BaseToolInputHandler for input handling
 * 2. Inherit from BaseToolScene for your tool's scene
 * 3. Inherit from BaseToolGame for the game instance
 * 4. Implement executeCommand() to handle tool-specific commands
 * 5. Support both GUI and script modes
 */

#pragma once

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ImGui includes (tools always use ImGui for interactive mode)
#include <vde/VulkanContext.h>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace vde {
namespace tools {

inline void setWorkingDirectoryToExecutablePath() {
#ifdef _WIN32
    char exePathBuffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, exePathBuffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return;
    }

    std::error_code error;
    std::filesystem::path exeDir = std::filesystem::path(exePathBuffer).parent_path();
    if (!exeDir.empty()) {
        std::filesystem::current_path(exeDir, error);
    }
#endif
}

/**
 * @brief Tool execution mode
 */
enum class ToolMode {
    INTERACTIVE,  ///< Interactive GUI mode with REPL console
    SCRIPT        ///< Batch script mode (headless)
};

/**
 * @brief Base input handler for tools with mouse camera controls.
 *
 * Provides standard functionality for:
 * - ESC key for exit
 * - Mouse camera rotation and zoom
 * - F1 key for UI toggle
 * - F11 key for fullscreen
 */
class BaseToolInputHandler : public vde::InputHandler {
  public:
    BaseToolInputHandler() = default;
    virtual ~BaseToolInputHandler() = default;

    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_F11) {
            m_fullscreenTogglePressed = true;
        }
        if (key == vde::KEY_F1) {
            m_debugUITogglePressed = true;
        }
    }

    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = true;
            m_lastMouseX = x;
            m_lastMouseY = y;
        }
    }

    void onMouseButtonRelease(int button, [[maybe_unused]] double x,
                              [[maybe_unused]] double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = false;
        }
    }

    void onMouseMove(double x, double y) override {
        if (m_mouseDown) {
            m_mouseDeltaX = x - m_lastMouseX;
            m_mouseDeltaY = y - m_lastMouseY;
        }
        m_lastMouseX = x;
        m_lastMouseY = y;
    }

    void onMouseScroll([[maybe_unused]] double xOffset, double yOffset) override {
        m_scrollDelta += static_cast<float>(yOffset);
    }

    bool isEscapePressed() {
        bool val = m_escapePressed;
        m_escapePressed = false;
        return val;
    }

    bool isFullscreenTogglePressed() {
        bool val = m_fullscreenTogglePressed;
        m_fullscreenTogglePressed = false;
        return val;
    }

    bool isDebugUITogglePressed() {
        bool val = m_debugUITogglePressed;
        m_debugUITogglePressed = false;
        return val;
    }

    bool isMouseDown() const { return m_mouseDown; }

    void getMouseDelta(double& dx, double& dy) {
        dx = m_mouseDeltaX;
        dy = m_mouseDeltaY;
        m_mouseDeltaX = 0.0;
        m_mouseDeltaY = 0.0;
    }

    float consumeScrollDelta() {
        float delta = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return delta;
    }

  protected:
    bool m_escapePressed = false;
    bool m_fullscreenTogglePressed = false;
    bool m_debugUITogglePressed = false;
    bool m_mouseDown = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    double m_mouseDeltaX = 0.0;
    double m_mouseDeltaY = 0.0;
    float m_scrollDelta = 0.0f;
};

/**
 * @brief Base scene class for tools with command execution and logging.
 *
 * Provides:
 * - Command execution interface
 * - Console logging
 * - Script file loading and execution
 * - Mouse camera controls
 * - Tool mode management
 *
 * To use:
 * 1. Inherit from this class
 * 2. Override executeCommand() to handle tool-specific commands
 * 3. Override getToolName() and getToolDescription()
 * 4. Call processScriptFile() if running in script mode
 */
class BaseToolScene : public vde::Scene {
  public:
    explicit BaseToolScene(ToolMode mode = ToolMode::INTERACTIVE) : m_toolMode(mode) {}

    virtual ~BaseToolScene() = default;

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        auto* input = dynamic_cast<BaseToolInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Handle ESC key
        if (input->isEscapePressed()) {
            if (getGame())
                getGame()->quit();
        }

        // Handle fullscreen toggle
        if (input->isFullscreenTogglePressed()) {
            if (getGame() && getGame()->getWindow()) {
                auto* window = getGame()->getWindow();
                window->setFullscreen(!window->isFullscreen());
            }
        }

        // Handle debug UI toggle
        if (input->isDebugUITogglePressed()) {
            m_debugUIVisible = !m_debugUIVisible;
        }

        // Handle camera rotation with mouse drag (only if not over ImGui window)
        if (m_toolMode == ToolMode::INTERACTIVE && input->isMouseDown() &&
            !ImGui::GetIO().WantCaptureMouse) {
            double dx, dy;
            input->getMouseDelta(dx, dy);
            if (dx != 0.0 || dy != 0.0) {
                auto* cam = dynamic_cast<vde::OrbitCamera*>(getCamera());
                if (cam) {
                    cam->rotate(static_cast<float>(-dy) * 0.2f,  // pitch (inverted)
                                static_cast<float>(dx) * 0.2f);  // yaw
                }
            }
        }

        // Handle camera zoom with mouse wheel
        if (m_toolMode == ToolMode::INTERACTIVE) {
            float scrollDelta = input->consumeScrollDelta();
            if (scrollDelta != 0.0f) {
                auto* cam = dynamic_cast<vde::OrbitCamera*>(getCamera());
                if (cam) {
                    cam->zoom(scrollDelta * 0.8f);
                }
            }
        }
    }

    /**
     * @brief Get the tool mode (interactive or script).
     */
    ToolMode getToolMode() const { return m_toolMode; }

    /**
     * @brief Check if debug UI is visible.
     */
    bool isDebugUIVisible() const { return m_debugUIVisible; }

    /**
     * @brief Set debug UI visibility.
     */
    void setDebugUIVisible(bool visible) { m_debugUIVisible = visible; }

    /**
     * @brief Execute a command (must be implemented by derived class).
     * @param cmdLine Full command line string
     */
    virtual void executeCommand(const std::string& cmdLine) = 0;

    /**
     * @brief Get the tool name (e.g., "Geometry REPL").
     */
    virtual std::string getToolName() const = 0;

    /**
     * @brief Get a brief description of the tool.
     */
    virtual std::string getToolDescription() const = 0;

    /**
     * @brief Load and execute commands from a script file.
     * @param filename Path to script file
     * @return true if successful, false on error
     */
    bool processScriptFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            addConsoleMessage("ERROR: Failed to open script file: " + filename);
            return false;
        }

        addConsoleMessage("Executing script: " + filename);
        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
            ++lineNum;

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            addConsoleMessage("> " + line);
            executeCommand(line);
        }

        addConsoleMessage("Script execution complete: " + std::to_string(lineNum) +
                          " lines processed");
        return true;
    }

    /**
     * @brief Add a message to the console log.
     *
     * Virtual so derived scenes can forward messages to custom UI widgets.
     */
    virtual void addConsoleMessage(const std::string& message) {
        m_consoleLog.push_back(message);

        // Limit console log size
        if (m_consoleLog.size() > 1000) {
            m_consoleLog.erase(m_consoleLog.begin());
        }

        m_scrollToBottom = true;

        // Print to stdout
        std::cout << message << std::endl;
    }

    /**
     * @brief Get the console log.
     */
    const std::vector<std::string>& getConsoleLog() const { return m_consoleLog; }

    /**
     * @brief Check if console should scroll to bottom.
     */
    bool shouldScrollToBottom() {
        bool val = m_scrollToBottom;
        m_scrollToBottom = false;
        return val;
    }

    /**
     * @brief Draw debug UI using ImGui (override for custom UI).
     */
    virtual void drawDebugUI() {
        // Default implementation - tools should override this
    }

    /**
     * @brief Called right before ImGui backend shutdown.
     *
     * Derived scenes can override this to release ImGui renderer resources
     * (e.g., texture descriptor sets) while the backend is still valid.
     */
    virtual void onBeforeImGuiShutdown() {}

  protected:
    ToolMode m_toolMode;
    bool m_debugUIVisible = true;
    std::vector<std::string> m_consoleLog;
    bool m_scrollToBottom = false;
};

/**
 * @brief Base game class for tools with ImGui integration.
 *
 * Template parameters:
 * - TInputHandler: Your input handler type (must derive from BaseToolInputHandler)
 * - TScene: Your scene type (must derive from BaseToolScene)
 */
template <typename TInputHandler, typename TScene>
class BaseToolGame : public vde::Game {
    static_assert(std::is_base_of<BaseToolInputHandler, TInputHandler>::value,
                  "TInputHandler must derive from BaseToolInputHandler");
    static_assert(std::is_base_of<BaseToolScene, TScene>::value,
                  "TScene must derive from BaseToolScene");

  public:
    BaseToolGame(ToolMode mode = ToolMode::INTERACTIVE) : m_toolMode(mode) {}

    virtual ~BaseToolGame() { cleanupImGui(); }

    void onStart() override {
        m_inputHandler = std::make_unique<TInputHandler>();
        setInputHandler(m_inputHandler.get());

        m_scenePtr = new TScene(m_toolMode);
        addScene("main", m_scenePtr);
        setActiveScene("main");

        if (m_toolMode == ToolMode::INTERACTIVE) {
            initImGui();
        }
    }

    void onRender() override {
        if (m_toolMode == ToolMode::INTERACTIVE) {
            renderImGui();
        }
    }

    void onShutdown() override {
        if (m_scenePtr && m_toolMode == ToolMode::INTERACTIVE) {
            m_scenePtr->onBeforeImGuiShutdown();
        }

        if (getVulkanContext()) {
            vkDeviceWaitIdle(getVulkanContext()->getDevice());
        }
        cleanupImGui();
    }

    int getExitCode() const { return m_exitCode; }

    TScene* getToolScene() { return m_scenePtr; }

  protected:
    static VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ImGui descriptor pool!");
        }
        return pool;
    }

    void initImGui() {
        auto* ctx = getVulkanContext();
        auto* win = getWindow();
        if (!ctx || !win)
            return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // Apply DPI scaling to fonts
        float dpiScale = getDPIScale();
        if (dpiScale > 0.0f) {
            io.FontGlobalScale = dpiScale;
        }

        ImGui_ImplGlfw_InitForVulkan(win->getHandle(), true);

        m_imguiPool = createImGuiDescriptorPool(ctx->getDevice());

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = ctx->getInstance();
        initInfo.PhysicalDevice = ctx->getPhysicalDevice();
        initInfo.Device = ctx->getDevice();
        initInfo.QueueFamily = ctx->getGraphicsQueueFamily();
        initInfo.Queue = ctx->getGraphicsQueue();
        initInfo.DescriptorPool = m_imguiPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = 2;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.RenderPass = ctx->getRenderPass();
        initInfo.Subpass = 0;

        ImGui_ImplVulkan_Init(&initInfo);

        ImGui_ImplVulkan_CreateFontsTexture();

        m_imguiInitialized = true;
    }

    void renderImGui() {
        if (!m_imguiInitialized || !m_scenePtr)
            return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (m_scenePtr->isDebugUIVisible()) {
            m_scenePtr->drawDebugUI();
        }

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData) {
            auto* ctx = getVulkanContext();
            if (ctx) {
                ImGui_ImplVulkan_RenderDrawData(drawData, ctx->getCurrentCommandBuffer());
            }
        }
    }

    void cleanupImGui() {
        if (m_imguiInitialized) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            m_imguiInitialized = false;
        }

        if (m_imguiPool != VK_NULL_HANDLE) {
            auto* ctx = getVulkanContext();
            if (ctx) {
                vkDestroyDescriptorPool(ctx->getDevice(), m_imguiPool, nullptr);
                m_imguiPool = VK_NULL_HANDLE;
            }
        }
    }

    ToolMode m_toolMode;
    std::unique_ptr<TInputHandler> m_inputHandler;
    TScene* m_scenePtr = nullptr;
    int m_exitCode = 0;
    bool m_imguiInitialized = false;
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
};

/**
 * @brief Helper function to run a tool.
 *
 * @param tool Tool game instance
 * @param title Window title
 * @param width Window width
 * @param height Window height
 * @param argc Argument count for CLI parsing (default: 0)
 * @param argv Argument values for CLI parsing (default: nullptr)
 * @return Exit code (0 = success)
 */
template <typename TInputHandler, typename TScene>
int runTool(BaseToolGame<TInputHandler, TScene>& tool, const std::string& title,
            uint32_t width = 1400, uint32_t height = 800, int argc = 0, char** argv = nullptr) {
    // Configure input script from CLI args BEFORE changing working directory
    // so relative paths resolve from the user's CWD
    if (argc > 0 && argv != nullptr) {
        vde::configureInputScriptFromArgs(tool, argc, argv);
    }

    setWorkingDirectoryToExecutablePath();

    vde::GameSettings settings;
    settings.gameName = title;
    settings.display.windowWidth = width;
    settings.display.windowHeight = height;
    settings.debug.enableValidation = true;

    if (!tool.initialize(settings)) {
        std::cerr << "Failed to initialize tool" << std::endl;
        return 1;
    }

    tool.run();
    return tool.getExitCode();
}

}  // namespace tools
}  // namespace vde
