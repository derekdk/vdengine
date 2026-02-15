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

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <glslang/Public/ShaderLang.h>

namespace vde {

// Forward declaration of sprite descriptor cache cleanup function from Entity.cpp
extern void clearSpriteDescriptorCache();

// ============================================================================
// Game Implementation
// ============================================================================

Game::Game()
    : m_initialized(false), m_running(false), m_settings(), m_window(nullptr),
      m_vulkanContext(nullptr), m_activeScene(nullptr), m_inputHandler(nullptr), m_deltaTime(0.0f),
      m_totalTime(0.0), m_fps(0.0f), m_frameCount(0), m_lastFrameTime(0.0), m_fpsAccumulator(0.0),
      m_fpsFrameCount(0), m_sceneSwitchPending(false) {}

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
        m_window = std::make_unique<Window>(
            settings.display.windowWidth, settings.display.windowHeight, settings.gameName.c_str());

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

        // Initialize audio system (Phase 6)
        AudioManager::getInstance().initialize(settings.audio);

        // Setup input callbacks
        setupInputCallbacks();

        // Set up resize callback
        m_window->setResizeCallback([this](uint32_t width, uint32_t height) {
            if (m_vulkanContext) {
                m_vulkanContext->recreateSwapchain(width, height);
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
                m_inputScriptFile = envScript;
            }
        }
        if (!m_inputScriptFile.empty()) {
            loadInputScript();
        }

        return true;

    } catch (const std::exception& e) {
        // Clean up on failure
        std::cerr << "Game initialization failed: " << e.what() << std::endl;
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
    onShutdown();

    // Deactivate all scenes in the active group
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) {
            it->second->onExit();
        }
    }
    m_activeScene = nullptr;
    m_activeSceneGroup = SceneGroup{};

    // Clear all scenes
    m_scenes.clear();
    m_sceneStack.clear();

    // Clear sprite descriptor cache (static in Entity.cpp)
    clearSpriteDescriptorCache();

    // Shutdown audio system
    AudioManager::getInstance().shutdown();

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
        m_activeScene->onEnter();
    }

    // Build the initial scheduler task graph
    rebuildSchedulerGraph();

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

        // Process input script commands
        processInputScript();

        // Execute the scheduler task graph
        // (covers: onUpdate, scene update, audio, pre-render, render)
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

void Game::setInputScriptFile(const std::string& scriptPath) {
    m_inputScriptFile = scriptPath;
}

const std::string& Game::getInputScriptFile() const {
    return m_inputScriptFile;
}

void Game::loadInputScript() {
    m_inputScriptState = std::make_unique<InputScriptState>();
    std::string errorMsg;

    if (!parseInputScript(m_inputScriptFile, m_inputScriptState->commands,
                          m_inputScriptState->labels, errorMsg)) {
        std::cerr << "[VDE:InputScript] " << errorMsg << std::endl;
        m_inputScriptState.reset();
        return;
    }

    m_inputScriptState->scriptPath = m_inputScriptFile;
    std::cout << "[VDE:InputScript] Loaded " << m_inputScriptState->commands.size()
              << " commands from " << m_inputScriptFile << std::endl;
}

