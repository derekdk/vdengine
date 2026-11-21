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

namespace {

/// Upper bound on scene-flow requests applied in one flush (guards against scene
/// callbacks that keep requesting changes from onEnter()/onExit()).
constexpr size_t kMaxSceneFlowRequestsPerFlush = 256;

bool groupContains(const SceneGroup& group, const std::string& name) {
    return std::ranges::find(group.sceneNames, name) != group.sceneNames.end();
}

void eraseSceneFromGroup(SceneGroup& group, const std::string& name) {
    std::erase(group.sceneNames, name);
    std::erase_if(group.entries,
                  [&name](const SceneGroupEntry& entry) { return entry.sceneName == name; });
}

/**
 * Build one SceneRenderInfo per layer: viewport/scissor from the layer, view/projection
 * from the layer scene's own camera (aspect matched to its viewport).  Scenes without a
 * camera reuse the primary (bottom) layer's matrices.
 */
template <typename Layers>
std::vector<VulkanContext::SceneRenderInfo> buildSceneRenderInfos(VulkanContext& context,
                                                                  const Layers& layers) {
    const VkExtent2D extent = context.getSwapChainExtent();
    std::vector<VulkanContext::SceneRenderInfo> infos;
    infos.reserve(layers.size());

    glm::mat4 fallbackView = context.getCamera().getViewMatrix();
    glm::mat4 fallbackProj = context.getCamera().getProjectionMatrix();

    for (const auto& layer : layers) {
        Scene* scene = layer.scene;
        VulkanContext::SceneRenderInfo info{};
        info.viewport = layer.viewport.toVkViewport(extent.width, extent.height);
        info.scissor = layer.viewport.toVkScissor(extent.width, extent.height);

        if (GameCamera* camera = scene->getCamera()) {
            camera->setAspectRatio(layer.viewport.getAspectRatio(extent.width, extent.height));
            camera->applyTo(context);
            info.viewMatrix = context.getCamera().getViewMatrix();
            info.projMatrix = context.getCamera().getProjectionMatrix();
        } else {
            info.viewMatrix = fallbackView;
            info.projMatrix = fallbackProj;
        }
        if (infos.empty()) {
            fallbackView = info.viewMatrix;
            fallbackProj = info.projMatrix;
        }

        info.renderCallback = [scene](VkCommandBuffer /*cmd*/) { scene->render(); };
        infos.push_back(std::move(info));
    }
    return infos;
}

template <typename Layers>
glm::vec4 bottomLayerClearColor(const Layers& layers, const glm::vec4& fallback) {
    if (layers.empty()) {
        return fallback;
    }
    const Color& bg = layers.front().scene->getBackgroundColor();
    return {bg.r, bg.g, bg.b, bg.a};
}

}  // namespace

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

    // Deactivate all currently active scenes (including paused stack scenes).
    // Scene-flow requests made from onExit() are dropped: nothing may re-enter during shutdown.
    m_processingSceneFlow = true;
    std::vector<std::string> activeScenes(m_activeSceneNames.begin(), m_activeSceneNames.end());
    for (const auto& sceneName : activeScenes) {
        deactivateScene(sceneName);
    }
    m_sceneFlowRequests.clear();
    m_processingSceneFlow = false;
    clearTransitionState();
    m_activeScene = nullptr;
    m_activeSceneGroup = SceneGroup{};
    m_activeSceneNames.clear();  // Defensive: ensure clean state on shutdown
    m_pausedSceneNames.clear();
    m_groupAssignedViewports.clear();

    // Clear all scenes
    m_scenes.clear();
    m_sceneStack.clear();
    m_retiredScenes.clear();

    // Release persistent GPU resources while the Vulkan device is still alive
    m_resourceManager.clear();

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

    // Scene-flow requests made before run() were applied immediately (requests from
    // onStart() apply at the first frame boundary).  Sync focus diagnostics with input routing.
    refreshFocusDiagnostics();

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
    if (m_processingSceneFlow) {
        SceneFlowRequest request;
        request.kind = SceneFlowRequest::Kind::Remove;
        request.sceneName = name;
        requestSceneFlow(std::move(request));
        return;
    }

    if (!sceneExists(name)) {
        return;
    }

    // Keep direct removal immediate, but serialize requests from its lifecycle callbacks.
    m_processingSceneFlow = true;
    try {
        applyRemoveScene(name);
    } catch (...) {
        m_processingSceneFlow = false;
        throw;
    }
    m_processingSceneFlow = false;

    if (!shouldDeferSceneFlow()) {
        processPendingSceneChange();
    } else if (!m_scheduler.isExecuting()) {
        rebuildSchedulerGraph();
    }
}

