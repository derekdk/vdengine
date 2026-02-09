/**
 * @file main.cpp
 * @brief Simple game example using the VDE Game API.
 *
 * This example demonstrates:
 * - Using the Game class for initialization
 * - Creating custom scenes
 * - Handling input
 * - Managing entities
 * - Basic game loop
 */

#include <vde/api/GameAPI.h>

#include <iostream>

#include "../ExampleBase.h"

/**
 * @brief Custom input handler for the game.
 */
class GameInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    GameInputHandler() = default;

    void onKeyPress(int key) override {
        // Call base class first for ESC and F keys
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
        if (key == vde::KEY_W)
            m_moveForward = true;
        if (key == vde::KEY_S)
            m_moveBackward = true;
        if (key == vde::KEY_A)
            m_moveLeft = true;
        if (key == vde::KEY_D)
            m_moveRight = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_W)
            m_moveForward = false;
        if (key == vde::KEY_S)
            m_moveBackward = false;
        if (key == vde::KEY_A)
            m_moveLeft = false;
        if (key == vde::KEY_D)
            m_moveRight = false;
    }

    void onMouseMove(double x, double y) override {
        m_mouseX = x;
        m_mouseY = y;
    }

    void onMouseScroll(double /* xOffset */, double yOffset) override {
        m_scrollDelta = static_cast<float>(yOffset);
    }

    // Query methods
    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

    bool isMovingForward() const { return m_moveForward; }
    bool isMovingBackward() const { return m_moveBackward; }
    bool isMovingLeft() const { return m_moveLeft; }
    bool isMovingRight() const { return m_moveRight; }

    double getMouseX() const { return m_mouseX; }
    double getMouseY() const { return m_mouseY; }

    float getScrollDelta() {
        float val = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
    float m_scrollDelta = 0.0f;
};

/**
 * @brief A rotating cube entity.
 */
class RotatingCube : public vde::MeshEntity {
  public:
    RotatingCube() = default;

    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }

    void update(float deltaTime) override {
        // Rotate the cube over time
        auto rot = getRotation();
        rot.yaw += m_rotationSpeed * deltaTime;
        if (rot.yaw > 360.0f)
            rot.yaw -= 360.0f;
        setRotation(rot);
    }

  private:
    float m_rotationSpeed = 45.0f;  // degrees per second
};

/**
 * @brief Main game scene with a rotating cube.
 */
class MainScene : public vde::examples::BaseExampleScene {
  public:
    MainScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        // Print standard header
        printExampleHeader();

        // Set up an orbit camera looking at the origin
        setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 5.0f, 20.0f, 45.0f));

        // Set background color to dark blue
        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));

        // Create a rotating cube entity
        m_cube = addEntity<RotatingCube>();
        m_cube->setName("MainCube");
        m_cube->setPosition(0.0f, 0.0f, 0.0f);
        m_cube->setColor(vde::Color::fromHex(0x4a90d9));
        m_cube->setRotationSpeed(30.0f);

        // Set a cube mesh
        m_cube->setMesh(vde::Mesh::createCube(1.0f));
    }

    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);

        // Handle input
        auto* input = dynamic_cast<GameInputHandler*>(getInputHandler());
        if (input) {
            // Camera zoom with scroll
            float scroll = input->getScrollDelta();
            if (scroll != 0.0f) {
                auto* orbitCam = dynamic_cast<vde::OrbitCamera*>(getCamera());
                if (orbitCam) {
                    float newDist = orbitCam->getDistance() - scroll * 0.5f;
                    orbitCam->setDistance(std::max(2.0f, std::min(20.0f, newDist)));
                }
            }

            // Speed up/slow down cube with space
            if (input->isSpacePressed()) {
                m_speedMultiplier = (m_speedMultiplier == 1.0f) ? 3.0f : 1.0f;
                m_cube->setRotationSpeed(30.0f * m_speedMultiplier);
                std::cout << "Rotation speed: " << (m_speedMultiplier == 1.0f ? "normal" : "fast")
                          << std::endl;
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Simple Game"; }

    std::vector<std::string> getFeatures() const override {
        return {"Game class initialization", "Scene management", "MeshEntity with rotation",
                "OrbitCamera controls"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Blue rotating cube at origin", "Dark blue background"};
    }

    std::vector<std::string> getControls() const override {
        return {"SCROLL - Zoom camera in/out", "SPACE  - Toggle rotation speed"};
    }

  private:
    std::shared_ptr<RotatingCube> m_cube;
    float m_speedMultiplier = 1.0f;
};

/**
 * @brief Menu scene (simplified for auto-termination support).
 */
class MenuScene : public vde::Scene {
  public:
    MenuScene() = default;

    void onEnter() override {
        std::cout << "MenuScene: Press SPACE to start the game" << std::endl;
        setBackgroundColor(vde::Color::fromHex(0x0f0f23));
        m_elapsedTime = 0.0f;
    }

    void update(float deltaTime) override {
        m_elapsedTime += deltaTime;

        auto* input = dynamic_cast<GameInputHandler*>(getInputHandler());
        if (input) {
            if (input->isEscapePressed()) {
                if (getGame())
                    getGame()->quit();
                return;
            }
            if (input->isSpacePressed()) {
                // Switch to main scene
                getGame()->setActiveScene("main");
            }
        }

        // Auto-advance to main scene after 2 seconds
        if (m_elapsedTime >= 2.0f) {
            getGame()->setActiveScene("main");
        }
    }

  private:
    float m_elapsedTime = 0.0f;
};

/**
 * @brief Game class for the demo.
 */
class SimpleGameDemo : public vde::Game {
  public:
    SimpleGameDemo() = default;
    ~SimpleGameDemo() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        cleanupImGui();
#endif
    }

    void onStart() override {
        // Set up input handler
        m_inputHandler = std::make_unique<GameInputHandler>();
        setInputHandler(m_inputHandler.get());

        // Add scenes
        addScene("menu", new MenuScene());
        auto* mainScene = new MainScene();
        m_mainScenePtr = mainScene;
        addScene("main", mainScene);

        // Start with menu scene
        setActiveScene("menu");

#ifdef VDE_EXAMPLE_USE_IMGUI
        // Initialize ImGui after scene setup
        initImGui();
#endif
    }

    void onRender() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        // Render ImGui overlay for BaseExampleScene-derived scenes
        auto* activeScene = dynamic_cast<vde::examples::BaseExampleScene*>(getActiveScene());
        if (activeScene && activeScene->isDebugUIVisible()) {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            activeScene->drawDebugUI();

            ImGui::Render();
            ImDrawData* drawData = ImGui::GetDrawData();

            auto* ctx = getVulkanContext();
            if (ctx) {
                VkCommandBuffer cmd = ctx->getCurrentCommandBuffer();
                if (cmd != VK_NULL_HANDLE) {
                    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
                }
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
        if (m_mainScenePtr && m_mainScenePtr->didTestFail()) {
            m_exitCode = 1;
        }
    }

    int getExitCode() const { return m_exitCode; }

  private:
    std::unique_ptr<GameInputHandler> m_inputHandler;
    MainScene* m_mainScenePtr = nullptr;
    int m_exitCode = 0;

#ifdef VDE_EXAMPLE_USE_IMGUI
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;

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

/**
 * @brief Main entry point.
 */
int main() {
    SimpleGameDemo demo;
    return vde::examples::runExample(demo, "VDE Simple Game Example", 1280, 720);
}