void Game::processInputScript() {
    if (!m_inputScriptState || m_inputScriptState->finished) {
        return;
    }

    auto& state = *m_inputScriptState;
    state.frameNumber++;

    // Handle pending mouse button release from previous frame (for click commands)
    if (state.pendingMouseRelease) {
        state.pendingMouseRelease = false;
        // Resolve handler: focused scene handler → global handler
        InputHandler* handler = nullptr;
        Scene* focused = getFocusedScene();
        if (focused) {
            handler = focused->getInputHandler();
        }
        if (!handler) {
            handler = m_inputHandler;
        }
        if (handler) {
            handler->onMouseButtonRelease(state.pendingMouseButton, state.pendingMouseX,
                                          state.pendingMouseY);
        }
    }

    // Process commands until we hit a blocking command or run out
    while (state.currentCommand < state.commands.size()) {
        const auto& cmd = state.commands[state.currentCommand];

        // Resolve handler: focused scene handler → global handler
        InputHandler* handler = nullptr;
        Scene* focused = getFocusedScene();
        if (focused) {
            handler = focused->getInputHandler();
        }
        if (!handler) {
            handler = m_inputHandler;
        }

        switch (cmd.type) {
        case InputCommandType::WaitStartup:
            if (!state.startupComplete) {
                state.startupComplete = true;
                state.currentCommand++;
                return;  // Wait until next frame
            }
            state.currentCommand++;
            break;

        case InputCommandType::WaitMs:
            state.waitAccumulator += static_cast<double>(m_deltaTime) * 1000.0;
            if (state.waitAccumulator >= cmd.waitMs) {
                state.waitAccumulator = 0.0;
                state.currentCommand++;
            } else {
                return;  // Still waiting
            }
            break;

        case InputCommandType::Press:
            if (handler) {
                handler->onKeyPress(cmd.keyCode);
                handler->onKeyRelease(cmd.keyCode);
            }
            state.currentCommand++;
            break;

        case InputCommandType::KeyDown:
            if (handler) {
                handler->onKeyPress(cmd.keyCode);
            }
            state.currentCommand++;
            break;

        case InputCommandType::KeyUp:
            if (handler) {
                handler->onKeyRelease(cmd.keyCode);
            }
            state.currentCommand++;
            break;

        case InputCommandType::Click: {
            if (handler) {
                handler->onMouseMove(cmd.mouseX, cmd.mouseY);
                handler->onMouseButtonPress(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
                // Schedule release for next frame
                state.pendingMouseRelease = true;
                state.pendingMouseButton = MOUSE_BUTTON_LEFT;
                state.pendingMouseX = cmd.mouseX;
                state.pendingMouseY = cmd.mouseY;
            }
            state.currentCommand++;
            return;  // Wait for next frame for release
        }

        case InputCommandType::ClickRight: {
            if (handler) {
                handler->onMouseMove(cmd.mouseX, cmd.mouseY);
                handler->onMouseButtonPress(MOUSE_BUTTON_RIGHT, cmd.mouseX, cmd.mouseY);
                state.pendingMouseRelease = true;
                state.pendingMouseButton = MOUSE_BUTTON_RIGHT;
                state.pendingMouseX = cmd.mouseX;
                state.pendingMouseY = cmd.mouseY;
            }
            state.currentCommand++;
            return;  // Wait for next frame for release
        }

        case InputCommandType::MouseDown:
            if (handler) {
                handler->onMouseMove(cmd.mouseX, cmd.mouseY);
                handler->onMouseButtonPress(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
            }
            state.currentCommand++;
            break;

        case InputCommandType::MouseUp:
            if (handler) {
                handler->onMouseButtonRelease(MOUSE_BUTTON_LEFT, cmd.mouseX, cmd.mouseY);
            }
            state.currentCommand++;
            break;

        case InputCommandType::MouseMove:
            if (handler) {
                handler->onMouseMove(cmd.mouseX, cmd.mouseY);
            }
            state.currentCommand++;
            break;

        case InputCommandType::Scroll:
            if (handler) {
                handler->onMouseMove(cmd.mouseX, cmd.mouseY);
                handler->onMouseScroll(0.0, cmd.scrollDelta);
            }
            state.currentCommand++;
            break;

        case InputCommandType::Screenshot:
            // Screenshot support deferred (WI-5)
            std::cout << "[VDE:InputScript] screenshot command not yet implemented" << std::endl;
            state.currentCommand++;
            break;

        case InputCommandType::Label:
            // Labels are no-ops at execution time
            state.currentCommand++;
            break;

        case InputCommandType::Loop: {
            auto labelIt = state.labels.find(cmd.argument);
            if (labelIt == state.labels.end()) {
                std::cerr << "[VDE:InputScript] Error at line " << cmd.lineNumber
                          << ": undefined label '" << cmd.argument << "'" << std::endl;
                state.finished = true;
                return;
            }

            auto& labelState = labelIt->second;

            if (cmd.loopCount == 0) {
                // Infinite loop
                state.currentCommand = labelState.commandIndex + 1;
            } else {
                if (labelState.remainingIterations < 0) {
                    // First encounter
                    labelState.remainingIterations = cmd.loopCount - 1;
                } else {
                    labelState.remainingIterations--;
                }

                if (labelState.remainingIterations > 0) {
                    state.currentCommand = labelState.commandIndex + 1;
                } else {
                    // Reset for potential re-entry (e.g., outer loop re-runs inner)
                    labelState.remainingIterations = -1;
                    state.currentCommand++;
                }
            }
            break;
        }

        case InputCommandType::Exit:
            std::cout << "[VDE:InputScript] exit" << std::endl;
            state.finished = true;
            quit();
            return;
        }
    }

    // All commands consumed — script is done, app continues
    state.finished = true;
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
}

void Game::removeScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it == m_scenes.end()) {
        return;
    }

    // If this is the active scene, deactivate it
    if (m_activeScene == it->second.get()) {
        m_activeScene->onExit();
        m_activeScene = nullptr;
    }

    // Remove from stack if present
    m_sceneStack.erase(std::remove(m_sceneStack.begin(), m_sceneStack.end(), name),
                       m_sceneStack.end());

    m_scenes.erase(it);
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

    // Build sets for old and new groups to diff them
    auto isInList = [](const std::vector<std::string>& list, const std::string& name) {
        for (const auto& n : list) {
            if (n == name)
                return true;
        }
        return false;
    };

    // Exit scenes that are in the OLD group but NOT in the NEW group
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        if (!isInList(group.sceneNames, sceneName)) {
            auto it = m_scenes.find(sceneName);
            if (it != m_scenes.end()) {
                it->second->onExit();
            }
        }
    }

    // Clear scene stack (group switch resets the stack)
    m_sceneStack.clear();

    // Remember old group for enter logic
    SceneGroup oldGroup = m_activeSceneGroup;

    // Set new group
    m_activeSceneGroup = group;

    // Set primary scene (first in the group)
    if (!group.sceneNames.empty()) {
        auto it = m_scenes.find(group.sceneNames[0]);
        if (it != m_scenes.end()) {
            m_activeScene = it->second.get();
        }
    } else {
        m_activeScene = nullptr;
    }

    // Enter scenes that are in the NEW group but NOT in the OLD group
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        if (!isInList(oldGroup.sceneNames, sceneName)) {
            auto it = m_scenes.find(sceneName);
            if (it != m_scenes.end()) {
                it->second->onEnter();
            }
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
    m_activeScene->onEnter();
}

