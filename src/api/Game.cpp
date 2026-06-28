/**
 * @file Game.cpp
 * @brief Implementation of Game class
 */

#include <vde/BufferUtils.h>
#include <vde/ShaderCompiler.h>
#include <vde/Types.h>
#include <vde/VulkanContext.h>
#include <vde/Window.h>
#include <vde/api/AudioManager.h>
#include <vde/api/Game.h>
#include <vde/api/InputScript.h>
#include <vde/api/LightBox.h>
#include <vde/api/PhysicsEntity.h>
#include <vde/api/PhysicsScene.h>
#include <vde/api/StorageManager.h>
#include <vde/api/TransitionManager.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "stb_image.h"
#include "stb_image_write.h"
#include <glslang/Public/ShaderLang.h>

namespace vde {

// Forward declaration of sprite descriptor cache cleanup function from Entity.cpp
extern void clearSpriteDescriptorCache();

// ============================================================================
// Game Implementation
// ============================================================================

Game::Game() = default;

// NOLINTNEXTLINE(bugprone-exception-escape)
Game::~Game() {
    if (m_initialized) {
        shutdown();
    }
}

bool Game::initialize(const GameSettings& settings) {
    if (m_initialized) {
        return true;
    }

    m_settings = settings;

    // Initialize glslang for shader compilation
    glslang::InitializeProcess();

    try {
        // Create window
        m_window =
            std::make_unique<Window>(settings.display.windowWidth, settings.display.windowHeight,
                                     settings.gameName.c_str(), settings.display.resizable);

        if (settings.display.fullscreen) {
            m_window->setFullscreen(true);
        }

        // Create and initialize Vulkan context
        m_vulkanContext = std::make_unique<VulkanContext>();
        m_vulkanContext->initialize(m_window.get());

        // Create lighting resources first (needed by mesh pipeline)
        createLightingResources();

        // Create mesh rendering pipeline
        createMeshRenderingPipeline();

        // Create sprite rendering pipeline (Phase 3)
        createSpriteRenderingPipeline();

        // Create transition system
        m_transitionManager = std::make_unique<TransitionManager>(m_vulkanContext.get());
        {
            VkExtent2D extent = m_vulkanContext->getSwapChainExtent();
            m_transitionManager->recreateRenderTargets(extent.width, extent.height);
        }

        // Initialize audio system (Phase 6)
        AudioManager::getInstance().initialize(settings.audio);

        // Setup input callbacks
        setupInputCallbacks();

        // Set up resize callback
        m_window->setResizeCallback([this](uint32_t width, uint32_t height) {
            if (m_vulkanContext) {
                m_vulkanContext->recreateSwapchain(width, height);
            }
            // Resize transition render targets
            if (m_transitionManager) {
                m_transitionManager->recreateRenderTargets(width, height);
            }
            // Update camera aspect ratio if there's an active scene
            if (m_activeScene && m_activeScene->getCamera()) {
                float aspect = static_cast<float>(width) / static_cast<float>(height);
                m_activeScene->getCamera()->setAspectRatio(aspect);
            }
            if (m_resizeCallback) {
                m_resizeCallback(width, height);
            }
        });

        m_initialized = true;
        m_lastFrameTime = glfwGetTime();

        // Input script discovery: API call > CLI arg > env var
        // (CLI arg is applied before initialize via configureInputScriptFromArgs)
        if (m_inputScriptFile.empty()) {
            const char* envScript = std::getenv("VDE_INPUT_SCRIPT");
            if (envScript && envScript[0] != '\0') {
                std::filesystem::path scriptPath(envScript);
                // Only allow .vdescript files to prevent unintended file access via the env var.
                if (scriptPath.extension() == ".vdescript") {
                    std::error_code ec;
                    auto canonical = std::filesystem::weakly_canonical(scriptPath, ec);
                    if (!ec) {
                        m_inputScriptFile = canonical.string();
                    } else {
                        std::cerr << "[VDE] VDE_INPUT_SCRIPT path could not be resolved: "
                                  << envScript << '\n';
                    }
                } else {
                    std::cerr << "[VDE] VDE_INPUT_SCRIPT rejected: path must have a .vdescript "
                                 "extension"
                              << '\n';
                }
            }
        }
        if (!m_inputScriptFile.empty()) {
            loadInputScript();
        }

        return true;

    } catch (const std::exception& e) {
        // Clean up on failure
        std::cerr << "Game initialization failed: " << e.what() << '\n';
        m_vulkanContext.reset();
        m_window.reset();
        throw;
    }
}

void Game::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Call shutdown hook
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    onShutdown();

    // Deactivate all currently active scenes
    std::vector<std::string> activeScenes(m_activeSceneNames.begin(), m_activeSceneNames.end());
    for (const auto& sceneName : activeScenes) {
        deactivateScene(sceneName);
    }
    m_activeScene = nullptr;
    m_activeSceneGroup = SceneGroup{};
    m_activeSceneNames.clear();  // Defensive: ensure clean state on shutdown

    // Clear all scenes
    m_scenes.clear();
    m_sceneStack.clear();

    // Clear sprite descriptor cache (static in Entity.cpp)
    clearSpriteDescriptorCache();

    // Shutdown audio system
    AudioManager::getInstance().shutdown();

    // Shutdown persistent storage
    StorageManager::getInstance().shutdown();

    // Destroy transition system (before rendering pipelines)
    m_transitionManager.reset();

    // Cleanup rendering pipelines
    destroyLightingResources();
    destroySpriteRenderingPipeline();
    destroyMeshRenderingPipeline();

    // Cleanup Vulkan
    if (m_vulkanContext) {
        m_vulkanContext->cleanup();
        m_vulkanContext.reset();
    }

    // Destroy window
    m_window.reset();

    // Finalize glslang
    glslang::FinalizeProcess();

    m_initialized = false;
}

void Game::run() {
    if (!m_initialized) {
        throw std::runtime_error("Game::run() called before initialize()");
    }

    m_running = true;

    // Call start hook
    onStart();

    // Enter active scene if one is set
    if (m_activeScene) {
        // Ensure there's a scene group for the active scene
        if (m_activeSceneGroup.empty()) {
            for (auto& [name, scenePtr] : m_scenes) {
                if (scenePtr.get() == m_activeScene) {
                    m_activeSceneGroup = SceneGroup::create(name, {name});
                    break;
                }
            }
        }
        // Enter all scenes in the initial group
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            activateScene(sceneName);
        }

        // Set isFocused based on getFocusedScene() to match actual input routing
        for (auto& [name, scenePtr] : m_scenes) {
            if (scenePtr) {
                scenePtr->m_diagnostics.isFocused = false;
            }
        }
        Scene* focusedScene = getFocusedScene();
        if (!focusedScene) {
            focusedScene = m_activeScene;
        }
        if (focusedScene) {
            focusedScene->m_diagnostics.isFocused = true;
        }
    }

    // Build the initial scheduler task graph
    rebuildSchedulerGraph();

    // Ensure the thread pool is ready before the first frame.
    // Minimum 3 workers to allow physics sub-phases from different scenes to
    // overlap.  Default: max(3, hardware_concurrency - 1), capped at 16.
    {
        const size_t kMinWorkers = 3;
        const size_t kMaxWorkers = 16;
        size_t current = m_scheduler.getWorkerThreadCount();
        if (current == 0) {
            size_t hw = std::thread::hardware_concurrency();
            size_t target = (hw > 1) ? (hw - 1) : kMinWorkers;
            target = std::max(target, kMinWorkers);
            target = std::min(target, kMaxWorkers);
            m_scheduler.setWorkerThreadCount(target);
        } else if (current < kMinWorkers) {
            std::cerr << "[VDE] Worker thread count clamped from " << current << " to "
                      << kMinWorkers << " (minimum for physics parallelism)\n";
            m_scheduler.setWorkerThreadCount(kMinWorkers);
        }
    }

    // Main game loop
    while (m_running && !m_window->shouldClose()) {
        // Poll window events
        m_window->pollEvents();

        // Update timing
        updateTiming();

        // Process any pending scene changes
        processPendingSceneChange();

        // Process input
        processInput();

        // Execute the scheduler task graph
        // (covers: input script, onUpdate, scene update, audio, pre-render, render)
        m_scheduler.execute();

        m_frameCount++;
    }

    // Wait for GPU to finish
    if (m_vulkanContext) {
        vkDeviceWaitIdle(m_vulkanContext->getDevice());
    }

    m_running = false;
}

void Game::quit() {
    m_running = false;
}

void Game::setExitCode(int code) {
    if (code != 0) {
        m_exitCode = code;
    }
}

bool Game::captureScreenshot(const std::string& outputPath) {
    if (!m_vulkanContext) {
        std::cerr << "[VDE:InputScript] screenshot failed: no Vulkan context" << '\n';
        return false;
    }

    uint32_t width = 0, height = 0;
    auto pixels = m_vulkanContext->captureFramebuffer(width, height);
    if (pixels.empty() || width == 0 || height == 0) {
        std::cerr << "[VDE:InputScript] screenshot failed: framebuffer capture returned no data"
                  << '\n';
        return false;
    }

    // Create parent directories if they don't exist
    auto parentPath = std::filesystem::path(outputPath).parent_path();
    if (!parentPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parentPath, ec);
        if (ec) {
            std::cerr << "[VDE:InputScript] screenshot warning: could not create directory '"
                      << parentPath.string() << "'" << '\n';
        }
    }

    // Write PNG via stb_image_write
    int result =
        stbi_write_png(outputPath.c_str(), static_cast<int>(width), static_cast<int>(height),
                       4,  // RGBA
                       pixels.data(), static_cast<int>(width * 4));

    if (result == 0) {
        std::cerr << "[VDE:InputScript] screenshot failed: could not write '" << outputPath << "'"
                  << '\n';
        return false;
    }

    std::cout << "[VDE:InputScript] screenshot saved: " << outputPath << " (" << width << "x"
              << height << ")" << '\n';
    return true;
}

