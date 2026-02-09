/**
 * @file main.cpp
 * @brief Dear ImGui Integration Demo for VDE
 *
 * Demonstrates how to integrate Dear ImGui with the VDE engine as an
 * application-side overlay. ImGui is NOT part of the VDE engine itself;
 * it is pulled in by the example via CMake FetchContent and rendered
 * into VDE's existing render pass using its own Vulkan backend.
 *
 * This approach keeps ImGui out of the engine core while giving
 * applications full access to ImGui's debug/tool UI capabilities.
 *
 * Features demonstrated:
 * - ImGui initialization with VDE's Vulkan context
 * - Rendering ImGui as an overlay on top of VDE scenes
 * - ImGui controlling VDE scene properties (entity transforms, colors, lighting)
 * - FPS counter and engine stats window
 * - Entity inspector panel
 *
 * Controls:
 *   Mouse  - Interact with ImGui panels
 *   ESC    - Exit
 *   F      - Fail test
 */

#include <vde/VulkanContext.h>
#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

#include "../ExampleBase.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// =============================================================================
// ImGui Vulkan helpers
// =============================================================================

/// Creates a dedicated descriptor pool for ImGui's internal use.
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

// =============================================================================
// Input Handler
// =============================================================================

class ImGuiDemoInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Let base handler process ESC / F keys
        BaseExampleInputHandler::onKeyPress(key);
    }

    // We intentionally do NOT forward mouse events here because
    // ImGui_ImplGlfw installs its own GLFW callbacks.
};

// =============================================================================
// Scene
// =============================================================================

class ImGuiDemoScene : public vde::examples::BaseExampleScene {
  public:
    ImGuiDemoScene() : BaseExampleScene(60.0f) {}  // 60s timeout for interactive demo