void Game::popScene() {
    if (m_sceneStack.empty()) {
        return;
    }

    // Exit current scene
    if (m_activeScene) {
        m_activeScene->onExit();
    }

    // Resume previous scene
    std::string prevName = m_sceneStack.back();
    m_sceneStack.pop_back();

    auto it = m_scenes.find(prevName);
    if (it != m_scenes.end()) {
        m_activeScene = it->second.get();
        m_activeScene->onResume();
    } else {
        m_activeScene = nullptr;
    }
}

void Game::applyDisplaySettings(const DisplaySettings& settings) {
    if (!m_window)
        return;

    m_settings.display = settings;

    m_window->setResolution(settings.windowWidth, settings.windowHeight);
    m_window->setFullscreen(settings.fullscreen);
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
        if (!isGamepad)
            continue;

        GLFWgamepadstate state;
        if (glfwGetGamepadState(jid, &state) != GLFW_TRUE)
            continue;

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

    // Exit scenes in the current group that are NOT the pending scene
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        if (sceneName == m_pendingScene)
            continue;  // Will stay active — don't exit
        auto sceneIt = m_scenes.find(sceneName);
        if (sceneIt != m_scenes.end()) {
            sceneIt->second->onExit();
        }
    }

    // Clear scene stack (setActiveScene resets the stack)
    m_sceneStack.clear();

    // Check if the pending scene was already in the old group
    bool wasAlreadyActive = false;
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        if (sceneName == m_pendingScene) {
            wasAlreadyActive = true;
            break;
        }
    }

    // Create a single-scene group
    m_activeSceneGroup = SceneGroup::create(m_pendingScene, {m_pendingScene});

    // Enter new scene only if it wasn't already in the group
    m_activeScene = it->second.get();
    if (!wasAlreadyActive) {
        m_activeScene->onEnter();
    }

    // Rebuild the scheduler graph for the new scene
    rebuildSchedulerGraph();
}