void Game::setInputScriptFile(const std::string& scriptPath) {
    m_inputScriptFile = scriptPath;
}

const std::string& Game::getInputScriptFile() const {
    return m_inputScriptFile;
}

void Game::loadInputScript() {
    ScriptEnvironment& env = *this;
    m_scriptExecutor = std::make_unique<InputScriptExecutor>(env);
    m_scriptExecutor->loadScript(m_inputScriptFile);
}

InputHandler* Game::resolveInputHandler() {
    if (Scene* focusedScene = getFocusedScene()) {
        if (InputHandler* handler = focusedScene->getInputHandler()) {
            return handler;
        }
    }

    return m_inputHandler;
}

std::pair<uint32_t, uint32_t> Game::getSwapChainExtent() const {
    if (!m_vulkanContext) {
        return {0, 0};
    }

    const auto extent = m_vulkanContext->getSwapChainExtent();
    return {extent.width, extent.height};
}

void Game::processInputScript() {
    if (m_scriptExecutor) {
        m_scriptExecutor->processFrame(m_deltaTime);
    }
}

float Game::getDPIScale() const {
    return m_window ? m_window->getDPIScale() : 1.0f;
}

void Game::addScene(const std::string& name, Scene* scene) {
    addScene(name, std::unique_ptr<Scene>(scene));
}

void Game::addScene(const std::string& name, std::unique_ptr<Scene> scene) {
    if (!scene) {
        return;
    }

    scene->m_name = name;
    scene->m_game = this;
    m_scenes[name] = std::move(scene);
    m_scenesCreated++;
}

void Game::removeScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        return;
    }

    // If this scene is active, deactivate it
    bool wasActiveScene = (m_activeScene == it->second.get());
    deactivateScene(name);
    if (wasActiveScene) {
        m_activeScene = nullptr;
    }

    // Remove from stack if present
    std::erase(m_sceneStack, name);

    m_scenes.erase(it);
    m_scenesRemoved++;
}

Scene* Game::getScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Game::setActiveScene(const std::string& name) {
    // Defer scene switch to avoid issues during update/render.
    // Internally creates a single-scene group.
    m_pendingScene = name;
    m_sceneSwitchPending = true;
}

void Game::setActiveSceneGroup(const SceneGroup& group) {
    // Validate that all scenes exist
    for (const auto& sceneName : group.sceneNames) {
        if (m_scenes.find(sceneName) == m_scenes.end()) {
            return;  // Silently ignore invalid groups
        }
    }

    // Build set for new group for fast lookup
    std::unordered_set<std::string> newGroupSet(group.sceneNames.begin(), group.sceneNames.end());

    // Exit scenes that are currently active but NOT in the NEW group
    std::vector<std::string> activeScenes(m_activeSceneNames.begin(), m_activeSceneNames.end());
    for (const auto& sceneName : activeScenes) {
        if (newGroupSet.find(sceneName) == newGroupSet.end()) {
            deactivateScene(sceneName);
        }
    }

    // Clear scene stack (group switch resets the stack)
    m_sceneStack.clear();

    // Set new group
    m_activeSceneGroup = group;

    // Set primary scene (first in the group)
    if (!group.sceneNames.empty()) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        auto it = m_scenes.find(group.sceneNames[0]);
        if (it != m_scenes.end()) {
            m_activeScene = it->second.get();
        }
    } else {
        m_activeScene = nullptr;
    }

    // Enter scenes that are in the NEW group but NOT currently active
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        activateScene(sceneName);
    }

    // Refresh focus diagnostics: exactly one scene in the group is focused —
    // the explicitly focused scene if it is in the group, otherwise the primary scene.
    Scene* focusedScene = nullptr;
    if (!m_focusedSceneName.empty() && newGroupSet.count(m_focusedSceneName) > 0) {
        auto focusedIt = m_scenes.find(m_focusedSceneName);
        if (focusedIt != m_scenes.end()) {
            focusedScene = focusedIt->second.get();
        }
    }
    if (focusedScene == nullptr) {
        focusedScene = m_activeScene;
    }

    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) {
            it->second->m_diagnostics.isFocused = (it->second.get() == focusedScene);
        }
    }

    // Apply per-scene viewport rects from group entries (if present)
    if (m_activeSceneGroup.hasViewports()) {
        for (const auto& entry : m_activeSceneGroup.entries) {
            auto it = m_scenes.find(entry.sceneName);
            if (it != m_scenes.end()) {
                it->second->setViewportRect(entry.viewport);
            }
        }
    }

    // Rebuild the scheduler graph for the new group
    rebuildSchedulerGraph();
}

void Game::pushScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        return;
    }

    // Pause current scene
    if (m_activeScene) {
        m_activeScene->m_diagnostics.isFocused = false;
        m_activeScene->m_diagnostics.pauseCount++;
        m_activeScene->onPause();
        // Find current scene name and push to stack
        for (auto& pair : m_scenes) {
            if (pair.second.get() == m_activeScene) {
                m_sceneStack.push_back(pair.first);
                break;
            }
        }
    }

    // Activate new scene
    m_activeScene = it->second.get();
    activateScene(name);
    m_activeScene->m_diagnostics.isFocused = true;
}

void Game::popScene() {
    if (m_sceneStack.empty()) {
        return;
    }

    // Exit current scene
    if (m_activeScene) {
        deactivateScene(m_activeScene->getName());
    }

    // Resume previous scene
    std::string prevName = m_sceneStack.back();
    m_sceneStack.pop_back();

    auto it = m_scenes.find(prevName);
    if (it != m_scenes.end()) {
        m_activeScene = it->second.get();
        m_activeScene->m_diagnostics.isFocused = true;
        m_activeScene->m_diagnostics.resumeCount++;
        m_activeScene->onResume();
    } else {
        m_activeScene = nullptr;
    }
}

// ============================================================================
// Transitions
// ============================================================================

void Game::transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                             float duration) {
    // Validate destination scene exists before mutating any state
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        throw std::runtime_error("[VDE] transitionToScene: unknown scene '" + name + "'");
    }

    // Full-screen transition (default)
    m_viewportTransition = false;
    m_transitionViewport = ViewportRect::fullWindow();

    // If duration <= 0, do an instant switch
    if (duration <= 0.0f) {
        setActiveScene(name);
        return;
    }

    // If already transitioning, cancel first (exits dest scene)
    if (m_transitionManager && m_transitionManager->isActive()) {
        cancelTransition();
    }

    // Record source scene name (current primary scene)
    m_transitionSourceScene.clear();
    if (m_activeScene) {
        for (const auto& [sceneName, scenePtr] : m_scenes) {
            if (scenePtr.get() == m_activeScene) {
                m_transitionSourceScene = sceneName;
                break;
            }
        }
    }
    m_transitionDestScene = name;

    // Enter the destination scene (so it starts receiving updates)
    // activateScene is idempotent — no-op if already active
    activateScene(name);

    // Start the transition
    m_transitionManager->start(std::move(transition), duration, [this]() {
        // Transition complete callback
        auto destIt = m_scenes.find(m_transitionDestScene);
        if (destIt != m_scenes.end()) {
            m_activeScene = destIt->second.get();
            m_activeScene->m_diagnostics.isFocused = true;
        }
        m_activeSceneGroup = SceneGroup::create(m_transitionDestScene, {m_transitionDestScene});

        // Finish the source-scene exit and scheduler rebuild at the next frame boundary.
        m_pendingScene = m_transitionDestScene;
        m_sceneSwitchPending = true;

        // Clear transition state
        m_transitionSourceScene.clear();
        m_transitionDestScene.clear();
        m_viewportTransition = false;
        m_transitionViewport = ViewportRect::fullWindow();
    });
}

void Game::transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                             float duration, const ViewportRect& region) {
    // Store viewport info before delegating to the main overload
    transitionToScene(name, std::move(transition), duration);

    // Apply viewport-scoped transition settings (after transitionToScene sets up state).
    // The render task reads m_viewportTransition at runtime, so no scheduler rebuild is needed.
    if (m_transitionManager && m_transitionManager->isActive()) {
        m_viewportTransition = true;
        m_transitionViewport = region;
    }
}

bool Game::isTransitioning() const {
    return m_transitionManager && m_transitionManager->isActive();
}

void Game::cancelTransition() {
    if (!m_transitionManager || !m_transitionManager->isActive()) {
        return;
    }

    // Exit the destination scene since we're reverting to source
    if (!m_transitionDestScene.empty() && m_transitionDestScene != m_transitionSourceScene) {
        deactivateScene(m_transitionDestScene);
    }

    m_transitionManager->cancel();
    m_transitionSourceScene.clear();
    m_transitionDestScene.clear();
    m_viewportTransition = false;
    m_transitionViewport = ViewportRect::fullWindow();
}

