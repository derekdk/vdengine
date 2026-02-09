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

        // Store DPI scale for UI scaling
        auto* game = getGame();
        if (game) {
            m_dpiScale = game->getDPIScale();
        }

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

    /// Called by BaseExampleGame during ImGui rendering phase.
    void drawDebugUI() override {
        // Apply DPI scale to all window positions and sizes
        float scale = m_dpiScale;

        // --- Stats overlay ---
        ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260 * scale, 140 * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Engine Stats")) {
            auto* game = getGame();
            ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
            ImGui::Text("Frame: %llu", game ? game->getFrameCount() : 0ULL);
            ImGui::Text("Delta: %.3f ms", game ? game->getDeltaTime() * 1000.0f : 0.0f);
            ImGui::Text("Entities: %zu", getEntities().size());
            ImGui::Text("DPI Scale: %.2f", game ? game->getDPIScale() : 1.0f);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "ImGui integrated as overlay");
        }
        ImGui::End();

        // --- Cube Inspector ---
        ImGui::SetNextWindowPos(ImVec2(10 * scale, 140 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280 * scale, 300 * scale), ImGuiCond_FirstUseEver);
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
        ImGui::SetNextWindowPos(ImVec2(10 * scale, 450 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280 * scale, 140 * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Sphere Inspector")) {
            ImGui::DragFloat3("Position##sphere", m_spherePos, 0.1f, -10.0f, 10.0f);
            ImGui::ColorEdit3("Color##sphere", m_sphereColor);
        }
        ImGui::End();

        // --- Lighting ---
        ImGui::SetNextWindowPos(ImVec2(300 * scale, 10 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260 * scale, 140 * scale), ImGuiCond_FirstUseEver);
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
        ImGui::SetNextWindowPos(ImVec2(300 * scale, 160 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260 * scale, 50 * scale), ImGuiCond_FirstUseEver);
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
    float m_dpiScale = 1.0f;
};

// =============================================================================
// Game – uses BaseExampleGame with built-in ImGui support
// =============================================================================

class ImGuiDemoGame : public vde::examples::BaseExampleGame<ImGuiDemoInputHandler, ImGuiDemoScene> {
  public:
    ImGuiDemoGame() = default;
    ~ImGuiDemoGame() override = default;

    // BaseExampleGame handles all ImGui initialization and rendering!
};

// =============================================================================
// Main
// =============================================================================

int main() {
    ImGuiDemoGame demo;

    // Adjust default resolution based on DPI
    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();

    // Scale the resolution for high DPI displays
    uint32_t width = static_cast<uint32_t>(1280 * dpiScale);
    uint32_t height = static_cast<uint32_t>(720 * dpiScale);

    return vde::examples::runExample(demo, "VDE ImGui Demo", width, height);
}