void Game::setupInputCallbacks() {
    if (!m_window)
        return;

    GLFWwindow* handle = m_window->getHandle();

    // Store 'this' pointer for callbacks
    glfwSetWindowUserPointer(handle, this);

    // Key callback
    glfwSetKeyCallback(handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)scancode;  // Unused

        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game)
            return;

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

    // Mouse button callback
    glfwSetMouseButtonCallback(handle, [](GLFWwindow* window, int button, int action, int mods) {
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game)
            return;

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
        if (!game)
            return;

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
        if (!game)
            return;

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

    // Joystick/gamepad connection callback
    // Note: GLFW joystick callbacks are global (not per-window), so we use a static
    // pointer to route events. This is safe because VDE only supports one Game instance.
    static Game* s_gameForJoystick = nullptr;
    s_gameForJoystick = this;

    glfwSetJoystickCallback([](int jid, int event) {
        Game* game = s_gameForJoystick;
        if (!game)
            return;

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
                for (int btn = 0; btn <= GAMEPAD_BUTTON_LAST; ++btn)
                    handler->_setGamepadButton(jid, btn, false);
                for (int axis = 0; axis <= GAMEPAD_AXIS_LAST; ++axis)
                    handler->_setGamepadAxis(jid, axis, 0.0f);
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
    std::cout << "Creating mesh rendering pipeline..." << std::endl;

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << std::endl;
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();
    std::cout << "Got device" << std::endl;

    // Compile shaders
    ShaderCompiler compiler;
    std::vector<uint32_t> vertSpv, fragSpv;

    std::cout << "Compiling vertex shader..." << std::endl;
    auto vertResult = compiler.compileFile("shaders/mesh.vert", ShaderStage::Vertex);
    if (!vertResult.success) {
        std::cerr << "Vertex shader compilation failed: " << vertResult.errorLog << std::endl;
        throw std::runtime_error("Failed to compile mesh vertex shader: " + vertResult.errorLog);
    }
    vertSpv = vertResult.spirv;
    std::cout << "Vertex shader compiled successfully (" << vertSpv.size() << " words)"
              << std::endl;

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
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

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
    meshTexturePoolSize.descriptorCount = 256;

    VkDescriptorPoolCreateInfo meshTexturePoolInfo{};
    meshTexturePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    meshTexturePoolInfo.poolSizeCount = 1;
    meshTexturePoolInfo.pPoolSizes = &meshTexturePoolSize;
    meshTexturePoolInfo.maxSets = 256;

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
    std::cout << "Creating sprite rendering pipeline..." << std::endl;

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << std::endl;
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

    std::cout << "Compiling sprite vertex shader..." << std::endl;
    auto vertResult = compiler.compileFile("shaders/simple_sprite.vert", ShaderStage::Vertex);
    if (!vertResult.success) {
        std::cerr << "Sprite vertex shader compilation failed: " << vertResult.errorLog
                  << std::endl;
        throw std::runtime_error("Failed to compile sprite vertex shader: " + vertResult.errorLog);
    }
    std::cout << "Sprite vertex shader compiled successfully" << std::endl;

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
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

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
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 100;  // Support up to 100 sprite descriptor sets
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 100;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 100;

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
        std::cerr << "Warning: Failed to create default white texture" << std::endl;
        m_defaultWhiteTexture.reset();
    }

    std::cout << "Sprite rendering pipeline created successfully" << std::endl;
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
    std::cout << "Creating lighting resources..." << std::endl;

    if (!m_vulkanContext) {
        std::cout << "No Vulkan context!" << std::endl;
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

    for (uint32_t i = 0; i < framesInFlight; i++) {
        BufferUtils::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  m_lightingUBOBuffers[i], m_lightingUBOMemory[i]);

        // Persistently map the buffer
        vkMapMemory(device, m_lightingUBOMemory[i], 0, bufferSize, 0, &m_lightingUBOMapped[i]);
    }

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

    std::cout << "Lighting resources created successfully" << std::endl;
}

void Game::destroyLightingResources() {
    if (!m_vulkanContext) {
        return;
    }

    VkDevice device = m_vulkanContext->getDevice();

    // Unmap and destroy UBO buffers
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
            const Light& light = lights[i];
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

    memcpy(m_lightingUBOMapped[currentFrame], &ubo, sizeof(LightingUBO));
}

void Game::setFocusedScene(const std::string& sceneName) {
    m_focusedSceneName = sceneName;
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
        const auto& sceneName = m_activeSceneGroup.sceneNames[i];
        auto it = m_scenes.find(sceneName);
        if (it == m_scenes.end()) {
            continue;
        }

        Scene* scene = it->second.get();

        VulkanContext::SceneRenderInfo info{};
        info.clearPass = (i == 0);

        // Get the scene's camera matrices
        if (scene->getCamera()) {
            // Apply camera to get internal state updated (positions etc.)
            scene->getCamera()->applyTo(*m_vulkanContext);
            info.viewMatrix = m_vulkanContext->getCamera().getViewMatrix();
            info.projMatrix = m_vulkanContext->getCamera().getProjectionMatrix();
        } else {
            info.viewMatrix = glm::mat4(1.0f);
            info.projMatrix = glm::mat4(1.0f);
        }

        // Compute viewport and scissor from the scene's ViewportRect
        const ViewportRect& vpRect = scene->getViewportRect();
        info.viewport = vpRect.toVkViewport(extent.width, extent.height);
        info.scissor = vpRect.toVkScissor(extent.width, extent.height);

        // Update camera aspect ratio based on viewport dimensions
        if (scene->getCamera()) {
            float vpAspect = vpRect.getAspectRatio(extent.width, extent.height);
            scene->getCamera()->setAspectRatio(vpAspect);
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

    // Active group scenes
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) {
            updateScenes.push_back({it->second.get(), sceneName, it->second->getUpdatePriority()});
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
            updateScenes.push_back({scenePtr.get(), name, scenePtr->getUpdatePriority()});
        }
    }

    // Sort by update priority (lower value = earlier execution)
    std::sort(updateScenes.begin(), updateScenes.end(),
              [](const SceneEntry& a, const SceneEntry& b) { return a.priority < b.priority; });

    // ---------------------------------------------------------------
    // Task 1: GameLogic — onUpdate hook + all scene updates
    // ---------------------------------------------------------------
    // Chain: onUpdate -> update(scene1) | gameLogic(scene1) -> ...
    TaskId prevTask = m_scheduler.addTask(
        {"game.update", TaskPhase::GameLogic, [this]() { onUpdate(m_deltaTime); }});

    // Track per-scene audio tasks so audio.global can depend on all of them
    std::vector<TaskId> audioTasks;

    for (size_t i = 0; i < updateScenes.size(); ++i) {
        Scene* scene = updateScenes[i].scene;
        const std::string& sceneName = updateScenes[i].name;

        if (scene->usesPhaseCallbacks()) {
            // --- Phase callbacks mode: three separate tasks ---

            // GameLogic task
            TaskId gameLogicTask =
                m_scheduler.addTask({"scene.gameLogic." + sceneName,
                                     TaskPhase::GameLogic,
                                     [this, scene]() { scene->updateGameLogic(m_deltaTime); },
                                     {prevTask}});

            // Audio task (depends on gameLogic so queued events are available)
            TaskId audioTask =
                m_scheduler.addTask({"scene.audio." + sceneName,
                                     TaskPhase::Audio,
                                     [this, scene]() { scene->updateAudio(m_deltaTime); },
                                     {gameLogicTask}});
            audioTasks.push_back(audioTask);

            // Visuals task (depends on gameLogic; can run concurrently with audio in future)
            TaskId visualsTask =
                m_scheduler.addTask({"scene.visuals." + sceneName,
                                     TaskPhase::GameLogic,
                                     [this, scene]() { scene->updateVisuals(m_deltaTime); },
                                     {gameLogicTask}});

            // The next scene's tasks depend on the last task of this scene
            // (visuals, since it's the broadest output)
            prevTask = visualsTask;
        } else {
            // --- Legacy mode: single update task ---
            prevTask = m_scheduler.addTask({"scene.update." + sceneName,
                                            TaskPhase::GameLogic,
                                            [this, scene]() { scene->update(m_deltaTime); },
                                            {prevTask}});
        }
    }

    TaskId lastUpdateTask = prevTask;

    // ---------------------------------------------------------------
    // Task 1b: Physics — step physics for scenes that have it.
    //          Physics depends on all game-logic/update tasks.
    // ---------------------------------------------------------------
    TaskId lastPhysicsTask = lastUpdateTask;
    std::vector<TaskId> postPhysicsTasks;

    for (size_t i = 0; i < updateScenes.size(); ++i) {
        Scene* scene = updateScenes[i].scene;
        const std::string& sceneName = updateScenes[i].name;

        if (scene->hasPhysics()) {
            TaskId physicsTask = m_scheduler.addTask({
                "scene.physics." + sceneName,
                TaskPhase::Physics,
                [this, scene]() { scene->getPhysicsScene()->step(m_deltaTime); },
                {lastUpdateTask},
                false  // Physics step can run on worker threads
            });

            // PostPhysics: sync physics entity transforms with interpolation
            TaskId postPhysicsTask = m_scheduler.addTask(
                {"scene.postPhysics." + sceneName,
                 TaskPhase::PostPhysics,
                 [scene]() {
                     if (!scene->hasPhysics())
                         return;
                     float alpha = scene->getPhysicsScene()->getInterpolationAlpha();
                     for (auto& entityRef : scene->getEntities()) {
                         auto* pe = dynamic_cast<PhysicsEntity*>(entityRef.get());
                         if (pe && pe->getAutoSync()) {
                             pe->syncFromPhysics(alpha);
                         }
                     }
                 },
                 {physicsTask}});

            postPhysicsTasks.push_back(postPhysicsTask);
            lastPhysicsTask = postPhysicsTask;
        }
    }

    // ---------------------------------------------------------------
    // Task 2: Audio — global audio system update
    //         Depends on all per-scene audio tasks AND the last
    //         physics/update task so it always runs after all scene work.
    // ---------------------------------------------------------------
    std::vector<TaskId> audioDeps = {lastPhysicsTask};
    for (TaskId id : audioTasks) {
        audioDeps.push_back(id);
    }
    for (TaskId id : postPhysicsTasks) {
        audioDeps.push_back(id);
    }

    TaskId audioTask = m_scheduler.addTask(
        {"audio.global", TaskPhase::Audio,
         [this]() { AudioManager::getInstance().update(m_deltaTime); }, audioDeps});

    // ---------------------------------------------------------------
    // Task 3: PreRender — apply clear color from primary scene.
    //         Camera apply moves into the render loop (per-scene).
    // ---------------------------------------------------------------
    TaskId preRenderTask = m_scheduler.addTask(
        {"scene.preRender",
         TaskPhase::PreRender,
         [this]() {
             if (m_activeScene && m_vulkanContext) {
                 // Apply scene background color to Vulkan context
                 const Color& bg = m_activeScene->getBackgroundColor();
                 m_vulkanContext->setClearColor(glm::vec4(bg.r, bg.g, bg.b, bg.a));
             }
         },
         {audioTask}});

    // ---------------------------------------------------------------
    // Task 4: Render — draw frame.  If any scene has a non-fullWindow
    //         viewport, use multi-pass rendering.  Otherwise, use the
    //         original single-pass path for backwards compatibility.
    // ---------------------------------------------------------------
    m_scheduler.addTask({"scene.render",
                         TaskPhase::Render,
                         [this]() {
                             if (!m_vulkanContext) {
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
                         {preRenderTask}});
}

}  // namespace vde
