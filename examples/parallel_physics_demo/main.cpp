/**
 * @file main.cpp
 * @brief Parallel physics demo demonstrating thread pool integration
 *
 * This example demonstrates:
 * - Two scenes, each with its own PhysicsScene
 * - Thread pool with 2 worker threads
 * - Per-scene physics running in parallel on worker threads
 * - Console output showing which thread ran each physics step
 * - Split-screen viewports (left/right) for the two physics scenes
 */

#include <vde/api/GameAPI.h>

#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "../ExampleBase.h"

#ifdef VDE_EXAMPLE_USE_IMGUI
#include <vde/VulkanContext.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

// ============================================================================
// Thread logging utility
// ============================================================================

static std::mutex g_logMutex;

// ============================================================================
// Input Handler
// ============================================================================

class ParallelPhysicsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_R)
            m_resetPressed = true;
    }

    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

    bool isResetPressed() {
        bool val = m_resetPressed;
        m_resetPressed = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_resetPressed = false;
};

// ============================================================================
// Helper scene base for physics worlds
// ============================================================================

class PhysicsWorldScene : public vde::Scene {
  public:
    PhysicsWorldScene(const std::string& name, const vde::Color& bgColor,
                      const vde::Color& groundColor, const vde::Color& boxColor, float gravityY)
        : m_sceneName(name), m_bgColor(bgColor), m_groundColor(groundColor), m_boxColor(boxColor),
          m_gravityY(gravityY) {}

    void onEnter() override {
        // Enable physics
        vde::PhysicsConfig config;
        config.gravity = {0.0f, m_gravityY};
        config.fixedTimestep = 1.0f / 60.0f;
        enablePhysics(config);

        // Camera
        auto* cam = new vde::OrbitCamera(vde::Position(0.0f, 2.0f, 0.0f), 10.0f);
        setCamera(std::unique_ptr<vde::GameCamera>(cam));

        // Lighting
        setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));

        // Background
        setBackgroundColor(m_bgColor);

        // Ground
        createGround();

        // Falling boxes
        spawnBoxes();

        // Collision callback fires during step() — on the worker thread!
        getPhysicsScene()->setOnCollisionBegin([this](const vde::CollisionEvent&) {
            auto id = std::this_thread::get_id();
            auto hash = std::hash<std::thread::id>{}(id);
            m_lastPhysicsThreadHash.store(hash, std::memory_order_relaxed);
        });

        std::cout << "[" << m_sceneName << "] Initialized with "
                  << getPhysicsScene()->getBodyCount() << " bodies (gravity y=" << m_gravityY << ")"
                  << std::endl;
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        m_statusTimer += deltaTime;
        if (m_statusTimer >= 3.0f) {
            m_statusTimer = 0.0f;
            size_t threadHash = m_lastPhysicsThreadHash.load(std::memory_order_relaxed);
            if (threadHash != 0) {
                std::lock_guard<std::mutex> lock(g_logMutex);
                std::cout << "[Physics] Scene '" << m_sceneName
                          << "' last stepped on thread hash: " << threadHash << std::endl;
            }
        }
    }

    void spawnExtraBox() {
        float x = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 4.0f;
        spawnSingleBox(x, 7.0f);
    }

    void resetBoxes() {
        for (auto& sprite : m_physicsSprites) {
            if (sprite) {
                removeEntity(sprite->getId());
            }
        }
        m_physicsSprites.clear();
        spawnBoxes();
        std::cout << "[" << m_sceneName << "] Reset" << std::endl;
    }

  protected:
    std::string m_sceneName;
    vde::Color m_bgColor;
    vde::Color m_groundColor;
    vde::Color m_boxColor;
    float m_gravityY;
    float m_statusTimer = 0.0f;
    std::atomic<size_t> m_lastPhysicsThreadHash{0};
    std::vector<std::shared_ptr<vde::PhysicsSpriteEntity>> m_physicsSprites;

    void createGround() {
        auto ground = addEntity<vde::PhysicsSpriteEntity>();
        ground->setColor(m_groundColor);
        ground->setScale(vde::Scale(10.0f, 0.5f, 1.0f));

        vde::PhysicsBodyDef groundDef;
        groundDef.type = vde::PhysicsBodyType::Static;
        groundDef.shape = vde::PhysicsShape::Box;
        groundDef.position = {0.0f, -2.0f};
        groundDef.extents = {5.0f, 0.25f};
        ground->createPhysicsBody(groundDef);
    }

    void spawnBoxes() {
        float positions[][2] = {
            {-1.0f, 4.0f}, {0.0f, 5.5f}, {1.0f, 4.5f}, {-0.5f, 6.5f}, {0.5f, 7.5f},
        };

        for (int i = 0; i < 5; ++i) {
            spawnSingleBox(positions[i][0], positions[i][1]);
        }
    }

    void spawnSingleBox(float x, float y) {
        float halfSize = 0.25f;
        float variation = static_cast<float>(rand()) / RAND_MAX * 0.3f;
        vde::Color color(m_boxColor.r + variation, m_boxColor.g - variation * 0.5f,
                         m_boxColor.b + variation * 0.2f, 1.0f);

        auto sprite = addEntity<vde::PhysicsSpriteEntity>();
        sprite->setColor(color);
        sprite->setScale(vde::Scale(halfSize * 2.0f, halfSize * 2.0f, 1.0f));

        vde::PhysicsBodyDef boxDef;
        boxDef.type = vde::PhysicsBodyType::Dynamic;
        boxDef.shape = vde::PhysicsShape::Box;
        boxDef.position = {x, y};
        boxDef.extents = {halfSize, halfSize};
        boxDef.mass = 1.0f;
        boxDef.restitution = 0.4f;
        boxDef.friction = 0.3f;
        boxDef.linearDamping = 0.01f;
        sprite->createPhysicsBody(boxDef);

        m_physicsSprites.push_back(sprite);
    }
};