    void onEnter() override {
        printExampleHeader();

        // --- Camera ---
        setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 30.0f, 25.0f));

        // --- Lighting ---
        auto lightBox = std::make_unique<vde::LightBox>();
        lightBox->setAmbientColor(
            vde::Color(m_ambientColor[0], m_ambientColor[1], m_ambientColor[2]));

        vde::Light sun = vde::Light::directional(vde::Direction(-0.5f, -1.0f, -0.3f),
                                                 vde::Color(1.0f, 0.95f, 0.85f), m_sunIntensity);
        lightBox->addLight(sun);
        setLightBox(std::move(lightBox));

        // --- Entities ---
        auto cube = addEntity<vde::MeshEntity>();
        cube->setMesh(vde::Mesh::createCube(1.0f));
        cube->setPosition(m_cubePos[0], m_cubePos[1], m_cubePos[2]);
        cube->setColor(vde::Color(m_cubeColor[0], m_cubeColor[1], m_cubeColor[2]));
        cube->setName("Cube");
        m_cube = cube;

        auto sphere = addEntity<vde::MeshEntity>();
        sphere->setMesh(vde::Mesh::createSphere(0.7f, 24, 24));
        sphere->setPosition(m_spherePos[0], m_spherePos[1], m_spherePos[2]);
        sphere->setColor(vde::Color(m_sphereColor[0], m_sphereColor[1], m_sphereColor[2]));
        sphere->setName("Sphere");
        m_sphere = sphere;

        auto plane = addEntity<vde::MeshEntity>();
        plane->setMesh(vde::Mesh::createPlane(10.0f, 10.0f));
        plane->setPosition(0.0f, -1.0f, 0.0f);
        plane->setColor(vde::Color(0.3f, 0.3f, 0.35f));
        plane->setName("Ground");
        m_plane = plane;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        m_totalTime += deltaTime;

        // Auto-rotate cube
        if (m_autoRotate) {
            m_cubeRotY += m_rotationSpeed * deltaTime;
            if (m_cubeRotY > 360.0f)
                m_cubeRotY -= 360.0f;
        }

        // Apply current values to entities
        if (m_cube) {
            m_cube->setPosition(m_cubePos[0], m_cubePos[1], m_cubePos[2]);
            m_cube->setRotation(0.0f, m_cubeRotY, 0.0f);
            m_cube->setScale(m_cubeScale);
            m_cube->setColor(vde::Color(m_cubeColor[0], m_cubeColor[1], m_cubeColor[2]));
        }

        if (m_sphere) {
            m_sphere->setPosition(m_spherePos[0], m_spherePos[1], m_spherePos[2]);
            m_sphere->setColor(vde::Color(m_sphereColor[0], m_sphereColor[1], m_sphereColor[2]));
        }

        // Update lighting if changed
        if (m_lightingDirty) {
            auto lightBox = std::make_unique<vde::LightBox>();
            lightBox->setAmbientColor(
                vde::Color(m_ambientColor[0], m_ambientColor[1], m_ambientColor[2]));

            vde::Light sun =
                vde::Light::directional(vde::Direction(-0.5f, -1.0f, -0.3f),
                                        vde::Color(1.0f, 0.95f, 0.85f), m_sunIntensity);
            lightBox->addLight(sun);
            setLightBox(std::move(lightBox));

            m_lightingDirty = false;
        }
    }

    /// Called by the Game subclass during ImGui rendering phase.
    void drawImGui() {
        // --- Stats overlay ---
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260, 120), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Engine Stats")) {
            auto* game = getGame();
            ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
            ImGui::Text("Frame: %llu", game ? game->getFrameCount() : 0ULL);
            ImGui::Text("Delta: %.3f ms", game ? game->getDeltaTime() * 1000.0f : 0.0f);
            ImGui::Text("Entities: %zu", getEntities().size());
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "ImGui integrated as overlay");
        }
        ImGui::End();

        // --- Cube Inspector ---
        ImGui::SetNextWindowPos(ImVec2(10, 140), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Cube Inspector")) {
            ImGui::DragFloat3("Position##cube", m_cubePos, 0.1f, -10.0f, 10.0f);
            ImGui::SliderFloat("Scale", &m_cubeScale, 0.1f, 5.0f);
            ImGui::ColorEdit3("Color##cube", m_cubeColor);
            ImGui::Separator();
            ImGui::Checkbox("Auto Rotate", &m_autoRotate);
            if (m_autoRotate) {
                ImGui::SliderFloat("Speed (deg/s)", &m_rotationSpeed, 10.0f, 360.0f);
            }
            ImGui::SliderFloat("Rotation Y", &m_cubeRotY, 0.0f, 360.0f);
        }
        ImGui::End();

        // --- Sphere Inspector ---
        ImGui::SetNextWindowPos(ImVec2(10, 450), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 140), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Sphere Inspector")) {
            ImGui::DragFloat3("Position##sphere", m_spherePos, 0.1f, -10.0f, 10.0f);
            ImGui::ColorEdit3("Color##sphere", m_sphereColor);
        }
        ImGui::End();

        // --- Lighting ---
        ImGui::SetNextWindowPos(ImVec2(300, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260, 140), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Lighting")) {
            if (ImGui::ColorEdit3("Ambient", m_ambientColor)) {
                m_lightingDirty = true;
            }
            if (ImGui::SliderFloat("Sun Intensity", &m_sunIntensity, 0.0f, 3.0f)) {
                m_lightingDirty = true;
            }
        }
        ImGui::End();

        // --- ImGui demo window (toggle with checkbox) ---
        ImGui::SetNextWindowPos(ImVec2(300, 160), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260, 50), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Options")) {
            ImGui::Checkbox("Show ImGui Demo Window", &m_showDemoWindow);
        }
        ImGui::End();

        if (m_showDemoWindow) {
            ImGui::ShowDemoWindow(&m_showDemoWindow);
        }
    }

  protected:
    std::string getExampleName() const override { return "Dear ImGui Integration"; }

    std::vector<std::string> getFeatures() const override {
        return {"ImGui overlay on VDE scene", "Entity property editors (position, color, scale)",
                "Lighting controls", "FPS / engine stats", "ImGui Demo Window toggle"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"3D scene with cube, sphere, and ground plane",
                "Multiple ImGui windows overlaid on top",
                "Cube rotating when auto-rotate is enabled",
                "Real-time property changes reflected in the scene"};
    }

    std::vector<std::string> getControls() const override {
        return {"Mouse - Interact with ImGui panels"};
    }

  private:
    // Entity references
    std::shared_ptr<vde::MeshEntity> m_cube;
    std::shared_ptr<vde::MeshEntity> m_sphere;
    std::shared_ptr<vde::MeshEntity> m_plane;

    // ImGui-editable properties
    float m_cubePos[3] = {-1.5f, 0.0f, 0.0f};
    float m_cubeColor[3] = {0.2f, 0.5f, 0.9f};
    float m_cubeScale = 1.0f;
    float m_cubeRotY = 0.0f;
    bool m_autoRotate = true;
    float m_rotationSpeed = 90.0f;

    float m_spherePos[3] = {1.5f, 0.0f, 0.0f};
    float m_sphereColor[3] = {0.9f, 0.3f, 0.2f};

    float m_ambientColor[3] = {0.15f, 0.15f, 0.2f};
    float m_sunIntensity = 1.0f;
    bool m_lightingDirty = false;

    bool m_showDemoWindow = false;
    float m_totalTime = 0.0f;
};

