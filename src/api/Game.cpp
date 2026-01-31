/**
 * @file Game.cpp
 * @brief Implementation of Game class
 */

#include <vde/api/Game.h>
#include <vde/Window.h>
#include <vde/VulkanContext.h>

#include <GLFW/glfw3.h>
#include <chrono>
#include <stdexcept>

namespace vde {

// ============================================================================
// Game Implementation
// ============================================================================

Game::Game()
    : m_initialized(false)
    , m_running(false)
    , m_settings()
    , m_window(nullptr)
    , m_vulkanContext(nullptr)
    , m_activeScene(nullptr)
    , m_inputHandler(nullptr)
    , m_deltaTime(0.0f)
    , m_totalTime(0.0)
    , m_fps(0.0f)
    , m_frameCount(0)
    , m_lastFrameTime(0.0)
    , m_fpsAccumulator(0.0)
    , m_fpsFrameCount(0)
    , m_sceneSwitchPending(false)
{
}

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
    
    try {
        // Create window
        m_window = std::make_unique<Window>(
            settings.display.windowWidth,
            settings.display.windowHeight,
            settings.gameName.c_str()
        );
        
        if (settings.display.fullscreen) {
            m_window->setFullscreen(true);
        }
        
        // Create and initialize Vulkan context
        m_vulkanContext = std::make_unique<VulkanContext>();
        m_vulkanContext->initialize(m_window.get());
        
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
        
        return true;
        
    } catch (const std::exception& e) {
        // Clean up on failure
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
    
    // Deactivate current scene
    if (m_activeScene) {
        m_activeScene->onExit();
        m_activeScene = nullptr;
    }
    
    // Clear all scenes
    m_scenes.clear();
    m_sceneStack.clear();
    
    // Cleanup Vulkan
    if (m_vulkanContext) {
        m_vulkanContext->cleanup();
        m_vulkanContext.reset();
    }
    
    // Destroy window
    m_window.reset();
    
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
        m_activeScene->onEnter();
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
        
        // Call update hook
        onUpdate(m_deltaTime);
        
        // Update active scene
        if (m_activeScene) {
            // Apply scene camera to Vulkan context
            if (m_activeScene->getCamera() && m_vulkanContext) {
                m_activeScene->getCamera()->applyTo(*m_vulkanContext);
            }
            
            m_activeScene->update(m_deltaTime);
        }
        
        // Render
        if (m_vulkanContext) {
            // Set render callback to render the active scene
            m_vulkanContext->setRenderCallback([this](VkCommandBuffer cmd) {
                (void)cmd; // Unused in Phase 1
                if (m_activeScene) {
                    m_activeScene->render();
                }
                onRender();
            });
            
            m_vulkanContext->drawFrame();
        }
        
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
    m_sceneStack.erase(
        std::remove(m_sceneStack.begin(), m_sceneStack.end(), name),
        m_sceneStack.end()
    );
    
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
    // Defer scene switch to avoid issues during update/render
    m_pendingScene = name;
    m_sceneSwitchPending = true;
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
    if (!m_window) return;
    
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
    // Input is handled via GLFW callbacks set up in setupInputCallbacks
    // The callbacks dispatch to the input handler
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
    
    // Exit current scene
    if (m_activeScene) {
        m_activeScene->onExit();
    }
    
    // Clear scene stack (setActiveScene resets the stack)
    m_sceneStack.clear();
    
    // Enter new scene
    m_activeScene = it->second.get();
    m_activeScene->onEnter();
}

void Game::setupInputCallbacks() {
    if (!m_window) return;
    
    GLFWwindow* handle = m_window->getHandle();
    
    // Store 'this' pointer for callbacks
    glfwSetWindowUserPointer(handle, this);
    
    // Key callback
    glfwSetKeyCallback(handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)scancode; // Unused
        
        Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
        if (!game) return;
        
        // Determine which input handler to use
        InputHandler* handler = nullptr;
        if (game->m_activeScene && game->m_activeScene->getInputHandler()) {
            handler = game->m_activeScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }
        
        if (handler) {
            (void)mods; // Modifier keys available but not passed to simple interface
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
        if (!game) return;
        
        InputHandler* handler = nullptr;
        if (game->m_activeScene && game->m_activeScene->getInputHandler()) {
            handler = game->m_activeScene->getInputHandler();
        } else if (game->m_inputHandler) {
            handler = game->m_inputHandler;
        }
        
        if (handler) {
            (void)mods; // Modifier keys available but not passed to simple interface
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
        if (!game) return;
        
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
        if (!game) return;
        
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
}

} // namespace vde