float Game::getTransitionProgress() const {
    if (m_transitionManager) {
        return m_transitionManager->getProgress();
    }
    return 0.0f;
}

void Game::setTransitionPaused(bool paused) {
    if (m_transitionManager) {
        m_transitionManager->setPaused(paused);
    }
}

bool Game::isTransitionPaused() const {
    if (m_transitionManager) {
        return m_transitionManager->isPaused();
    }
    return false;
}

void Game::stepTransitionOneFrame() {
    if (m_transitionManager) {
        m_transitionManager->stepOneFrame(m_deltaTime);
    }
}

void Game::setTransitionSpeed(float speed) {
    if (m_transitionManager) {
        m_transitionManager->setSpeed(speed);
    }
}

float Game::getTransitionSpeed() const {
    if (m_transitionManager) {
        return m_transitionManager->getSpeed();
    }
    return 1.0f;
}

void Game::applyDisplaySettings(const DisplaySettings& settings) {
    if (!m_window) {
        return;
    }

    m_settings.display = settings;

    scheduleWindowResize(settings.windowWidth, settings.windowHeight);
    scheduleWindowFullscreen(settings.fullscreen);
}

void Game::scheduleWindowOperation(std::function<void(Window&)> operation) {
    if (!operation || !m_window) {
        return;
    }

    if (!m_running) {
        operation(*m_window);
        return;
    }

    std::scoped_lock lock(m_pendingWindowOperationsMutex);
    m_pendingWindowOperations.push_back(
        {.kind = WindowOperationKind::Generic, .operation = std::move(operation)});
}

void Game::scheduleWindowResize(uint32_t width, uint32_t height) {
    if (!m_window) {
        return;
    }

    if (!m_running) {
        m_window->setResolution(width, height);
        return;
    }

    std::scoped_lock lock(m_pendingWindowOperationsMutex);
    m_pendingWindowOperations.push_back(
        {.kind = WindowOperationKind::Resize,
         .operation = [width, height](Window& window) { window.setResolution(width, height); }});
}

void Game::scheduleWindowFullscreen(bool fullscreen) {
    scheduleWindowOperation([fullscreen](Window& window) { window.setFullscreen(fullscreen); });
}

void Game::scheduleWindowResizable(bool resizable) {
    scheduleWindowOperation([resizable](Window& window) {
        if (auto* handle = window.getHandle()) {
            glfwSetWindowAttrib(handle, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
        }
    });
}

void Game::applyGraphicsSettings(const GraphicsSettings& settings) {
    m_settings.graphics = settings;
    // Phase 2+: Apply graphics settings to renderer
}

void Game::setResizeCallback(std::function<void(uint32_t, uint32_t)> callback) {
    m_resizeCallback = std::move(callback);
}

void Game::setFocusCallback(std::function<void(bool)> callback) {
    m_focusCallback = std::move(callback);
}

// ============================================================================
// Private Methods
// ============================================================================

void Game::processInput() {
    // Keyboard and mouse input is handled via GLFW callbacks (setupInputCallbacks).
    // Gamepad input must be polled each frame since GLFW doesn't provide callbacks for it.
    pollGamepads();
}

void Game::pollGamepads() {
    // Collect all input handlers that should receive gamepad events
    auto dispatchToHandlers = [&](auto callback) {
        // Global handler
        if (m_inputHandler) {
            callback(m_inputHandler);
        }
        // Per-scene handlers
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            auto it = m_scenes.find(sceneName);
            if (it != m_scenes.end()) {
                InputHandler* sceneHandler = it->second->getInputHandler();
                if (sceneHandler && sceneHandler != m_inputHandler) {
                    callback(sceneHandler);
                }
            }
        }
    };

    for (int jid = JOYSTICK_1; jid <= JOYSTICK_LAST; ++jid) {
        bool present = glfwJoystickPresent(jid) == GLFW_TRUE;
        bool isGamepad = present && glfwJoystickIsGamepad(jid) == GLFW_TRUE;

        // Connection/disconnection is handled by the joystick callback
        // (set in setupInputCallbacks). Here we only poll state for gamepads.
        if (!isGamepad) {
            continue;
        }

        GLFWgamepadstate state;
        if (glfwGetGamepadState(jid, &state) != GLFW_TRUE) {
            continue;
        }

        // Dispatch button press/release events by comparing with previous state
        dispatchToHandlers([&](InputHandler* handler) {
            float deadZone = handler->getDeadZone();

            for (int btn = 0; btn <= GAMEPAD_BUTTON_LAST; ++btn) {
                bool pressed = (state.buttons[btn] == GLFW_PRESS);
                bool wasPressed = handler->isGamepadButtonPressed(jid, btn);

                if (pressed && !wasPressed) {
                    handler->_setGamepadButton(jid, btn, true);
                    handler->onGamepadButtonPress(jid, btn);
                } else if (!pressed && wasPressed) {
                    handler->_setGamepadButton(jid, btn, false);
                    handler->onGamepadButtonRelease(jid, btn);
                }
            }

            // Dispatch axis changes
            for (int axis = 0; axis <= GAMEPAD_AXIS_LAST; ++axis) {
                float raw = state.axes[axis];
                // Apply dead zone
                float value = (std::abs(raw) < deadZone) ? 0.0f : raw;
                float prev = handler->getGamepadAxis(jid, axis);

                // Only fire event if the value actually changed (with a small epsilon)
                if (std::abs(value - prev) > 0.001f) {
                    handler->_setGamepadAxis(jid, axis, value);
                    handler->onGamepadAxis(jid, axis, value);
                }
            }
        });
    }
}

void Game::updateTiming() {
    double currentTime = glfwGetTime();
    m_deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;
    m_totalTime = currentTime;

    // FPS calculation (averaged over ~1 second)
    m_fpsAccumulator += m_deltaTime;
    m_fpsFrameCount++;

    if (m_fpsAccumulator >= 1.0) {
        m_fps = static_cast<float>(m_fpsFrameCount) / static_cast<float>(m_fpsAccumulator);
        m_fpsAccumulator = 0.0;
        m_fpsFrameCount = 0;
    }
}

void Game::processPendingSceneChange() {
    if (!m_sceneSwitchPending) {
        return;
    }

    m_sceneSwitchPending = false;

    auto it = m_scenes.find(m_pendingScene);
    if (it == m_scenes.end()) {
        return;
    }

    // Exit currently active scenes that are NOT the pending scene
    std::vector<std::string> activeScenes(m_activeSceneNames.begin(), m_activeSceneNames.end());
    for (const auto& sceneName : activeScenes) {
        if (sceneName == m_pendingScene) {
            continue;  // Will stay active — don't exit
        }
        deactivateScene(sceneName);
    }

    // Clear scene stack (setActiveScene resets the stack)
    m_sceneStack.clear();

    // Create a single-scene group
    m_activeSceneGroup = SceneGroup::create(m_pendingScene, {m_pendingScene});

    // Enter new scene (activateScene is a no-op if already active)
    m_activeScene = it->second.get();
    activateScene(m_pendingScene);

    // Update focus tracking
    m_activeScene->m_diagnostics.isFocused = true;

    // Rebuild the scheduler graph for the new scene
    rebuildSchedulerGraph();
}