// ============================================================================
// Coordinator Scene (manages demo lifecycle via BaseExampleScene)
// ============================================================================

class CoordinatorScene : public vde::examples::BaseExampleScene {
  public:
    CoordinatorScene() : BaseExampleScene(12.0f) {}

    void setWorldScenes(PhysicsWorldScene* left, PhysicsWorldScene* right) {
        m_leftScene = left;
        m_rightScene = right;
    }

    void onEnter() override {
        printExampleHeader();

        // Minimal camera / lighting so the scene is valid
        auto* cam = new vde::OrbitCamera(vde::Position(0, 0, 0), 1.0f);
        setCamera(std::unique_ptr<vde::GameCamera>(cam));
        setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));
        setBackgroundColor(vde::Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<ParallelPhysicsInputHandler*>(getInputHandler());
        if (input) {
            if (input->isSpacePressed()) {
                if (m_leftScene)
                    m_leftScene->spawnExtraBox();
                if (m_rightScene)
                    m_rightScene->spawnExtraBox();
            }
            if (input->isResetPressed()) {
                if (m_leftScene)
                    m_leftScene->resetBoxes();
                if (m_rightScene)
                    m_rightScene->resetBoxes();
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Parallel Physics (Thread Pool)"; }

    std::vector<std::string> getFeatures() const override {
        return {"ThreadPool with 2 worker threads", "Two independent PhysicsScene instances",
                "Per-scene physics stepping on worker threads",
                "Split-screen viewports (left/right)", "Scheduler parallel task dispatch"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Left half: blue world with falling boxes (normal gravity)",
                "Right half: red world with falling boxes (lower gravity)",
                "Boxes falling and stacking on ground platforms",
                "Console output showing different thread IDs per scene"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Spawn extra boxes in both scenes", "R     - Reset both scenes"};
    }

  private:
    PhysicsWorldScene* m_leftScene = nullptr;
    PhysicsWorldScene* m_rightScene = nullptr;
};

// ============================================================================
// Game
// ============================================================================

class ParallelPhysicsGame : public vde::Game {
  public:
    ParallelPhysicsGame() = default;
    ~ParallelPhysicsGame() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        cleanupImGui();
#endif
    }

    void onStart() override {
        // Input handler
        m_input = std::make_unique<ParallelPhysicsInputHandler>();
        setInputHandler(m_input.get());

        // Enable thread pool with 2 workers
        getScheduler().setWorkerThreadCount(2);

        std::cout << "\n[ThreadPool] Enabled with 2 worker threads" << std::endl;
        std::cout << "[ThreadPool] Main thread: " << std::this_thread::get_id() << std::endl;

        // Create left physics scene (blue, normal gravity)
        auto* leftScene =
            new PhysicsWorldScene("LeftWorld", vde::Color(0.05f, 0.05f, 0.15f, 1.0f),  // bg
                                  vde::Color(0.2f, 0.5f, 0.8f, 1.0f),                  // ground
                                  vde::Color(0.3f, 0.5f, 0.9f, 1.0f),                  // boxes
                                  -9.81f  // normal gravity
            );
        addScene("left", leftScene);

        // Create right physics scene (red, lower gravity)
        auto* rightScene =
            new PhysicsWorldScene("RightWorld", vde::Color(0.15f, 0.05f, 0.05f, 1.0f),  // bg
                                  vde::Color(0.8f, 0.4f, 0.2f, 1.0f),                   // ground
                                  vde::Color(0.9f, 0.3f, 0.3f, 1.0f),                   // boxes
                                  -4.0f  // lower gravity
            );
        addScene("right", rightScene);

        // Create coordinator scene (invisible, manages demo lifecycle)
        auto* coordScene = new CoordinatorScene();
        coordScene->setWorldScenes(leftScene, rightScene);
        addScene("coordinator", coordScene);

        // Set up scene group with split-screen viewports
        auto group = vde::SceneGroup::createWithViewports(
            "parallel_physics", {{"left", vde::ViewportRect::leftHalf()},
                                 {"right", vde::ViewportRect::rightHalf()},
                                 {"coordinator", vde::ViewportRect::fullWindow()}});

        setActiveSceneGroup(group);

#ifdef VDE_EXAMPLE_USE_IMGUI
        initImGui();
#endif
    }

    void onRender() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        if (!m_imguiInitialized)
            return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 160), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Info")) {
            ImGui::Text("FPS: %.1f", getFPS());
            ImGui::Text("Frame: %llu", getFrameCount());
            ImGui::Text("Delta: %.3f ms", getDeltaTime() * 1000.0f);
            ImGui::Text("DPI Scale: %.2f", getDPIScale());
            ImGui::Separator();
            ImGui::Text("Workers: 2 threads");
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press F1 to toggle");
        }
        ImGui::End();

        ImGui::Render();
        auto* ctx = getVulkanContext();
        if (ctx) {
            VkCommandBuffer cmd = ctx->getCurrentCommandBuffer();
            if (cmd != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            }
        }
#endif
    }

    void onShutdown() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        if (getVulkanContext()) {
            vkDeviceWaitIdle(getVulkanContext()->getDevice());
        }
        cleanupImGui();