void Game::applyRemoveScene(const std::string& name) {
    if (!sceneExists(name)) {
        return;
    }

    // A transition that can no longer complete as requested is cancelled with the normal undo
    // (exits scenes it entered, re-pauses scenes it resumed).
    if (m_transitionActive) {
        const bool regionSceneRemoved =
            name == m_transitionSourceScene || name == m_transitionDestScene;
        const bool destGroupEmptied = !m_viewportTransition && m_transitionDestGroup.size() == 1 &&
                                      groupContains(m_transitionDestGroup, name);
        if (regionSceneRemoved || destGroupEmptied) {
            applyCancelTransition();
        }
    }
    eraseSceneFromGroup(m_transitionDestGroup, name);
    std::erase(m_transitionEnteredScenes, name);
    std::erase(m_transitionResumedScenes, name);

    // Exit the scene if it is active or paused
    m_pausedSceneNames.erase(name);
    m_groupAssignedViewports.erase(name);
    deactivateScene(name);

    // Remove from the active group and the scene stack
    eraseSceneFromGroup(m_activeSceneGroup, name);
    for (auto& stacked : m_sceneStack) {
        eraseSceneFromGroup(stacked, name);
    }
    std::erase_if(m_sceneStack, [](const SceneGroup& group) { return group.empty(); });
    m_activeScene =
        m_activeSceneGroup.empty() ? nullptr : getScene(m_activeSceneGroup.sceneNames.front());
    if (m_focusedSceneName == name) {
        m_focusedSceneName.clear();
    }

    auto removed = m_scenes.extract(name);
    m_scenesRemoved++;
    refreshFocusDiagnostics();

    // Scheduler tasks hold raw Scene pointers: when called mid-frame, keep the scene alive
    // until the frame boundary, where the task graph is rebuilt without it.
    if (m_scheduler.isExecuting()) {
        m_retiredScenes.push_back(std::move(removed.mapped()));
    }
    m_schedulerRebuildPending = true;
}

Scene* Game::getScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

// ============================================================================
// Scene flow
//
// All scene-flow changes (set/push/pop/transition start/cancel/complete) go through
// one queue.  While the game loop runs (or a frame is executing) requests are applied
// in call order at the next frame boundary, so the task graph and the set of drawn
// scenes never change mid-frame.  Otherwise they apply immediately.
// ============================================================================

void Game::setActiveScene(const std::string& name) {
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::SetGroup;
    request.group = SceneGroup::create(name, {name});
    requestSceneFlow(std::move(request));
}

void Game::setActiveSceneGroup(const SceneGroup& group) {
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::SetGroup;
    request.group = group;
    requestSceneFlow(std::move(request));
}

void Game::pushScene(const std::string& name) {
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::Push;
    request.sceneName = name;
    requestSceneFlow(std::move(request));
}

void Game::popScene() {
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::Pop;
    requestSceneFlow(std::move(request));
}

std::vector<std::string> Game::getVisibleSceneNames() const {
    std::vector<std::string> names;
    for (const auto& layer : collectFrameLayers()) {
        names.push_back(layer.scene->getName());
    }
    return names;
}

bool Game::sceneExists(const std::string& name) const {
    return m_scenes.find(name) != m_scenes.end();
}

void Game::applyPendingSceneChanges() {
    if (m_scheduler.isExecuting()) {
        return;
    }
    processPendingSceneChange();
}

bool Game::shouldDeferSceneFlow() const {
    return m_running || m_scheduler.isExecuting();
}

void Game::requestSceneFlow(SceneFlowRequest request) {
    m_sceneFlowRequests.push_back(std::move(request));
    if (!shouldDeferSceneFlow()) {
        processPendingSceneChange();
    }
}

void Game::processPendingSceneChange() {
    if (m_processingSceneFlow) {
        return;  // Re-entrant request from a scene callback: the outer flush applies it
    }

    m_processingSceneFlow = true;
    try {
        size_t applied = 0;
        while (!m_sceneFlowRequests.empty()) {
            if (applied++ >= kMaxSceneFlowRequestsPerFlush) {
                std::cerr << "[VDE] Scene flow: more than " << kMaxSceneFlowRequestsPerFlush
                          << " requests in one frame (scene callbacks requesting changes in a "
                             "loop?); dropping "
                          << m_sceneFlowRequests.size() << " request(s)\n";
                m_sceneFlowRequests.clear();
                break;
            }
            SceneFlowRequest request = std::move(m_sceneFlowRequests.front());
            m_sceneFlowRequests.erase(m_sceneFlowRequests.begin());
            applySceneFlowRequest(request);
        }
    } catch (...) {
        m_processingSceneFlow = false;
        throw;
    }
    m_processingSceneFlow = false;

    if (m_schedulerRebuildPending) {
        rebuildSchedulerGraph();
    }
    // Safe now: the rebuilt graph no longer references scenes removed mid-frame.
    m_retiredScenes.clear();
}

void Game::applySceneFlowRequest(SceneFlowRequest& request) {
    using Kind = SceneFlowRequest::Kind;

    // A transition whose effect already finished counts as complete before any later request.
    if (m_transitionCompletionPending && request.kind != Kind::CompleteTransition) {
        applyCompleteTransition();
    }

    switch (request.kind) {
    case Kind::SetGroup: {
        for (const auto& sceneName : request.group.sceneNames) {
            if (!sceneExists(sceneName)) {
                std::cerr << "[VDE] setActiveScene/Group: unknown scene '" << sceneName
                          << "'; request ignored\n";
                return;
            }
        }
        if (m_transitionActive) {
            // Abandon without lifecycle undo: applySceneGroup() exits/keeps scenes itself.
            if (m_transitionManager) {
                m_transitionManager->cancel();
            }
            clearTransitionState();
        }
        applySceneGroup(request.group, /*keepStack=*/false);
        break;
    }
    case Kind::Push: {
        // Validate before touching a running transition: an ignored push must not cancel it.
        const bool enteredOnlyByTransition =
            std::ranges::find(m_transitionEnteredScenes, request.sceneName) !=
            m_transitionEnteredScenes.end();
        if (!sceneExists(request.sceneName) ||
            (m_activeSceneNames.contains(request.sceneName) && !enteredOnlyByTransition)) {
            applyPushScene(request.sceneName);  // Reports why the request is ignored
            break;
        }
        applyCancelTransition();
        applyPushScene(request.sceneName);
        break;
    }
    case Kind::Pop:
        if (m_sceneStack.empty()) {
            break;  // Nothing to pop; leave any running transition alone
        }
        applyCancelTransition();
        applyPopScene();
        break;
    case Kind::Remove:
        applyRemoveScene(request.sceneName);
        break;
    case Kind::StartTransition:
        applyStartTransition(request);
        break;
    case Kind::CancelTransition:
        applyCancelTransition();
        break;
    case Kind::CompleteTransition:
        if (request.transitionId == m_transitionGeneration) {
            applyCompleteTransition();
        }
        break;
    }
}