// =============================================================================
// Game – manages ImGui lifecycle alongside the VDE engine
// =============================================================================

class ImGuiDemoGame : public vde::examples::BaseExampleGame<ImGuiDemoInputHandler, ImGuiDemoScene> {
  public:
    ImGuiDemoGame() = default;
    ~ImGuiDemoGame() override { cleanupImGui(); }

    void onStart() override {
        BaseExampleGame::onStart();
        initImGui();
    }

    void onRender() override {
        // This is called inside the active render pass, after
        // scene entities have been drawn – perfect for ImGui overlay.
        renderImGui();
    }

    void onShutdown() override {
        // Ensure GPU is idle before tearing down ImGui resources
        if (getVulkanContext()) {
            vkDeviceWaitIdle(getVulkanContext()->getDevice());
        }
        cleanupImGui();
        BaseExampleGame::onShutdown();
    }

  private:
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;

    // -----------------------------------------------------------------
    // ImGui init / shutdown
    // -----------------------------------------------------------------

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

        // Platform backend – GLFW.
        // install_callbacks=true lets ImGui capture input alongside VDE.
        ImGui_ImplGlfw_InitForVulkan(win->getHandle(), true);

        // Create a descriptor pool dedicated to ImGui.
        m_imguiPool = createImGuiDescriptorPool(ctx->getDevice());

        // Renderer backend – Vulkan.
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = ctx->getInstance();
        initInfo.PhysicalDevice = ctx->getPhysicalDevice();
        initInfo.Device = ctx->getDevice();
        initInfo.QueueFamily = ctx->getGraphicsQueueFamily();
        initInfo.Queue = ctx->getGraphicsQueue();
        initInfo.DescriptorPool = m_imguiPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = 2;  // double-buffered
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.RenderPass = ctx->getRenderPass();
        initInfo.Subpass = 0;

        ImGui_ImplVulkan_Init(&initInfo);

        // Upload font atlas textures.
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

    // -----------------------------------------------------------------
    // Per-frame ImGui rendering
    // -----------------------------------------------------------------

    void renderImGui() {
        if (!m_imguiInitialized)
            return;

        // Start the ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Let the scene build its ImGui windows
        auto* scene = getExampleScene();
        if (scene) {
            scene->drawImGui();
        }

        // Finalize and record draw data into the active command buffer
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
};

// =============================================================================
// Main
// =============================================================================

int main() {
    ImGuiDemoGame demo;
    return vde::examples::runExample(demo, "VDE ImGui Demo", 1280, 720);
}