#endif
        // Reset thread pool before shutdown
        getScheduler().setWorkerThreadCount(0);
    }

    int getExitCode() const { return 0; }

  private:
    std::unique_ptr<ParallelPhysicsInputHandler> m_input;

#ifdef VDE_EXAMPLE_USE_IMGUI
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;

    VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
        VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}};
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = poolSizes;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
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

        float dpiScale = getDPIScale();
        if (dpiScale > 0.0f)
            io.FontGlobalScale = dpiScale;

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

    void cleanupImGui() {
        if (!m_imguiInitialized)
            return;
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (m_imguiPool != VK_NULL_HANDLE) {
            auto* ctx = getVulkanContext();
            if (ctx && ctx->getDevice()) {
                vkDestroyDescriptorPool(ctx->getDevice(), m_imguiPool, nullptr);
            }
            m_imguiPool = VK_NULL_HANDLE;
        }
        m_imguiInitialized = false;
    }
#endif
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ParallelPhysicsGame game;

    // Configure input script from CLI args if provided
    if (argc > 0 && argv != nullptr) {
        vde::configureInputScriptFromArgs(game, argc, argv);
    }

    vde::GameSettings settings;
    settings.gameName = "VDE Parallel Physics Demo";
    settings.display.windowWidth = 1280;
    settings.display.windowHeight = 720;
    settings.display.fullscreen = false;

    try {
        if (!game.initialize(settings)) {
            std::cerr << "Failed to initialize!" << std::endl;
            return 1;
        }
        game.run();
        return game.getExitCode();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