void Game::executePendingWindowOperations() {
    if (!m_window) {
        return;
    }

    std::vector<PendingWindowOperation> pending;
    {
        std::scoped_lock lock(m_pendingWindowOperationsMutex);
        if (m_pendingWindowOperations.empty()) {
            return;
        }
        pending.swap(m_pendingWindowOperations);
    }

    size_t lastResizeIndex = pending.size();
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (size_t index = 0; index < pending.size(); ++index) {
        if (pending[index].kind == WindowOperationKind::Resize) {
            lastResizeIndex = index;
        }
    }

    for (size_t index = 0; index < pending.size(); ++index) {
        auto& pendingOperation = pending[index];
        if (pendingOperation.kind == WindowOperationKind::Resize && index != lastResizeIndex) {
            continue;
        }

        if (pendingOperation.operation) {
            pendingOperation.operation(*m_window);
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}

void Game::setupInputCallbacks() {
    if (!m_window) {
        return;
    }

    GLFWwindow* handle = m_window->getHandle();

    // Store 'this' pointer for callbacks
    glfwSetWindowUserPointer(handle, this);

    // Key callback
    glfwSetKeyCallback(handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)scancode;  // Unused

        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) {
            return;
        }

        // Determine which input handler to use — focused scene for split-screen
        InputHandler* handler = nullptr;
        Scene* focusedScene = game->getFocusedScene();
        if (focusedScene && focusedScene->getInputHandler()) {
            handler = focusedScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }

        if (handler) {
            (void)mods;  // Modifier keys available but not passed to simple interface
            if (action == GLFW_PRESS) {
                handler->onKeyPress(key);
            } else if (action == GLFW_RELEASE) {
                handler->onKeyRelease(key);
            } else if (action == GLFW_REPEAT) {
                handler->onKeyRepeat(key);
            }
        }
    });

    // Character input callback (for text entry)
    glfwSetCharCallback(handle, [](GLFWwindow* window, unsigned int codepoint) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) {
            return;
        }

        InputHandler* handler = nullptr;
        Scene* focusedScene = game->getFocusedScene();
        if (focusedScene && focusedScene->getInputHandler()) {
            handler = focusedScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }

        if (handler) {
            handler->onCharInput(codepoint);
        }
    });

    // Mouse button callback
    glfwSetMouseButtonCallback(handle, [](GLFWwindow* window, int button, int action, int mods) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) {
            return;
        }

        InputHandler* handler = nullptr;
        if (game->m_activeScene && game->m_activeScene->getInputHandler()) {
            handler = game->m_activeScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }

        if (handler) {
            (void)mods;  // Modifier keys available but not passed to simple interface
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (action == GLFW_PRESS) {
                handler->onMouseButtonPress(button, xpos, ypos);
            } else if (action == GLFW_RELEASE) {
                handler->onMouseButtonRelease(button, xpos, ypos);
            }
        }
    });

    // Mouse move callback
    glfwSetCursorPosCallback(handle, [](GLFWwindow* window, double xpos, double ypos) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) {
            return;
        }

        InputHandler* handler = nullptr;
        if (game->m_activeScene && game->m_activeScene->getInputHandler()) {
            handler = game->m_activeScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }

        if (handler) {
            handler->onMouseMove(xpos, ypos);
        }
    });

    // Mouse scroll callback
    glfwSetScrollCallback(handle, [](GLFWwindow* window, double xoffset, double yoffset) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) {
            return;
        }

        InputHandler* handler = nullptr;
        if (game->m_activeScene && game->m_activeScene->getInputHandler()) {
            handler = game->m_activeScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }

        if (handler) {
            handler->onMouseScroll(xoffset, yoffset);
        }
    });

    // Focus callback
    glfwSetWindowFocusCallback(handle, [](GLFWwindow* window, int focused) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (game && game->m_focusCallback) {
            game->m_focusCallback(focused != 0);
        }
    });

    // Window resize callback (for user-driven OS resize)
    glfwSetWindowSizeCallback(handle, [](GLFWwindow* window, int width, int height) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game || !game->m_window) {
            return;
        }

        if (width <= 0 || height <= 0) {
            return;
        }

        game->m_window->handleExternalResize(static_cast<uint32_t>(width),
                                             static_cast<uint32_t>(height));
    });

    // Joystick/gamepad connection callback
    // Note: GLFW joystick callbacks are global (not per-window), so we use a static
    // pointer to route events. This is safe because VDE only supports one Game instance.
    static Game* s_gameForJoystick = nullptr;
    s_gameForJoystick = this;

    glfwSetJoystickCallback([](int jid, int event) {
        Game* game = s_gameForJoystick;
        if (!game) {
            return;
        }

        auto notifyHandler = [&](InputHandler* handler) {
            if (event == GLFW_CONNECTED) {
                const char* name = glfwGetJoystickName(jid);
                bool isGamepad = glfwJoystickIsGamepad(jid) == GLFW_TRUE;
                const char* displayName = isGamepad ? glfwGetGamepadName(jid) : name;
                handler->_setGamepadConnected(jid, true);
                handler->onGamepadConnect(jid, displayName ? displayName : "Unknown");
            } else if (event == GLFW_DISCONNECTED) {
                handler->_setGamepadConnected(jid, false);
                handler->onGamepadDisconnect(jid);
                // Clear the state for this gamepad
                for (int btn = 0; btn <= GAMEPAD_BUTTON_LAST; ++btn) {
                    handler->_setGamepadButton(jid, btn, false);
                }
                for (int axis = 0; axis <= GAMEPAD_AXIS_LAST; ++axis) {
                    handler->_setGamepadAxis(jid, axis, 0.0f);
                }
            }
        };

        if (game->m_inputHandler) {
            notifyHandler(game->m_inputHandler);
        }
        for (const auto& [sceneName, scene] : game->m_scenes) {
            InputHandler* sceneHandler = scene->getInputHandler();
            if (sceneHandler && sceneHandler != game->m_inputHandler) {
                notifyHandler(sceneHandler);
            }
        }
    });

    // Initialize connection state for gamepads that are already plugged in
    for (int jid = JOYSTICK_1; jid <= JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) == GLFW_TRUE) {
            bool isGamepad = glfwJoystickIsGamepad(jid) == GLFW_TRUE;
            const char* name = isGamepad ? glfwGetGamepadName(jid) : glfwGetJoystickName(jid);

            auto initHandler = [&](InputHandler* handler) {
                handler->_setGamepadConnected(jid, true);
                handler->onGamepadConnect(jid, name ? name : "Unknown");
            };

            if (m_inputHandler) {
                initHandler(m_inputHandler);
            }
        }
    }
}

void Game::createMeshRenderingPipeline() {
    std::cout << "Creating mesh rendering pipeline..." << '\n';

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << '\n';
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();
    std::cout << "Got device" << '\n';

    // Compile shaders
    ShaderCompiler compiler;
    std::vector<uint32_t> vertSpv, fragSpv;

    std::cout << "Compiling vertex shader..." << '\n';
    auto vertResult = compiler.compileFile("shaders/mesh.vert", ShaderStage::Vertex);
    if (!vertResult.success) {
        std::cerr << "Vertex shader compilation failed: " << vertResult.errorLog << '\n';
        throw std::runtime_error("Failed to compile mesh vertex shader: " + vertResult.errorLog);
    }
    vertSpv = vertResult.spirv;
    std::cout << "Vertex shader compiled successfully (" << vertSpv.size() << " words)" << '\n';

    auto fragResult = compiler.compileFile("shaders/mesh.frag", ShaderStage::Fragment);
    if (!fragResult.success) {
        throw std::runtime_error("Failed to compile mesh fragment shader: " + fragResult.errorLog);
    }
    fragSpv = fragResult.spirv;

    // Create shader modules
    VkShaderModule vertShaderModule = m_vulkanContext->createShaderModule(std::vector<char>(
        reinterpret_cast<char*>(vertSpv.data()),
        reinterpret_cast<char*>(vertSpv.data()) + vertSpv.size() * sizeof(uint32_t)));
    VkShaderModule fragShaderModule = m_vulkanContext->createShaderModule(std::vector<char>(
        reinterpret_cast<char*>(fragSpv.data()),
        reinterpret_cast<char*>(fragSpv.data()) + fragSpv.size() * sizeof(uint32_t)));

    // Shader stage creation
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Descriptor set layout (for view/projection UBO)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_meshDescriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create mesh descriptor set layout");
    }

    // Descriptor set layout (for mesh texture sampler at set 2)
    VkDescriptorSetLayoutBinding textureLayoutBinding{};
    textureLayoutBinding.binding = 0;
    textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureLayoutBinding.descriptorCount = 1;
    textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.bindingCount = 1;
    textureLayoutInfo.pBindings = &textureLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr,
                                    &m_meshTextureDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mesh texture descriptor set layout");
    }

    // Create descriptor pool for mesh texture descriptor sets
    VkDescriptorPoolSize meshTexturePoolSize{};
    meshTexturePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    meshTexturePoolSize.descriptorCount = 1024;

    VkDescriptorPoolCreateInfo meshTexturePoolInfo{};
    meshTexturePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    meshTexturePoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    meshTexturePoolInfo.poolSizeCount = 1;
    meshTexturePoolInfo.pPoolSizes = &meshTexturePoolSize;
    meshTexturePoolInfo.maxSets = 1024;

    if (vkCreateDescriptorPool(device, &meshTexturePoolInfo, nullptr,
                               &m_meshTextureDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mesh texture descriptor pool");
    }

    // Push constant range (for model matrix + material properties)
    // Size: 64 (mat4) + 48 (MaterialPushConstants) = 112 bytes
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + sizeof(MaterialPushConstants);

    // Pipeline layout with descriptor set layouts
    // Set 0: UBO (view/projection), Set 1: Lighting UBO, Set 2: Texture sampler
    std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts = {
        m_meshDescriptorSetLayout, m_lightingDescriptorSetLayout, m_meshTextureDescriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_meshPipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create mesh pipeline layout");
    }

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_meshPipelineLayout;
    pipelineInfo.renderPass = m_vulkanContext->getRenderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_meshPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mesh graphics pipeline");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Game::destroyMeshRenderingPipeline() {
    if (!m_vulkanContext) {
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();

    if (m_meshPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_meshPipeline, nullptr);
        m_meshPipeline = VK_NULL_HANDLE;
    }

    if (m_meshPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_meshPipelineLayout, nullptr);
        m_meshPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_meshDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_meshDescriptorSetLayout, nullptr);
        m_meshDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_meshTextureDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_meshTextureDescriptorSetLayout, nullptr);
        m_meshTextureDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_meshTextureDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_meshTextureDescriptorPool, nullptr);
        m_meshTextureDescriptorPool = VK_NULL_HANDLE;
    }
}