void Game::applySceneGroup(const SceneGroup& group, bool keepStack) {
    std::unordered_set<std::string> keep(group.sceneNames.begin(), group.sceneNames.end());
    if (keepStack) {
        for (const auto& stacked : m_sceneStack) {
            keep.insert(stacked.sceneNames.begin(), stacked.sceneNames.end());
        }
    }

    // Exit entered scenes that are neither in the new group nor (if kept) the stack.
    // Order: active group top-down, then the stack top-down, then anything else by name.
    std::vector<std::string> exitOrder(m_activeSceneGroup.sceneNames.rbegin(),
                                       m_activeSceneGroup.sceneNames.rend());
    for (auto stackIt = m_sceneStack.rbegin(); stackIt != m_sceneStack.rend(); ++stackIt) {
        exitOrder.insert(exitOrder.end(), stackIt->sceneNames.rbegin(), stackIt->sceneNames.rend());
    }
    std::vector<std::string> remaining;
    for (const auto& sceneName : m_activeSceneNames) {
        if (std::ranges::find(exitOrder, sceneName) == exitOrder.end()) {
            remaining.push_back(sceneName);
        }
    }
    std::ranges::sort(remaining);
    exitOrder.insert(exitOrder.end(), remaining.begin(), remaining.end());

    for (const auto& sceneName : exitOrder) {
        if (keep.contains(sceneName) || !m_activeSceneNames.contains(sceneName)) {
            continue;
        }
        m_pausedSceneNames.erase(sceneName);
        deactivateScene(sceneName);
    }

    if (!keepStack) {
        m_sceneStack.clear();
    }

    m_activeSceneGroup = group;
    m_activeScene = group.empty() ? nullptr : getScene(group.sceneNames.front());
    applyGroupViewports(group);

    for (const auto& sceneName : group.sceneNames) {
        if (m_pausedSceneNames.erase(sceneName) > 0) {
            Scene* scene = getScene(sceneName);
            scene->m_diagnostics.resumeCount++;
            scene->onResume();
        }
        activateScene(sceneName);
    }

    refreshFocusDiagnostics();
    m_schedulerRebuildPending = true;
}

void Game::applyPushScene(const std::string& name) {
    Scene* pushed = getScene(name);
    if (!pushed) {
        std::cerr << "[VDE] pushScene: unknown scene '" << name << "'; request ignored\n";
        return;
    }
    if (m_activeSceneNames.contains(name)) {
        std::cerr << "[VDE] pushScene: scene '" << name
                  << "' is already active or paused; request ignored\n";
        return;
    }

    // Pause the current group; it stays visible (frozen) beneath the pushed scene.
    if (!m_activeSceneGroup.empty()) {
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            Scene* scene = getScene(sceneName);
            if (scene && m_pausedSceneNames.insert(sceneName).second) {
                scene->m_diagnostics.pauseCount++;
                scene->onPause();
            }
        }
        m_sceneStack.push_back(m_activeSceneGroup);
    }

    m_activeSceneGroup = SceneGroup::create(name, {name});
    m_activeScene = pushed;
    applyGroupViewports(m_activeSceneGroup);
    activateScene(name);

    refreshFocusDiagnostics();
    m_schedulerRebuildPending = true;
}

void Game::applyPopScene() {
    if (m_sceneStack.empty()) {
        return;
    }

    SceneGroup restored = std::move(m_sceneStack.back());
    m_sceneStack.pop_back();

    std::unordered_set<std::string> keep(restored.sceneNames.begin(), restored.sceneNames.end());
    for (const auto& stacked : m_sceneStack) {
        keep.insert(stacked.sceneNames.begin(), stacked.sceneNames.end());
    }

    // Exit the popped group, topmost scene first
    const std::vector<std::string> popped = m_activeSceneGroup.sceneNames;
    for (auto it = popped.rbegin(); it != popped.rend(); ++it) {
        if (!keep.contains(*it)) {
            m_pausedSceneNames.erase(*it);
            deactivateScene(*it);
        }
    }

    m_activeSceneGroup = std::move(restored);
    m_activeScene =
        m_activeSceneGroup.empty() ? nullptr : getScene(m_activeSceneGroup.sceneNames.front());

    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        if (m_pausedSceneNames.erase(sceneName) > 0) {
            Scene* scene = getScene(sceneName);
            scene->m_diagnostics.resumeCount++;
            scene->onResume();
        }
        activateScene(sceneName);
    }

    refreshFocusDiagnostics();
    m_schedulerRebuildPending = true;
}

void Game::applyGroupViewports(const SceneGroup& group) {
    for (const auto& sceneName : group.sceneNames) {
        Scene* scene = getScene(sceneName);
        if (!scene) {
            continue;
        }
        const ViewportRect viewport = resolveGroupViewport(group, sceneName, *scene);
        const bool hasEntry = std::ranges::any_of(group.entries, [&](const SceneGroupEntry& entry) {
            return entry.sceneName == sceneName;
        });
        if (hasEntry) {
            if (scene->getViewportRect() != viewport) {
                scene->setViewportRect(viewport);
                m_groupAssignedViewports[sceneName] = viewport;
            }
        } else {
            scene->setViewportRect(viewport);
            m_groupAssignedViewports.erase(sceneName);
        }
    }
}

ViewportRect Game::resolveGroupViewport(const SceneGroup& group, const std::string& sceneName,
                                        const Scene& scene) const {
    for (const auto& entry : group.entries) {
        if (entry.sceneName == sceneName) {
            return entry.viewport;
        }
    }
    // No entry: a viewport an earlier group assigned (and the scene has not changed since)
    // reverts to full window; a viewport the scene chose itself is kept.
    auto assigned = m_groupAssignedViewports.find(sceneName);
    if (assigned != m_groupAssignedViewports.end() && scene.getViewportRect() == assigned->second) {
        return ViewportRect::fullWindow();
    }
    return scene.getViewportRect();
}

void Game::refreshFocusDiagnostics() {
    for (auto& [name, scenePtr] : m_scenes) {
        if (scenePtr) {
            scenePtr->m_diagnostics.isFocused = false;
        }
    }
    if (Scene* focused = getFocusedScene()) {
        focused->m_diagnostics.isFocused = true;
    }
}

// ============================================================================
// Transitions
// ============================================================================

void Game::transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                             float duration) {
    if (!sceneExists(name)) {
        throw std::runtime_error("[VDE] transitionToScene: unknown scene '" + name + "'");
    }
    transitionToSceneGroup(SceneGroup::create(name, {name}), std::move(transition), duration);
}

void Game::transitionToScene(const std::string& name, std::unique_ptr<Transition> transition,
                             float duration, const ViewportRect& region) {
    if (!sceneExists(name)) {
        throw std::runtime_error("[VDE] transitionToScene: unknown scene '" + name + "'");
    }
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::StartTransition;
    request.sceneName = name;
    request.transition = std::move(transition);
    request.duration = duration;
    request.hasRegion = true;
    request.region = region;
    requestSceneFlow(std::move(request));
}

void Game::transitionToSceneGroup(const SceneGroup& group, std::unique_ptr<Transition> transition,
                                  float duration) {
    if (group.empty()) {
        throw std::runtime_error("[VDE] transitionToSceneGroup: group '" + group.name +
                                 "' is empty");
    }
    for (const auto& sceneName : group.sceneNames) {
        if (!sceneExists(sceneName)) {
            throw std::runtime_error("[VDE] transitionToSceneGroup: unknown scene '" + sceneName +
                                     "'");
        }
    }
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::StartTransition;
    request.group = group;
    request.transition = std::move(transition);
    request.duration = duration;
    requestSceneFlow(std::move(request));
}