void Game::createSpriteRenderingPipeline() {
    std::cout << "Creating sprite rendering pipeline..." << '\n';

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << '\n';
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();

    // Create sampler for sprites
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_spriteSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sprite sampler");
    }

    // Compile shaders
    ShaderCompiler compiler;

    std::cout << "Compiling sprite vertex shader..." << '\n';
    auto vertResult = compiler.compileFile("shaders/simple_sprite.vert", ShaderStage::Vertex);
    if (!vertResult.success) {
        std::cerr << "Sprite vertex shader compilation failed: " << vertResult.errorLog << '\n';
        throw std::runtime_error("Failed to compile sprite vertex shader: " + vertResult.errorLog);
    }
    std::cout << "Sprite vertex shader compiled successfully" << '\n';

    auto fragResult = compiler.compileFile("shaders/simple_sprite.frag", ShaderStage::Fragment);
    if (!fragResult.success) {
        throw std::runtime_error("Failed to compile sprite fragment shader: " +
                                 fragResult.errorLog);
    }

    // Create shader modules
    VkShaderModule vertShaderModule = m_vulkanContext->createShaderModule(
        std::vector<char>(reinterpret_cast<char*>(vertResult.spirv.data()),
                          reinterpret_cast<char*>(vertResult.spirv.data()) +
                              vertResult.spirv.size() * sizeof(uint32_t)));
    VkShaderModule fragShaderModule = m_vulkanContext->createShaderModule(
        std::vector<char>(reinterpret_cast<char*>(fragResult.spirv.data()),
                          reinterpret_cast<char*>(fragResult.spirv.data()) +
                              fragResult.spirv.size() * sizeof(uint32_t)));

    // Shader stage creation
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input - uses same Vertex structure as mesh
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer - no culling for sprites (they may be flipped)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling for sprites
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil (no depth test for sprites - render order matters)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Color blending - alpha blending for sprites
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Descriptor set layout (for UBO and texture sampler)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    // Binding 0: UBO (view/projection)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Texture sampler
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_spriteDescriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create sprite descriptor set layout");
    }

    // Create descriptor pool for sprite descriptor sets
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1024;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1024;
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1024;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_spriteDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sprite descriptor pool");
    }

    // Push constant range (for model matrix, tint, and UV rect)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size =
        sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec4);  // model + tint + uvRect

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_spriteDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_spritePipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create sprite pipeline layout");
    }

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_spritePipelineLayout;
    pipelineInfo.renderPass = m_vulkanContext->getRenderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_spritePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sprite graphics pipeline");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    // Create default white texture (1x1 pixel)
    m_defaultWhiteTexture = std::make_unique<Texture>();
    uint8_t whitePixel[4] = {255, 255, 255, 255};  // RGBA white
    if (!m_defaultWhiteTexture->createFromData(
            whitePixel, 1, 1, device, m_vulkanContext->getPhysicalDevice(),
            m_vulkanContext->getCommandPool(), m_vulkanContext->getGraphicsQueue())) {
        std::cerr << "Warning: Failed to create default white texture" << '\n';
        m_defaultWhiteTexture.reset();
    }

    std::cout << "Sprite rendering pipeline created successfully" << '\n';
}

void Game::destroySpriteRenderingPipeline() {
    if (!m_vulkanContext) {
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();

    // Clean up default white texture
    if (m_defaultWhiteTexture) {
        m_defaultWhiteTexture.reset();
    }

    if (m_spritePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_spritePipeline, nullptr);
        m_spritePipeline = VK_NULL_HANDLE;
    }

    if (m_spritePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_spritePipelineLayout, nullptr);
        m_spritePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_spriteDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_spriteDescriptorSetLayout, nullptr);
        m_spriteDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_spriteDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_spriteDescriptorPool, nullptr);
        m_spriteDescriptorPool = VK_NULL_HANDLE;
    }

    if (m_spriteSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_spriteSampler, nullptr);
        m_spriteSampler = VK_NULL_HANDLE;
    }
}