void Game::applyStartTransition(SceneFlowRequest& request) {
    // Build the group that becomes active when the transition completes.
    SceneGroup destGroup;
    std::string sourceScene;
    if (request.hasRegion) {
        const std::string& destName = request.sceneName;
        if (!sceneExists(destName)) {
            std::cerr << "[VDE] transitionToScene: unknown scene '" << destName
                      << "'; request ignored\n";
            return;
        }

        // Source: the active scene whose viewport is exactly the region, else the
        // topmost one containing the region's center.
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            const Scene* scene = getScene(sceneName);
            if (scene && scene->getViewportRect() == request.region) {
                sourceScene = sceneName;
            }
        }
        if (sourceScene.empty()) {
            const float cx = request.region.x + (request.region.width * 0.5f);
            const float cy = request.region.y + (request.region.height * 0.5f);
            for (auto it = m_activeSceneGroup.sceneNames.rbegin();
                 it != m_activeSceneGroup.sceneNames.rend(); ++it) {
                const Scene* scene = getScene(*it);
                if (scene && scene->getViewportRect().contains(cx, cy)) {
                    sourceScene = *it;
                    break;
                }
            }
        }
        if (destName == sourceScene) {
            return;  // Already showing the destination in that region
        }
    }

    // Cancel an in-progress transition before validating against the reverted state.
    applyCancelTransition();

    if (request.hasRegion) {
        const std::string& destName = request.sceneName;
        if (m_activeSceneNames.contains(destName)) {
            std::cerr << "[VDE] transitionToScene(region): scene '" << destName
                      << "' is already active or paused; request ignored\n";
            return;
        }
        destGroup.name = m_activeSceneGroup.name;
        bool replaced = false;
        for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
            if (sceneName == sourceScene) {
                destGroup.entries.push_back({.sceneName = destName, .viewport = request.region});
                replaced = true;
            } else {
                const Scene* scene = getScene(sceneName);
                destGroup.entries.push_back(
                    {.sceneName = sceneName,
                     .viewport = scene ? scene->getViewportRect() : ViewportRect::fullWindow()});
            }
        }
        if (!replaced) {
            destGroup.entries.push_back({.sceneName = destName, .viewport = request.region});
        }
        for (const auto& entry : destGroup.entries) {
            destGroup.sceneNames.push_back(entry.sceneName);
        }
    } else {
        for (const auto& sceneName : request.group.sceneNames) {
            if (!sceneExists(sceneName)) {
                std::cerr << "[VDE] transitionToSceneGroup: unknown scene '" << sceneName
                          << "'; request ignored\n";
                return;
            }
        }
        destGroup = request.group;
    }

    const bool animated = m_transitionManager && request.transition && request.duration > 0.0f;
    if (!animated) {
        if (request.transition) {
            request.transition->onStart();
            request.transition->onComplete();
        }
        applySceneGroup(destGroup, /*keepStack=*/request.hasRegion);
        return;
    }

    m_transitionActive = true;
    m_transitionCompletionPending = false;
    m_transitionDestGroup = destGroup;
    m_viewportTransition = request.hasRegion;
    m_transitionViewport = request.hasRegion ? request.region : ViewportRect::fullWindow();
    m_transitionSourceScene = request.hasRegion ? sourceScene : std::string{};
    m_transitionDestScene = request.hasRegion ? request.sceneName : std::string{};
    m_transitionEnteredScenes.clear();
    m_transitionResumedScenes.clear();
    const uint64_t transitionId = ++m_transitionGeneration;

    // Destination scenes start updating now: enter new ones, resume paused ones.
    for (const auto& sceneName : destGroup.sceneNames) {
        if (m_pausedSceneNames.erase(sceneName) > 0) {
            Scene* scene = getScene(sceneName);
            scene->m_diagnostics.resumeCount++;
            scene->onResume();
            m_transitionResumedScenes.push_back(sceneName);
        } else if (!m_activeSceneNames.contains(sceneName)) {
            activateScene(sceneName);
            m_transitionEnteredScenes.push_back(sceneName);
        }
    }

    m_transitionManager->start(std::move(request.transition), request.duration,
                               [this, transitionId]() {
                                   // Effect finished (mid-frame): keep drawing the destination
                                   // and swap groups at the next frame boundary.
                                   m_transitionCompletionPending = true;
                                   SceneFlowRequest complete;
                                   complete.kind = SceneFlowRequest::Kind::CompleteTransition;
                                   complete.transitionId = transitionId;
                                   requestSceneFlow(std::move(complete));
                               });
    m_schedulerRebuildPending = true;
}

void Game::applyCancelTransition() {
    if (!m_transitionActive) {
        return;
    }

    if (m_transitionManager) {
        m_transitionManager->cancel();
    }

    // Undo the transition start: exit what it entered, re-pause what it resumed.
    const std::vector<std::string> entered = m_transitionEnteredScenes;
    const std::vector<std::string> resumed = m_transitionResumedScenes;
    clearTransitionState();

    for (auto it = entered.rbegin(); it != entered.rend(); ++it) {
        deactivateScene(*it);
    }
    for (const auto& sceneName : resumed) {
        Scene* scene = getScene(sceneName);
        if (scene && m_activeSceneNames.contains(sceneName) &&
            m_pausedSceneNames.insert(sceneName).second) {
            scene->m_diagnostics.pauseCount++;
            scene->onPause();
        }
    }

    refreshFocusDiagnostics();
    m_schedulerRebuildPending = true;
}

void Game::applyCompleteTransition() {
    if (!m_transitionActive) {
        return;
    }

    const SceneGroup destGroup = m_transitionDestGroup;
    const bool keepStack = m_viewportTransition;
    if (m_transitionManager) {
        m_transitionManager->cancel();  // No-op when the effect already finished
    }
    clearTransitionState();
    applySceneGroup(destGroup, keepStack);
}

void Game::clearTransitionState() {
    m_transitionActive = false;
    m_transitionCompletionPending = false;
    m_transitionDestGroup = SceneGroup{};
    m_transitionSourceScene.clear();
    m_transitionDestScene.clear();
    m_transitionEnteredScenes.clear();
    m_transitionResumedScenes.clear();
    m_viewportTransition = false;
    m_transitionViewport = ViewportRect::fullWindow();
}

bool Game::isTransitioning() const {
    if (m_transitionActive) {
        return true;
    }
    return m_transitionManager &&
           std::ranges::any_of(m_sceneFlowRequests, [](const SceneFlowRequest& request) {
               return request.kind == SceneFlowRequest::Kind::StartTransition &&
                      request.transition && request.duration > 0.0f;
           });
}

void Game::cancelTransition() {
    if (!isTransitioning()) {
        return;
    }
    SceneFlowRequest request;
    request.kind = SceneFlowRequest::Kind::CancelTransition;
    requestSceneFlow(std::move(request));
}

float Game::getTransitionProgress() const {
    if (m_transitionCompletionPending) {
        return 1.0f;
    }
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
    constexpr uint32_t slotsPerFrame = kMaxLightingSlotsPerFrame;
    constexpr uint32_t setCount = framesInFlight * slotsPerFrame;

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

    // One descriptor set per (frame-in-flight, lighting slot)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = setCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = setCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_lightingDescriptorPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor pool");
    }

    // Each frame's buffer holds slotsPerFrame LightingUBOs at aligned offsets so that
    // scenes drawn in the same frame keep independent lighting.
    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(m_vulkanContext->getPhysicalDevice(), &deviceProperties);
    const VkDeviceSize alignment =
        std::max<VkDeviceSize>(deviceProperties.limits.minUniformBufferOffsetAlignment, 1);
    m_lightingSlotStride = ((sizeof(LightingUBO) + alignment - 1) / alignment) * alignment;
    const VkDeviceSize bufferSize = m_lightingSlotStride * slotsPerFrame;

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
    std::vector<VkDescriptorSetLayout> layouts(setCount, m_lightingDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_lightingDescriptorPool;
    allocInfo.descriptorSetCount = setCount;
    allocInfo.pSetLayouts = layouts.data();

    m_lightingDescriptorSets.resize(setCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, m_lightingDescriptorSets.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate lighting descriptor sets");
    }

    // Point each descriptor set at its frame buffer + slot offset
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (uint32_t frame = 0; frame < framesInFlight; frame++) {
        for (uint32_t slot = 0; slot < slotsPerFrame; slot++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_lightingUBOBuffers[frame];
            bufferInfo.offset = m_lightingSlotStride * slot;
            bufferInfo.range = sizeof(LightingUBO);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_lightingDescriptorSets[(frame * slotsPerFrame) + slot];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    // Initialize every slot with default lighting (white ambient)
    LightingUBO defaultLighting{};
    defaultLighting.ambientColorAndIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f);
    defaultLighting.lightCounts = glm::ivec4(0, 0, 0, 0);

    for (uint32_t frame = 0; frame < framesInFlight; frame++) {
        auto* base = static_cast<unsigned char*>(m_lightingUBOMapped[frame]);
        for (uint32_t slot = 0; slot < slotsPerFrame; slot++) {
            memcpy(base + (m_lightingSlotStride * slot), &defaultLighting, sizeof(LightingUBO));
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    m_lightingSlotScenes.clear();
    m_lightingSlotScenes.reserve(slotsPerFrame);
    m_currentLightingSlot = 0;

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
    m_lightingSlotScenes.clear();
    m_lightingSlotStride = 0;

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

void Game::beginLightingFrame() {
    m_lightingSlotScenes.clear();
    m_currentLightingSlot = 0;
}

VkDescriptorSet Game::getCurrentLightingDescriptorSet() const {
    if (m_lightingDescriptorSets.empty() || !m_vulkanContext) {
        return VK_NULL_HANDLE;
    }
    const size_t framesInFlight = m_lightingDescriptorSets.size() / kMaxLightingSlotsPerFrame;
    size_t currentFrame = m_vulkanContext->getCurrentFrame();
    if (currentFrame >= framesInFlight) {
        currentFrame = 0;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return m_lightingDescriptorSets[(currentFrame * kMaxLightingSlotsPerFrame) +
                                    m_currentLightingSlot];
}

VkDescriptorSet Game::getLightingDescriptorSet(const Scene* scene) {
    updateLightingUBO(scene);
    return getCurrentLightingDescriptorSet();
}

void Game::updateLightingUBO(const Scene* scene) {
    if (!m_vulkanContext || m_lightingUBOMapped.empty()) {
        return;
    }

    // Reuse the scene's slot if it already uploaded this frame
    auto existing = std::ranges::find(m_lightingSlotScenes, scene);
    if (existing != m_lightingSlotScenes.end()) {
        m_currentLightingSlot =
            static_cast<uint32_t>(std::distance(m_lightingSlotScenes.begin(), existing));
        return;
    }

    uint32_t slot = 0;
    if (m_lightingSlotScenes.size() < kMaxLightingSlotsPerFrame) {
        slot = static_cast<uint32_t>(m_lightingSlotScenes.size());
        m_lightingSlotScenes.push_back(scene);
    } else {
        // Out of slots: the last slot is shared, so earlier users of it see this scene's lighting.
        if (!m_lightingSlotOverflowWarned) {
            std::cerr << "[VDE] More than " << kMaxLightingSlotsPerFrame
                      << " lit scenes drawn in one frame; extra scenes share a lighting slot\n";
            m_lightingSlotOverflowWarned = true;
        }
        slot = kMaxLightingSlotsPerFrame - 1;
        m_lightingSlotScenes.back() = scene;
    }
    m_currentLightingSlot = slot;

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
    memcpy(static_cast<unsigned char*>(m_lightingUBOMapped[currentFrame]) +
               (m_lightingSlotStride * slot),
           &ubo, sizeof(LightingUBO));
}

void Game::setFocusedScene(const std::string& sceneName) {
    m_focusedSceneName = sceneName;
    refreshFocusDiagnostics();
}

Scene* Game::getFocusedScene() {
    if (!m_focusedSceneName.empty() && groupContains(m_activeSceneGroup, m_focusedSceneName)) {
        auto it = m_scenes.find(m_focusedSceneName);
        if (it != m_scenes.end()) {
            return it->second.get();
        }
    }
    return m_activeScene;  // Default to primary scene
}

const Scene* Game::getFocusedScene() const {
    // The explicit focus only applies while its scene is in the active (top) group, so a
    // pushed scene receives input and focus returns to the explicit scene after popScene().
    if (!m_focusedSceneName.empty() && groupContains(m_activeSceneGroup, m_focusedSceneName)) {
        auto it = m_scenes.find(m_focusedSceneName);
        if (it != m_scenes.end()) {
            return it->second.get();
        }
    }
    return m_activeScene;  // Default to primary scene
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

std::vector<Game::RenderLayer> Game::collectStackLayers() const {
    std::vector<RenderLayer> layers;
    for (const auto& stacked : m_sceneStack) {
        for (const auto& sceneName : stacked.sceneNames) {
            auto it = m_scenes.find(sceneName);
            if (it != m_scenes.end() && it->second) {
                layers.push_back(
                    {.scene = it->second.get(), .viewport = it->second->getViewportRect()});
            }
        }
    }
    return layers;
}

std::vector<Game::RenderLayer> Game::collectVisibleLayers() const {
    std::vector<RenderLayer> layers = collectStackLayers();
    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end() && it->second) {
            layers.push_back(
                {.scene = it->second.get(), .viewport = it->second->getViewportRect()});
        }
    }
    return layers;
}

std::vector<Game::RenderLayer> Game::collectGroupLayers(const SceneGroup& group) const {
    std::vector<RenderLayer> layers;
    for (const auto& sceneName : group.sceneNames) {
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end() && it->second) {
            layers.push_back({.scene = it->second.get(),
                              .viewport = resolveGroupViewport(group, sceneName, *it->second)});
        }
    }
    return layers;
}

std::vector<Game::RenderLayer> Game::collectFrameLayers() const {
    if (m_transitionActive && m_transitionCompletionPending) {
        // The effect finished this frame; draw what becomes active at the next frame boundary.
        std::vector<RenderLayer> layers =
            m_viewportTransition ? collectStackLayers() : std::vector<RenderLayer>{};
        std::vector<RenderLayer> dest = collectGroupLayers(m_transitionDestGroup);
        layers.insert(layers.end(), dest.begin(), dest.end());
        return layers;
    }
    return collectVisibleLayers();
}

void Game::renderSingleViewport(Scene* scene) {
    if (scene && scene->getCamera()) {
        // Match the full-window aspect: the scene may have just left a sub-viewport
        // (split screen, region transition) whose aspect its camera still carries.
        const VkExtent2D extent = m_vulkanContext->getSwapChainExtent();
        scene->getCamera()->setAspectRatio(
            ViewportRect::fullWindow().getAspectRatio(extent.width, extent.height));
        scene->getCamera()->applyTo(*m_vulkanContext);
    }

    m_vulkanContext->setRenderCallback([this, scene](VkCommandBuffer /*cmd*/) {
        if (scene) {
            scene->render();
        }
        onRender();
    });

    m_vulkanContext->drawFrame();
}

void Game::renderLayers(const std::vector<RenderLayer>& layers) {
    std::vector<VulkanContext::SceneRenderInfo> renderInfos =
        buildSceneRenderInfos(*m_vulkanContext, layers);

    // Run the onRender hook (ImGui etc.) after the topmost scene
    if (!renderInfos.empty()) {
        auto topCallback = std::move(renderInfos.back().renderCallback);
        renderInfos.back().renderCallback = [this, topCallback](VkCommandBuffer cmd) {
            if (topCallback) {
                topCallback(cmd);
            }
            onRender();
        };
    }

    m_vulkanContext->drawFrameMultiScene(renderInfos);
}

void Game::renderTransition() {
    // Source = what was visible before the transition; dest = what will be visible after.
    // Region transitions only render the replaced scene into the offscreen targets and draw
    // every other visible scene directly to the swapchain beneath the scissored composite.
    std::vector<RenderLayer> sourceLayers;
    std::vector<RenderLayer> destLayers;
    std::vector<RenderLayer> baseLayers;

    if (m_viewportTransition) {
        for (const auto& layer : collectVisibleLayers()) {
            if (layer.scene->getName() == m_transitionSourceScene) {
                sourceLayers.push_back(layer);
            } else {
                baseLayers.push_back(layer);
            }
        }
        if (Scene* dest = getScene(m_transitionDestScene)) {
            destLayers.push_back({.scene = dest, .viewport = m_transitionViewport});
        }
    } else {
        sourceLayers = collectVisibleLayers();
        destLayers = collectGroupLayers(m_transitionDestGroup);
    }

    std::vector<VulkanContext::SceneRenderInfo> sourceInfos =
        buildSceneRenderInfos(*m_vulkanContext, sourceLayers);
    std::vector<VulkanContext::SceneRenderInfo> destInfos =
        buildSceneRenderInfos(*m_vulkanContext, destLayers);
    std::vector<VulkanContext::SceneRenderInfo> swapchainInfos =
        buildSceneRenderInfos(*m_vulkanContext, baseLayers);

    // Final swapchain pass: the composite (scissored to the region), then the onRender hook.
    const VkExtent2D extent = m_vulkanContext->getSwapChainExtent();
    VulkanContext::SceneRenderInfo compositeInfo{};
    compositeInfo.viewMatrix = glm::mat4(1.0f);
    compositeInfo.projMatrix = glm::mat4(1.0f);
    compositeInfo.viewport = ViewportRect::fullWindow().toVkViewport(extent.width, extent.height);
    compositeInfo.scissor = ViewportRect::fullWindow().toVkScissor(extent.width, extent.height);
    compositeInfo.renderCallback = [this](VkCommandBuffer cmd) {
        m_transitionManager->renderComposite(cmd, m_transitionViewport);
        onRender();
    };
    swapchainInfos.push_back(std::move(compositeInfo));

    const glm::vec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 sourceClear = bottomLayerClearColor(sourceLayers, black);
    const glm::vec4 destClear = bottomLayerClearColor(destLayers, black);
    const glm::vec4 swapchainClear = bottomLayerClearColor(baseLayers, black);

    m_vulkanContext->drawFrameCustom(
        [&](VkCommandBuffer cmd, VkFramebuffer swapchainFB, VkImage swapchainImage) {
            VkRenderPass offscreenClearPass = m_vulkanContext->getOffscreenRenderPass();
            VkRenderPass offscreenLoadPass = m_vulkanContext->getOffscreenRenderPassLoad();

            // Pass 1/2: source and destination scenes into the offscreen targets
            if (VkFramebuffer sourceFB = m_transitionManager->getSourceFramebuffer();
                sourceFB != VK_NULL_HANDLE) {
                m_vulkanContext->recordScenePasses(cmd, sourceFB, offscreenClearPass,
                                                   offscreenLoadPass, sourceInfos, sourceClear);
            }
            if (VkFramebuffer destFB = m_transitionManager->getDestFramebuffer();
                destFB != VK_NULL_HANDLE) {
                m_vulkanContext->recordScenePasses(cmd, destFB, offscreenClearPass,
                                                   offscreenLoadPass, destInfos, destClear);
            }

            // Pass 3: untouched scenes (region transitions) + composite to the swapchain
            m_vulkanContext->recordScenePasses(cmd, swapchainFB, m_vulkanContext->getRenderPass(),
                                               m_vulkanContext->getRenderPassLoad(), swapchainInfos,
                                               swapchainClear);

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
    m_schedulerRebuildPending = false;

    // ---------------------------------------------------------------
    // Collect scenes that need updating this frame:
    //   1. All scenes in the active group (paused stack scenes are excluded)
    //   2. During a transition, the destination group's scenes
    //   3. Any other scene with continueInBackground==true
    // A transition that freezes its source excludes the scenes being replaced.
    // ---------------------------------------------------------------
    struct SceneEntry {
        Scene* scene = nullptr;
        std::string name;
        int priority = 0;
    };

    std::vector<SceneEntry> updateScenes;
    std::unordered_set<std::string> listedScenes;

    std::unordered_set<std::string> frozenScenes;
    const Transition* activeTransition =
        m_transitionManager ? m_transitionManager->getActiveTransition() : nullptr;
    if (m_transitionActive && activeTransition && activeTransition->freezesSourceScene()) {
        if (m_viewportTransition) {
            frozenScenes.insert(m_transitionSourceScene);
        } else {
            for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
                if (!groupContains(m_transitionDestGroup, sceneName)) {
                    frozenScenes.insert(sceneName);
                }
            }
        }
    }

    auto addUpdateScene = [&](const std::string& sceneName) {
        if (frozenScenes.contains(sceneName) || !listedScenes.insert(sceneName).second) {
            return;
        }
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end() && it->second) {
            updateScenes.push_back({.scene = it->second.get(),
                                    .name = sceneName,
                                    .priority = it->second->getUpdatePriority()});
        }
    };

    for (const auto& sceneName : m_activeSceneGroup.sceneNames) {
        addUpdateScene(sceneName);
    }
    if (m_transitionActive) {
        for (const auto& sceneName : m_transitionDestGroup.sceneNames) {
            addUpdateScene(sceneName);
        }
    }
    for (auto& [name, scenePtr] : m_scenes) {
        if (scenePtr && scenePtr->getContinueInBackground()) {
            addUpdateScene(name);
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
                                             scene->updateCameraDependentVisuals(m_deltaTime);
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
                                             scene->updateCameraDependentVisuals(m_deltaTime);
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
    // Task 3: PreRender — advance the active transition effect.
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

    TaskId preRenderTask =
        m_scheduler.addTask({.name = "scene.preRender",
                             .phase = TaskPhase::PreRender,
                             .work =
                                 [this]() {
                                     if (m_transitionManager && m_transitionManager->isActive()) {
                                         m_transitionManager->update(m_deltaTime);
                                     }
                                 },
                             .dependsOn = preRenderDeps,
                             .mainThreadOnly = true});

    TaskId renderDep = preRenderTask;

    // ---------------------------------------------------------------
    // Task 4: Render — draw frame.  When transitioning, render the source
    //         and destination to offscreen targets and composite.
    //         Otherwise draw every visible layer (paused stack scenes,
    //         then the active group) with its own camera and lighting.
    // ---------------------------------------------------------------
    m_scheduler.addTask(
        {.name = "scene.render",
         .phase = TaskPhase::Render,
         .work =
             [this]() {
                 if (!m_vulkanContext) {
                     return;
                 }

                 beginLightingFrame();

                 if (m_transitionActive && m_transitionManager && m_transitionManager->isActive()) {
                     renderTransition();
                     return;
                 }

                 const std::vector<RenderLayer> layers = collectFrameLayers();
                 if (!layers.empty()) {
                     // Clear with the bottom (primary) layer's background color
                     m_vulkanContext->setClearColor(
                         bottomLayerClearColor(layers, m_vulkanContext->getClearColor()));
                 }

                 // A lone full-window scene uses the single-pass path; anything else
                 // (several scenes, or a sub-window viewport) renders one pass per scene.
                 if (layers.empty() || (layers.size() == 1 &&
                                        layers.front().viewport == ViewportRect::fullWindow())) {
                     renderSingleViewport(layers.empty() ? nullptr : layers.front().scene);
                 } else {
                     renderLayers(layers);
                 }
             },
         .dependsOn = {renderDep},
         .mainThreadOnly = true});
}

}  // namespace vde