VkDescriptorSet Game::allocateSpriteDescriptorSet() {
    if (!m_vulkanContext || m_spriteDescriptorPool == VK_NULL_HANDLE ||
        m_spriteDescriptorSetLayout == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_spriteDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_spriteDescriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(m_vulkanContext->getDevice(), &allocInfo, &descriptorSet) !=
        VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return descriptorSet;
}

void Game::updateSpriteDescriptor(VkDescriptorSet descriptorSet, VkBuffer uboBuffer,
                                  VkDeviceSize uboSize, VkImageView imageView, VkSampler sampler) {
    if (!m_vulkanContext || descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uboBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = uboSize;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    // Binding 0: UBO
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    // Binding 1: Texture sampler
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    vkUpdateDescriptorSets(m_vulkanContext->getDevice(),
                           static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(),
                           0, nullptr);
}

VkDescriptorSet Game::allocateMeshTextureDescriptorSet() {
    if (!m_vulkanContext || m_meshTextureDescriptorPool == VK_NULL_HANDLE ||
        m_meshTextureDescriptorSetLayout == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_meshTextureDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_meshTextureDescriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(m_vulkanContext->getDevice(), &allocInfo, &descriptorSet) !=
        VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return descriptorSet;
}

void Game::updateMeshTextureDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView,
                                       VkSampler sampler) {
    if (!m_vulkanContext || descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_vulkanContext->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

// ============================================================================
// Lighting Resources (Phase 4)
// ============================================================================

void Game::createLightingResources() {
    std::cout << "Creating lighting resources..." << '\n';

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << '\n';
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();
    constexpr uint32_t framesInFlight = 2;  // MAX_FRAMES_IN_FLIGHT

    // Create lighting descriptor set layout (Set 1: Lighting UBO)
    VkDescriptorSetLayoutBinding lightingBinding{};
    lightingBinding.binding = 0;
    lightingBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingBinding.descriptorCount = 1;
    lightingBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &lightingBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_lightingDescriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor set layout");
    }

    // Create descriptor pool for lighting UBO
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_lightingDescriptorPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor pool");
    }

    // Create lighting UBO buffers (one per frame)
    VkDeviceSize bufferSize = sizeof(LightingUBO);
    m_lightingUBOBuffers.resize(framesInFlight);
    m_lightingUBOMemory.resize(framesInFlight);
    m_lightingUBOMapped.resize(framesInFlight);

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (uint32_t i = 0; i < framesInFlight; i++) {
        BufferUtils::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  m_lightingUBOBuffers[i], m_lightingUBOMemory[i]);

        // Persistently map the buffer
        vkMapMemory(device, m_lightingUBOMemory[i], 0, bufferSize, 0, &m_lightingUBOMapped[i]);
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_lightingDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_lightingDescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    m_lightingDescriptorSets.resize(framesInFlight);
    if (vkAllocateDescriptorSets(device, &allocInfo, m_lightingDescriptorSets.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate lighting descriptor sets");
    }

    // Update descriptor sets to point to UBO buffers
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (uint32_t i = 0; i < framesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_lightingUBOBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = bufferSize;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_lightingDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    // Initialize with default lighting (white ambient)
    LightingUBO defaultLighting{};
    defaultLighting.ambientColorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f);
    defaultLighting.lightCounts = glm::ivec4(0, 0, 0, 0);

    for (uint32_t i = 0; i < framesInFlight; i++) {
        memcpy(m_lightingUBOMapped[i], &defaultLighting, sizeof(LightingUBO));
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    std::cout << "Lighting resources created successfully" << '\n';
}

void Game::destroyLightingResources() {
    if (!m_vulkanContext) {
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();

    // Unmap and destroy UBO buffers
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (size_t i = 0; i < m_lightingUBOBuffers.size(); i++) {
        if (m_lightingUBOMapped[i]) {
            vkUnmapMemory(device, m_lightingUBOMemory[i]);
        }
        if (m_lightingUBOBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_lightingUBOBuffers[i], nullptr);
        }
        if (m_lightingUBOMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, m_lightingUBOMemory[i], nullptr);
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    m_lightingUBOBuffers.clear();
    m_lightingUBOMemory.clear();
    m_lightingUBOMapped.clear();

    // Descriptor sets are freed when pool is destroyed
    m_lightingDescriptorSets.clear();

    if (m_lightingDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_lightingDescriptorPool, nullptr);
        m_lightingDescriptorPool = VK_NULL_HANDLE;
    }

    if (m_lightingDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_lightingDescriptorSetLayout, nullptr);
        m_lightingDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

VkDescriptorSet Game::getCurrentLightingDescriptorSet() const {
    if (m_lightingDescriptorSets.empty() || !m_vulkanContext) {
        return VK_NULL_HANDLE;
    }
    uint32_t currentFrame = m_vulkanContext->getCurrentFrame();
    if (currentFrame >= m_lightingDescriptorSets.size()) {
        currentFrame = 0;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return m_lightingDescriptorSets[currentFrame];
}

void Game::updateLightingUBO(const Scene* scene) {
    if (!m_vulkanContext || m_lightingUBOMapped.empty()) {
        return;
    }

    uint32_t currentFrame = m_vulkanContext->getCurrentFrame();
    if (currentFrame >= m_lightingUBOMapped.size()) {
        currentFrame = 0;
    }

    LightingUBO ubo{};

    if (scene) {
        const LightBox& lightBox = scene->getEffectiveLighting();

        // Set ambient
        const Color& ambient = lightBox.getAmbientColor();
        ubo.ambientColorAndIntensity =
            glm::vec4(ambient.r, ambient.g, ambient.b, lightBox.getAmbientIntensity());

        // Convert lights
        const std::vector<Light>& lights = lightBox.getLights();
        uint32_t numLights =
            static_cast<uint32_t>(std::min(lights.size(), static_cast<size_t>(MAX_LIGHTS)));
        ubo.lightCounts = glm::ivec4(static_cast<int>(numLights), 0, 0, 0);

        for (uint32_t i = 0; i < numLights; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            const Light& light = lights[i];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            GPULight& gpuLight = ubo.lights[i];

            // Position/direction and type
            if (light.type == LightType::Directional) {
                gpuLight.positionAndType =
                    glm::vec4(light.direction.x, light.direction.y, light.direction.z, 0.0f);
            } else {
                gpuLight.positionAndType =
                    glm::vec4(light.position.x, light.position.y, light.position.z,
                              static_cast<float>(static_cast<int>(light.type)));
            }

            // Direction and range
            gpuLight.directionAndRange =
                glm::vec4(light.direction.x, light.direction.y, light.direction.z, light.range);

            // Color and intensity
            gpuLight.colorAndIntensity =
                glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);

            // Spot params (cosines of angles)
            gpuLight.spotParams =
                glm::vec4(std::cos(glm::radians(light.spotAngle)),
                          std::cos(glm::radians(light.spotOuterAngle)), 0.0f, 0.0f);
        }
    } else {
        // Default: white ambient, no lights
        ubo.ambientColorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f);
        ubo.lightCounts = glm::ivec4(0, 0, 0, 0);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    memcpy(m_lightingUBOMapped[currentFrame], &ubo, sizeof(LightingUBO));
}

void Game::setFocusedScene(const std::string& sceneName) {
    // Clear isFocused on the previously focused scene
    if (!m_focusedSceneName.empty()) {
        auto prevIt = m_scenes.find(m_focusedSceneName);
        if (prevIt != m_scenes.end()) {
            prevIt->second->m_diagnostics.isFocused = false;
        }
    } else if (m_activeScene) {
        m_activeScene->m_diagnostics.isFocused = false;
    }

    m_focusedSceneName = sceneName;

    // Set isFocused on the new focused scene
    if (!sceneName.empty()) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) {
            it->second->m_diagnostics.isFocused = true;
        }
    } else if (m_activeScene) {
        m_activeScene->m_diagnostics.isFocused = true;
    }
}

Scene* Game::getFocusedScene() {
    if (!m_focusedSceneName.empty()) {
        auto it = m_scenes.find(m_focusedSceneName);
        if (it != m_scenes.end()) {
            return it->second.get();
        }
    }
    return m_activeScene;  // Default to primary scene
}

const Scene* Game::getFocusedScene() const {
    if (!m_focusedSceneName.empty()) {
        auto it = m_scenes.find(m_focusedSceneName);
        if (it != m_scenes.end()) {
            return it->second.get();
        }
    }
    return m_activeScene;
}

Scene* Game::getSceneAtScreenPosition(double mouseX, double mouseY) {
    if (!m_window) {
        return nullptr;
    }

    // Convert pixel coords to normalized [0,1]
    uint32_t winW = m_settings.display.windowWidth;
    uint32_t winH = m_settings.display.windowHeight;
    if (winW == 0 || winH == 0) {
        return nullptr;
    }

    float nx = static_cast<float>(mouseX) / static_cast<float>(winW);
    float ny = static_cast<float>(mouseY) / static_cast<float>(winH);

    // Check each scene in the active group (reverse order for overlays)
    for (auto it = m_activeSceneGroup.sceneNames.rbegin();
         it != m_activeSceneGroup.sceneNames.rend(); ++it) {
        auto sceneIt = m_scenes.find(*it);
        if (sceneIt != m_scenes.end()) {
            if (sceneIt->second->getViewportRect().contains(nx, ny)) {
                return sceneIt->second.get();
            }
        }
    }

    return nullptr;
}

void Game::renderSingleViewport() {
    // Apply primary scene's camera to VulkanContext (backwards compatible path)
    if (m_activeScene && m_activeScene->getCamera()) {
        m_activeScene->getCamera()->applyTo(*m_vulkanContext);
    }

    m_vulkanContext->setRenderCallback([this](VkCommandBuffer cmd) {
        (void)cmd;
        // Render all scenes in the active group
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            auto it = m_scenes.find(sceneName);
            if (it != m_scenes.end()) {
                it->second->render();
            }
        }
        onRender();
    });

    m_vulkanContext->drawFrame();
}

void Game::renderMultiViewport() {
    VkExtent2D extent = m_vulkanContext->getSwapChainExtent();

    std::vector<VulkanContext::SceneRenderInfo> renderInfos;

    for (size_t i = 0; i < m_activeSceneGroup.sceneNames.size(); ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        const auto& sceneName = m_activeSceneGroup.sceneNames[i];
        auto it = m_scenes.find(sceneName);
        if (it == m_scenes.end()) {
            continue;
        }

        Scene* scene = it->second.get();

        VulkanContext::SceneRenderInfo info{};
        info.clearPass = (i == 0);

        // Compute viewport and scissor from the scene's ViewportRect
        const ViewportRect& vpRect = scene->getViewportRect();
        info.viewport = vpRect.toVkViewport(extent.width, extent.height);
        info.scissor = vpRect.toVkScissor(extent.width, extent.height);

        // Get the scene's camera matrices
        if (scene->getCamera()) {
            // Set aspect ratio BEFORE applying camera so projection is correct
            float vpAspect = vpRect.getAspectRatio(extent.width, extent.height);
            scene->getCamera()->setAspectRatio(vpAspect);

            // Apply camera to get internal state updated (positions etc.)
            scene->getCamera()->applyTo(*m_vulkanContext);
            info.viewMatrix = m_vulkanContext->getCamera().getViewMatrix();
            info.projMatrix = m_vulkanContext->getCamera().getProjectionMatrix();
        } else {
            info.viewMatrix = glm::mat4(1.0f);
            info.projMatrix = glm::mat4(1.0f);
        }

        // Update lighting for this scene
        updateLightingUBO(scene);

        // Capture scene pointer for the lambda
        info.renderCallback = [this, scene](VkCommandBuffer cmd) {
            (void)cmd;
            scene->render();
        };

        renderInfos.push_back(std::move(info));
    }

    // Add the onRender callback to the last scene's render
    if (!renderInfos.empty()) {
        auto originalCallback = renderInfos.back().renderCallback;
        renderInfos.back().renderCallback = [this, originalCallback](VkCommandBuffer cmd) {
            if (originalCallback) {
                originalCallback(cmd);
            }
            onRender();
        };
    }

    m_vulkanContext->drawFrameMultiScene(renderInfos);
}

void Game::renderTransition() {
    m_vulkanContext->drawFrameCustom([this](VkCommandBuffer cmd, VkFramebuffer swapchainFB,
                                            VkImage swapchainImage) {
        VkExtent2D extent = m_vulkanContext->getSwapChainExtent();

        // Helper: render a scene into an offscreen framebuffer
        auto renderSceneOffscreen = [&](const std::string& sceneName, VkFramebuffer framebuffer) {
            auto it = m_scenes.find(sceneName);
            if (it == m_scenes.end()) {
                return;
            }
            Scene* scene = it->second.get();

            // Apply camera so UBO is correct
            if (scene->getCamera()) {
                scene->getCamera()->applyTo(*m_vulkanContext);
            }

            // Update UBO via vkCmdUpdateBuffer (outside render pass)
            UniformBufferObject ubo{};
            ubo.model = glm::mat4(1.0f);
            ubo.view = m_vulkanContext->getCamera().getViewMatrix();
            ubo.proj = m_vulkanContext->getCamera().getProjectionMatrix();

            // NOTE: getCurrentUniformBuffer() returns the same shared UBO buffer for both the
            // source and destination offscreen passes. The second call (dest scene) overwrites the
            // buffer, but by that point the source render pass has already ended, so correctness is
            // maintained for this sequential single-command-buffer design. If rendering is ever
            // parallelised across command buffers, this shared UBO would become a data race.
            VkBuffer uboBuffer = m_vulkanContext->getCurrentUniformBuffer();
            vkCmdUpdateBuffer(cmd, uboBuffer, 0, sizeof(UniformBufferObject), &ubo);

            // Barrier: transfer write → uniform read
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = uboBuffer;
            barrier.offset = 0;
            barrier.size = sizeof(UniformBufferObject);

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0,
                                 nullptr);

            // Begin offscreen render pass
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_vulkanContext->getOffscreenRenderPass();
            rpInfo.framebuffer = framebuffer;
            rpInfo.renderArea.offset = {.x = 0, .y = 0};
            rpInfo.renderArea.extent = extent;

            std::array<VkClearValue, 2> clearValues{};
            const Color& bg = scene->getBackgroundColor();
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            clearValues[0].color = {{bg.r, bg.g, bg.b, bg.a}};
            clearValues[1].depthStencil = {.depth = 1.0f, .stencil = 0};
            // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            rpInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Full viewport
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(extent.width);
            viewport.height = static_cast<float>(extent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {.x = 0, .y = 0};
            scissor.extent = extent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Update lighting for this scene
            updateLightingUBO(scene);

            // Render the scene
            scene->render();

            vkCmdEndRenderPass(cmd);
        };

        // Pass 1: Render source scene to offscreen target A
        renderSceneOffscreen(m_transitionSourceScene, m_transitionManager->getSourceFramebuffer());

        // Pass 2: Render dest scene to offscreen target B
        renderSceneOffscreen(m_transitionDestScene, m_transitionManager->getDestFramebuffer());

        // Pass 3: Composite to swapchain framebuffer
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_vulkanContext->getRenderPass();
        rpInfo.framebuffer = swapchainFB;
        rpInfo.renderArea.offset = {.x = 0, .y = 0};
        rpInfo.renderArea.extent = extent;

        std::array<VkClearValue, 2> clearValues{};
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {.depth = 1.0f, .stencil = 0};
        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // If viewport-scoped transition, render non-transitioning scenes first
        if (m_viewportTransition) {
            for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
                // Skip the source and dest scenes of the transition
                if (sceneName == m_transitionSourceScene || sceneName == m_transitionDestScene) {
                    continue;
                }

                auto sceneIt = m_scenes.find(sceneName);
                if (sceneIt == m_scenes.end()) {
                    continue;
                }

                Scene* scene = sceneIt->second.get();

                // Apply camera
                if (scene->getCamera()) {
                    scene->getCamera()->applyTo(*m_vulkanContext);
                }

                // Update UBO for this scene
                UniformBufferObject sceneUbo{};
                sceneUbo.model = glm::mat4(1.0f);
                sceneUbo.view = m_vulkanContext->getCamera().getViewMatrix();
                sceneUbo.proj = m_vulkanContext->getCamera().getProjectionMatrix();

                VkBuffer sceneUboBuffer = m_vulkanContext->getCurrentUniformBuffer();
                vkCmdUpdateBuffer(cmd, sceneUboBuffer, 0, sizeof(UniformBufferObject), &sceneUbo);

                VkBufferMemoryBarrier sceneBarrier{};
                sceneBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                sceneBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sceneBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
                sceneBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                sceneBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                sceneBarrier.buffer = sceneUboBuffer;
                sceneBarrier.offset = 0;
                sceneBarrier.size = sizeof(UniformBufferObject);

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1,
                                     &sceneBarrier, 0, nullptr);

                // Set viewport/scissor for this scene
                const ViewportRect& vpRect = scene->getViewportRect();
                VkViewport sceneVp = vpRect.toVkViewport(extent.width, extent.height);
                VkRect2D sceneSc = vpRect.toVkScissor(extent.width, extent.height);
                vkCmdSetViewport(cmd, 0, 1, &sceneVp);
                vkCmdSetScissor(cmd, 0, 1, &sceneSc);

                updateLightingUBO(scene);
                scene->render();
            }

            // Set scissor to the transition viewport region for composite
            VkViewport transVp = m_transitionViewport.toVkViewport(extent.width, extent.height);
            VkRect2D transSc = m_transitionViewport.toVkScissor(extent.width, extent.height);
            vkCmdSetViewport(cmd, 0, 1, &transVp);
            vkCmdSetScissor(cmd, 0, 1, &transSc);
        }

        // Render the composite (pipeline bind, descriptor set, push constants, draw)
        m_transitionManager->renderComposite(cmd);

        // Call onRender() hook (ImGui overlays etc.)
        onRender();

        vkCmdEndRenderPass(cmd);

        // Transition swapchain image layout to PRESENT_SRC_KHR
        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        presentBarrier.dstAccessMask = 0;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        presentBarrier.image = swapchainImage;
        presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presentBarrier.subresourceRange.baseMipLevel = 0;
        presentBarrier.subresourceRange.levelCount = 1;
        presentBarrier.subresourceRange.baseArrayLayer = 0;
        presentBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &presentBarrier);
    });
}

void Game::activateScene(const std::string& name) {
    if (m_activeSceneNames.count(name) > 0) {
        return;  // Already active — prevent double-enter
    }
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        return;
    }
    it->second->m_diagnostics.enterCount++;
    m_activeSceneNames.insert(name);
    it->second->onEnter();
}

void Game::deactivateScene(const std::string& name) {
    if (m_activeSceneNames.count(name) == 0) {
        return;  // Not active — prevent double-exit
    }
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        m_activeSceneNames.erase(name);  // Clean up stale entry
        return;
    }
    it->second->m_diagnostics.isFocused = false;
    it->second->m_diagnostics.exitCount++;
    m_activeSceneNames.erase(name);
    it->second->onExit();
}

void Game::rebuildSchedulerGraph() {
    m_scheduler.clear();

    // ---------------------------------------------------------------
    // Collect scenes that need updating this frame:
    //   1. All scenes in the active group
    //   2. Any scene outside the group with continueInBackground==true
    // ---------------------------------------------------------------
    struct SceneEntry {
        Scene* scene = nullptr;
        std::string name;
        int priority = 0;
    };

    std::vector<SceneEntry> updateScenes;

    // Check whether the active transition wants the source scene frozen.
    bool freezeSource = m_transitionManager && m_transitionManager->isActive() &&
                        m_transitionManager->getActiveTransition() &&
                        m_transitionManager->getActiveTransition()->freezesSourceScene();

    // Active group scenes
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        // Skip updating the source scene when the transition freezes it.
        if (freezeSource && sceneName == m_transitionSourceScene) {
            continue;
        }
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) {
            updateScenes.push_back({.scene = it->second.get(),
                                    .name = sceneName,
                                    .priority = it->second->getUpdatePriority()});
        }
    }

    // Background scenes (not already in the group)
    for (auto& [name, scenePtr] : m_scenes) {
        if (!scenePtr->getContinueInBackground()) {
            continue;
        }
        // Skip if already in the active group
        bool inGroup = false;
        for (const auto& gn : m_activeSceneGroup.sceneNames) {
            if (gn == name) {
                inGroup = true;
                break;
            }
        }
        if (!inGroup) {
            updateScenes.push_back(
                {.scene = scenePtr.get(), .name = name, .priority = scenePtr->getUpdatePriority()});
        }
    }

    // During transitions, ensure the destination scene is also updated
    if (m_transitionManager && m_transitionManager->isActive() && !m_transitionDestScene.empty()) {
        bool destAlreadyListed = false;
        for (const auto& entry : updateScenes) {
            if (entry.name == m_transitionDestScene) {
                destAlreadyListed = true;
                break;
            }
        }
        if (!destAlreadyListed) {
            auto it = m_scenes.find(m_transitionDestScene);
            if (it != m_scenes.end()) {
                updateScenes.push_back({.scene = it->second.get(),
                                        .name = m_transitionDestScene,
                                        .priority = it->second->getUpdatePriority()});
            }
        }
    }

    // Sort deterministically: priority ascending, then name lexicographically as tiebreaker.
    // Deterministic ordering is required because execution order must not change across
    // scheduler rebuilds when priority is equal — but it must NOT create false cross-scene
    // task dependencies. Ordering only controls task registration sequence, not edges.
    std::ranges::sort(updateScenes, [](const SceneEntry& a, const SceneEntry& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        return a.name < b.name;
    });

    // ---------------------------------------------------------------
    // Task 0: Input — process input script commands.
    //         Runs in the Input phase (before GameLogic) so scripted
    //         input is dispatched before any game logic reads it.
    //         Main-thread-only: scripted input mutates input state.
    // ---------------------------------------------------------------
    TaskId inputScriptTask = m_scheduler.addTask({.name = "input.script",
                                                  .phase = TaskPhase::Input,
                                                  .work = [this]() { processInputScript(); },
                                                  .dependsOn = {},
                                                  .mainThreadOnly = true});

    // ---------------------------------------------------------------
    // Task 0b: Window/OS operations — execute queued window changes.
    //          Main-thread-only: window/OS APIs require the main thread.
    // ---------------------------------------------------------------
    TaskId windowOpsTask =
        m_scheduler.addTask({.name = "window.ops",
                             .phase = TaskPhase::Input,
                             .work = [this]() { executePendingWindowOperations(); },
                             .dependsOn = {inputScriptTask},
                             .mainThreadOnly = true});

    // ---------------------------------------------------------------
    // Task 1: game.update — onUpdate hook.
    //         Main-thread-only: onUpdate may touch scene objects.
    // ---------------------------------------------------------------
    TaskId gameUpdateTask = m_scheduler.addTask({.name = "game.update",
                                                 .phase = TaskPhase::GameLogic,
                                                 .work = [this]() { onUpdate(m_deltaTime); },
                                                 .dependsOn = {windowOpsTask},
                                                 .mainThreadOnly = true});

    // Per-scene barrier collections:
    //   audioBarrierTasks — tasks audio.global must wait for
    //   finalVisualTasks  — tasks preRender must wait for (all final per-scene writes)
    std::vector<TaskId> audioBarrierTasks;
    std::vector<TaskId> finalVisualTasks;

    // ---------------------------------------------------------------
    // Per-scene task chains — no cross-scene prevTask chain.
    // Each scene's chain depends only on game.update and its own
    // prior phase tasks, so scenes are independent of each other.
    // ---------------------------------------------------------------
    for (auto& updateScene : updateScenes) {
        Scene* scene = updateScene.scene;
        const std::string& sceneName = updateScene.name;

        if (scene->usesPhaseCallbacks()) {
            // --- Phase callbacks mode ---

            // GameLogic — depends only on game.update (no cross-scene edge)
            TaskId gameLogicTask = m_scheduler.addTask(
                {.name = "scene.gameLogic." + sceneName,
                 .phase = TaskPhase::GameLogic,
                 .work = [this, scene]() { scene->updateGameLogic(m_deltaTime); },
                 .dependsOn = {gameUpdateTask},
                 .mainThreadOnly = true});

            // Physics chain (optional) — depends only on this scene's gameLogic
            TaskId visualDep = gameLogicTask;
            if (scene->hasPhysics()) {
                // Sub-phase 1: Integration — worker-eligible (no callbacks, no shared state)
                TaskId physicsIntegrateTask = m_scheduler.addTask({
                    .name = "scene.physics.integrate." + sceneName,
                    .phase = TaskPhase::Physics,
                    .work = [this,
                             scene]() { scene->getPhysicsScene()->integrationStep(m_deltaTime); },
                    .dependsOn = {gameLogicTask},
                    .mainThreadOnly = false  // worker-eligible
                });

                // Sub-phase 2: Broad-phase AABB detection — worker-eligible
                TaskId physicsBroadPhaseTask = m_scheduler.addTask({
                    .name = "scene.physics.broadPhase." + sceneName,
                    .phase = TaskPhase::Physics,
                    .work = [scene]() { scene->getPhysicsScene()->broadPhaseStep(); },
                    .dependsOn = {physicsIntegrateTask},
                    .mainThreadOnly = false  // worker-eligible
                });

                // Sub-phase 3: Impulse resolution + event staging — worker-eligible
                TaskId physicsResolveTask = m_scheduler.addTask({
                    .name = "scene.physics.resolve." + sceneName,
                    .phase = TaskPhase::Physics,
                    .work = [scene]() { scene->getPhysicsScene()->resolveStep(); },
                    .dependsOn = {physicsBroadPhaseTask},
                    .mainThreadOnly = false  // worker-eligible
                });

                // PostPhysics — main-thread: drain staged collision callbacks,
                // then sync interpolated transforms to physics entities.
                TaskId postPhysicsTask = m_scheduler.addTask(
                    {.name = "scene.postPhysics." + sceneName,
                     .phase = TaskPhase::PostPhysics,
                     .work =
                         [scene]() {
                             if (!scene->hasPhysics()) {
                                 return;
                             }
                             auto* ps = scene->getPhysicsScene();
                             // Dispatch staged collision events on the main thread.
                             ps->drainStagedEvents();
                             // Sync interpolated transforms.
                             float alpha = ps->getInterpolationAlpha();
                             for (auto& entityRef : scene->getEntities()) {
                                 auto* pe = dynamic_cast<PhysicsEntity*>(entityRef.get());
                                 if (pe && pe->getAutoSync()) {
                                     pe->syncFromPhysics(alpha);
                                 }
                             }
                         },
                     .dependsOn = {physicsResolveTask},
                     .mainThreadOnly = true});

                visualDep = postPhysicsTask;
            }

            // Timed events — Timed phase, after post-physics (or gameLogic when no physics).
            // Main-thread-only: timed callbacks may mutate scene state.
            TaskId timedTask = m_scheduler.addTask(
                {.name = "scene.timed." + sceneName,
                 .phase = TaskPhase::Timed,
                 .work = [this, scene]() { scene->getTimedEvents().tick(m_deltaTime); },
                 .dependsOn = {visualDep},
                 .mainThreadOnly = true});

            // Audio — depends on this scene's timed task (after post-physics).
            // Main-thread-only: audio callbacks may inspect scene state.
            TaskId sceneAudioTask =
                m_scheduler.addTask({.name = "scene.audio." + sceneName,
                                     .phase = TaskPhase::Audio,
                                     .work = [this, scene]() { scene->updateAudio(m_deltaTime); },
                                     .dependsOn = {timedTask},
                                     .mainThreadOnly = true});
            audioBarrierTasks.push_back(sceneAudioTask);

            // Visuals — Visual phase, after timed events.
            // Main-thread-only: updateVisuals() may mutate entity state.
            TaskId visualsTask =
                m_scheduler.addTask({.name = "scene.visuals." + sceneName,
                                     .phase = TaskPhase::Visual,
                                     .work =
                                         [this, scene]() {
                                             scene->updateVisuals(m_deltaTime);
                                             if (auto* camera = scene->getCamera()) {
                                                 camera->update(m_deltaTime);
                                             }
                                         },
                                     .dependsOn = {timedTask},
                                     .mainThreadOnly = true});

            // Animations — Visual phase, immediately after updateVisuals().
            // Main-thread-only: animation callbacks may mutate entity state.
            TaskId animationsTask = m_scheduler.addTask(
                {.name = "scene.animations." + sceneName,
                 .phase = TaskPhase::Visual,
                 .work = [this, scene]() { scene->animations().update(m_deltaTime); },
                 .dependsOn = {visualsTask},
                 .mainThreadOnly = true});
            finalVisualTasks.push_back(animationsTask);
        } else {
            // --- Legacy mode: single update task ---
            // update() covers logic + audio + visuals in one call.
            TaskId updateTask =
                m_scheduler.addTask({.name = "scene.update." + sceneName,
                                     .phase = TaskPhase::GameLogic,
                                     .work =
                                         [this, scene]() {
                                             scene->update(m_deltaTime);
                                             if (auto* camera = scene->getCamera()) {
                                                 camera->update(m_deltaTime);
                                             }
                                         },
                                     .dependsOn = {gameUpdateTask},
                                     .mainThreadOnly = true});

            // Timed events — run after the legacy update task.
            // Main-thread-only: timed callbacks may mutate scene state.
            TaskId timedTask = m_scheduler.addTask(
                {.name = "scene.timed." + sceneName,
                 .phase = TaskPhase::Timed,
                 .work = [this, scene]() { scene->getTimedEvents().tick(m_deltaTime); },
                 .dependsOn = {updateTask},
                 .mainThreadOnly = true});

            // Animations — Visual phase, after timed events (legacy: after update+timed).
            // Main-thread-only: animation callbacks may mutate entity state.
            TaskId animationsTask = m_scheduler.addTask(
                {.name = "scene.animations." + sceneName,
                 .phase = TaskPhase::Visual,
                 .work = [this, scene]() { scene->animations().update(m_deltaTime); },
                 .dependsOn = {timedTask},
                 .mainThreadOnly = true});

            // Audio barrier: legacy update task + timed task covers audio timing.
            audioBarrierTasks.push_back(timedTask);
            finalVisualTasks.push_back(animationsTask);
        }
    }

    // ---------------------------------------------------------------
    // audio.global — flush the audio manager after all per-scene audio.
    //   If no per-scene audio tasks exist (no phase-callback scenes),
    //   depend on game.update so this task is not orphaned.
    // ---------------------------------------------------------------
    if (audioBarrierTasks.empty()) {
        audioBarrierTasks.push_back(gameUpdateTask);
    }

    TaskId audioTask =
        m_scheduler.addTask({.name = "audio.global",
                             .phase = TaskPhase::Audio,
                             .work = [this]() { AudioManager::getInstance().update(m_deltaTime); },
                             .dependsOn = audioBarrierTasks,
                             .mainThreadOnly = true});

    // ---------------------------------------------------------------
    // Task 3: PreRender — apply clear color from primary scene.
    //         Depends on audio.global AND all final per-scene visual tasks.
    // ---------------------------------------------------------------
    std::vector<TaskId> preRenderDeps = {audioTask};
    for (TaskId id : finalVisualTasks) {
        // Deduplicate: legacy update tasks are already in audioBarrierTasks so
        // audio.global transitively covers them, but explicit edges make the
        // graph shape obvious and safe when audio tasks are empty.
        if (std::ranges::find(preRenderDeps, id) == preRenderDeps.end()) {
            preRenderDeps.push_back(id);
        }
    }

    TaskId preRenderTask = m_scheduler.addTask(
        {.name = "scene.preRender",
         .phase = TaskPhase::PreRender,
         .work =
             [this]() {
                 if (m_activeScene && m_vulkanContext) {
                     // Apply scene background color to Vulkan context
                     const Color& bg = m_activeScene->getBackgroundColor();
                     m_vulkanContext->setClearColor(glm::vec4(bg.r, bg.g, bg.b, bg.a));
                 }

                 if (m_transitionManager && m_transitionManager->isActive()) {
                     m_transitionManager->update(m_deltaTime);
                 }
             },
         .dependsOn = preRenderDeps,
         .mainThreadOnly = true});

    TaskId renderDep = preRenderTask;

    // ---------------------------------------------------------------
    // Task 4: Render — draw frame.  When transitioning, render both
    //         scenes to offscreen targets and composite.  Otherwise,
    //         use the normal single/multi-viewport path.
    // ---------------------------------------------------------------
    m_scheduler.addTask({.name = "scene.render",
                         .phase = TaskPhase::Render,
                         .work =
                             [this]() {
                                 if (!m_vulkanContext) {
                                     return;
                                 }

                                 // Transition rendering path
                                 if (m_transitionManager && m_transitionManager->isActive()) {
                                     renderTransition();
                                     return;
                                 }

                                 // Check if we need multi-viewport rendering
                                 bool needsMultiViewport = false;
                                 for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
                                     auto it = m_scenes.find(sceneName);
                                     if (it != m_scenes.end()) {
                                         if (it->second->getViewportRect() !=
                                             ViewportRect::fullWindow()) {
                                             needsMultiViewport = true;
                                             break;
                                         }
                                     }
                                 }

                                 if (needsMultiViewport) {
                                     // Multi-pass per-scene rendering
                                     renderMultiViewport();
                                 } else {
                                     // Original single-pass rendering (backwards compatible)
                                     renderSingleViewport();
                                 }
                             },
                         .dependsOn = {renderDep},
                         .mainThreadOnly = true});
}

}  // namespace vde
