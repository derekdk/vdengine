/**
 * @file main.cpp
 * @brief Quad Viewport Demo - Four simultaneous scenes in split-screen
 *
 * This example demonstrates rendering four independent scenes simultaneously
 * using the per-scene viewport system.  Each scene has its own Camera2D and
 * ViewportRect, and is rendered in its own Vulkan render pass:
 *
 *   +------------------+------------------+
 *   |   Top-Left:      |   Top-Right:     |
 *   |   SPACE          |   FOREST         |
 *   |   (rotating      |   (swaying       |
 *   |    planets)       |    trees)        |
 *   +------------------+------------------+
 *   |   Bottom-Left:   |   Bottom-Right:  |
 *   |   CITY           |   OCEAN          |
 *   |   (pulsing       |   (animated      |
 *   |    buildings)     |    waves)        |
 *   +------------------+------------------+
 *
 * Each quadrant is a truly independent scene with its own camera.
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../ExampleBase.h"

#ifdef VDE_EXAMPLE_USE_IMGUI
#include <vde/VulkanContext.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

using namespace vde;

// ============================================================================
// Helper: cast Entity* to SpriteEntity*
// ============================================================================

static SpriteEntity* asSprite(Entity* e) {
    return dynamic_cast<SpriteEntity*>(e);
}

// ============================================================================
// Constants — each scene covers this many world units
// ============================================================================

static constexpr float kSceneW = 16.0f;
static constexpr float kSceneH = 9.0f;

// ============================================================================
// Input Handler
// ============================================================================

class QuadViewportInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_SPACE)
            m_spacePressed = true;
        if (key == KEY_1)
            m_toggleQuadrant = 1;
        if (key == KEY_2)
            m_toggleQuadrant = 2;
        if (key == KEY_3)
            m_toggleQuadrant = 3;
        if (key == KEY_4)
            m_toggleQuadrant = 4;
        if (key == KEY_R)
            m_resetPressed = true;
    }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

    int consumeToggle() {
        int v = m_toggleQuadrant;
        m_toggleQuadrant = 0;
        return v;
    }

    bool consumeReset() {
        bool v = m_resetPressed;
        m_resetPressed = false;
        return v;
    }

  private:
    bool m_spacePressed = false;
    int m_toggleQuadrant = 0;
    bool m_resetPressed = false;
};

// ============================================================================
// Space Scene (Top-Left) — rotating planets around a sun
// ============================================================================

class SpaceScene : public Scene {
  public:
    void onEnter() override {
        auto* cam = new Camera2D(kSceneW, kSceneH);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);
        setLightBox(new SimpleColorLightBox(Color::white()));
        setBackgroundColor(Color::fromHex(0x050510));

        // Sun
        auto sun = addEntity<SpriteEntity>();
        sun->setPosition(0.0f, 0.0f, 0.1f);
        sun->setColor(Color::fromHex(0xffcc00));
        sun->setScale(0.8f, 0.8f, 1.0f);
        sun->setName("Sun");

        struct PlanetDef {
            const char* name;
            uint32_t color;
            float radius;
            float speed;
            float size;
        };
        PlanetDef planets[] = {
            {"Planet1", 0xff4444, 2.0f, 1.2f, 0.4f},  {"Planet2", 0x4488ff, 3.2f, 0.7f, 0.35f},
            {"Planet3", 0x44ff88, 4.2f, 0.45f, 0.5f}, {"Planet4", 0xff88ff, 1.4f, 2.0f, 0.25f},
            {"Planet5", 0xffaa44, 5.0f, 0.3f, 0.3f},
        };
        for (auto& def : planets) {
            auto p = addEntity<SpriteEntity>();
            p->setPosition(def.radius, 0.0f, 0.2f);
            p->setColor(Color::fromHex(def.color));
            p->setScale(def.size, def.size, 1.0f);
            p->setName(def.name);
        }

        // Stars
        for (int i = 0; i < 25; ++i) {
            auto star = addEntity<SpriteEntity>();
            float sx = (static_cast<float>((i * 37 + 13) % 140) / 10.0f - 7.0f);
            float sy = (static_cast<float>((i * 53 + 7) % 80) / 10.0f - 4.0f);
            star->setPosition(sx, sy, 0.0f);
            float b = 0.5f + (i % 5) * 0.1f;
            star->setColor(Color(b, b, b + 0.1f));
            float sz = 0.06f + (i % 3) * 0.03f;
            star->setScale(sz, sz, 1.0f);
            star->setName("Star_" + std::to_string(i));
        }
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_time += deltaTime;

        struct Orbit {
            float radius;
            float speed;
        };
        Orbit orbits[] = {{2.0f, 1.2f}, {3.2f, 0.7f}, {4.2f, 0.45f}, {1.4f, 2.0f}, {5.0f, 0.3f}};
        for (int i = 0; i < 5; ++i) {
            auto* p = getEntityByName("Planet" + std::to_string(i + 1));
            if (p) {
                float angle = m_time * orbits[i].speed + i * 1.2f;
                p->setPosition(orbits[i].radius * std::cos(angle),
                               orbits[i].radius * std::sin(angle), 0.2f);
            }
        }
        // Twinkle stars
        for (int i = 0; i < 25; ++i) {
            auto* star = asSprite(getEntityByName("Star_" + std::to_string(i)));
            if (star) {
                float tw = 0.4f + 0.3f * std::sin(m_time * 3.0f + i * 0.7f);
                float sz = 0.06f + 0.04f * std::sin(m_time * 2.0f + i * 1.1f);
                star->setColor(Color(tw, tw, tw + 0.15f));
                star->setScale(sz, sz, 1.0f);
            }
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Forest Scene (Top-Right) — swaying trees with wind
// ============================================================================

class ForestScene : public Scene {
  public:
    void onEnter() override {
        auto* cam = new Camera2D(kSceneW, kSceneH);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);
        setLightBox(new SimpleColorLightBox(Color::white()));
        setBackgroundColor(Color::fromHex(0x1a3a1a));

        const Color treeColors[] = {
            Color::fromHex(0x228b22), Color::fromHex(0x2e8b57), Color::fromHex(0x006400),
            Color::fromHex(0x32cd32), Color::fromHex(0x3cb371),
        };

        // Ground
        auto ground = addEntity<SpriteEntity>();
        ground->setPosition(0.0f, -3.5f, 0.0f);
        ground->setColor(Color::fromHex(0x2d5a1e));
        ground->setScale(kSceneW - 0.3f, 1.5f, 1.0f);
        ground->setName("Ground");

        for (int i = 0; i < 18; ++i) {
            float tx = (static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f);
            float ty = (static_cast<float>((i * 29 + 11) % 60) / 10.0f - 3.0f);
            float trunkH = 0.6f + (i % 4) * 0.2f;

            auto trunk = addEntity<SpriteEntity>();
            trunk->setPosition(tx, ty, 0.05f);
            trunk->setColor(Color::fromHex(0x8b4513));
            trunk->setScale(0.15f, trunkH, 1.0f);
            trunk->setName("Trunk_" + std::to_string(i));

            auto canopy = addEntity<SpriteEntity>();
            float sz = 0.5f + (i % 5) * 0.15f;
            canopy->setPosition(tx, ty + trunkH * 0.5f + sz * 0.3f, 0.1f);
            canopy->setColor(treeColors[i % 5]);
            canopy->setScale(sz, sz * 1.2f, 1.0f);
            canopy->setName("Canopy_" + std::to_string(i));
        }

        const Color flowerColors[] = {
            Color::fromHex(0xff69b4),
            Color::fromHex(0xff4500),
            Color::fromHex(0xffd700),
            Color::fromHex(0xda70d6),
        };
        for (int i = 0; i < 8; ++i) {
            auto flower = addEntity<SpriteEntity>();
            float fx = (static_cast<float>((i * 67 + 23) % 130) / 10.0f - 6.5f);
            float fy = -2.0f + (static_cast<float>((i * 43 + 17) % 20) / 10.0f);
            flower->setPosition(fx, fy, 0.15f);
            flower->setColor(flowerColors[i % 4]);
            flower->setScale(0.15f, 0.15f, 1.0f);
            flower->setName("Flower_" + std::to_string(i));
        }
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_time += deltaTime;

        for (int i = 0; i < 18; ++i) {
            auto* canopy = getEntityByName("Canopy_" + std::to_string(i));
            if (canopy) {
                auto pos = canopy->getPosition();
                float baseX = (static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f);
                float sway = 0.08f * std::sin(m_time * 1.5f + baseX * 0.8f + i * 0.4f);
                canopy->setPosition(baseX + sway, pos.y, pos.z);
            }
        }
        for (int i = 0; i < 8; ++i) {
            auto* flower = getEntityByName("Flower_" + std::to_string(i));
            if (flower) {
                float bob = 0.03f * std::sin(m_time * 2.0f + i * 1.3f);
                flower->setScale(0.15f + bob, 0.15f + bob, 1.0f);
            }
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// City Scene (Bottom-Left) — skyline with pulsing lights
// ============================================================================

class CityScene : public Scene {
  public:
    void onEnter() override {
        auto* cam = new Camera2D(kSceneW, kSceneH);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);
        setLightBox(new SimpleColorLightBox(Color::white()));
        setBackgroundColor(Color::fromHex(0x252530));

        const Color buildingColors[] = {
            Color::fromHex(0x555566), Color::fromHex(0x666677), Color::fromHex(0x444455),
            Color::fromHex(0x777788), Color::fromHex(0x888899),
        };

        auto road = addEntity<SpriteEntity>();
        road->setPosition(0.0f, -3.5f, 0.0f);
        road->setColor(Color::fromHex(0x333333));
        road->setScale(kSceneW - 0.3f, 1.5f, 1.0f);

        for (int i = 0; i < 6; ++i) {
            auto mark = addEntity<SpriteEntity>();
            mark->setPosition(-5.5f + i * 2.2f, -3.5f, 0.05f);
            mark->setColor(Color::fromHex(0xcccc44));
            mark->setScale(0.8f, 0.06f, 1.0f);
        }

        for (int i = 0; i < 12; ++i) {
            float bx = -6.5f + i * 1.1f;
            float bHeight = 1.5f + static_cast<float>((i * 7 + 3) % 5) * 0.8f;
            float by = -2.8f + bHeight * 0.5f;

            auto building = addEntity<SpriteEntity>();
            building->setPosition(bx, by, 0.1f);
            building->setColor(buildingColors[i % 5]);
            building->setScale(0.9f, bHeight, 1.0f);
            building->setName("Building_" + std::to_string(i));

            int nw = static_cast<int>(bHeight / 0.5f);
            for (int w = 0; w < std::min(nw, 6); ++w) {
                float wy = by - bHeight * 0.4f + w * 0.55f;
                auto w1 = addEntity<SpriteEntity>();
                w1->setPosition(bx - 0.15f, wy, 0.15f);
                w1->setColor(Color::fromHex(0xffee88));
                w1->setScale(0.12f, 0.12f, 1.0f);
                w1->setName("Win_" + std::to_string(i) + "_" + std::to_string(w));

                auto w2 = addEntity<SpriteEntity>();
                w2->setPosition(bx + 0.15f, wy, 0.15f);
                w2->setColor(Color::fromHex(0xffee88));
                w2->setScale(0.12f, 0.12f, 1.0f);
                w2->setName("Win2_" + std::to_string(i) + "_" + std::to_string(w));
            }
        }

        auto moon = addEntity<SpriteEntity>();
        moon->setPosition(5.5f, 3.0f, 0.05f);
        moon->setColor(Color::fromHex(0xeeeedd));
        moon->setScale(0.7f, 0.7f, 1.0f);
        moon->setName("Moon");
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_time += deltaTime;

        for (int i = 0; i < 12; ++i) {
            float bHeight = 1.5f + static_cast<float>((i * 7 + 3) % 5) * 0.8f;
            int nw = static_cast<int>(bHeight / 0.5f);
            for (int w = 0; w < std::min(nw, 6); ++w) {
                auto* w1 =
                    asSprite(getEntityByName("Win_" + std::to_string(i) + "_" + std::to_string(w)));
                auto* w2 = asSprite(
                    getEntityByName("Win2_" + std::to_string(i) + "_" + std::to_string(w)));
                if (w1) {
                    float f = 0.5f + 0.5f * std::sin(m_time * 2.5f + i * 1.3f + w * 0.9f);
                    w1->setColor(Color(0.6f + 0.4f * f, 0.55f + 0.35f * f, 0.3f + 0.2f * f));
                }
                if (w2) {
                    float f = 0.5f + 0.5f * std::sin(m_time * 2.1f + i * 0.8f + w * 1.4f);
                    w2->setColor(Color(0.6f + 0.4f * f, 0.55f + 0.35f * f, 0.3f + 0.2f * f));
                }
            }

            auto* b = asSprite(getEntityByName("Building_" + std::to_string(i)));
            if (b) {
                float pulse = 0.03f * std::sin(m_time * 1.5f + i * 0.5f);
                uint32_t base[] = {0x555566, 0x666677, 0x444455, 0x777788, 0x888899};
                Color c = Color::fromHex(base[i % 5]);
                b->setColor(Color(std::clamp(c.r + pulse, 0.0f, 1.0f),
                                  std::clamp(c.g + pulse, 0.0f, 1.0f),
                                  std::clamp(c.b + pulse * 1.5f, 0.0f, 1.0f)));
            }
        }

        auto* moon = asSprite(getEntityByName("Moon"));
        if (moon) {
            float g = 0.93f + 0.07f * std::sin(m_time * 0.5f);
            moon->setColor(Color(g, g, g * 0.95f));
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Ocean Scene (Bottom-Right) — animated waves and a boat
// ============================================================================

class OceanScene : public Scene {
  public:
    void onEnter() override {
        auto* cam = new Camera2D(kSceneW, kSceneH);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);
        setLightBox(new SimpleColorLightBox(Color::white()));
        setBackgroundColor(Color::fromHex(0x0a1628));

        for (int row = -3; row <= 3; ++row) {
            for (int col = -5; col <= 5; ++col) {
                auto wave = addEntity<SpriteEntity>();
                float wx = col * 1.3f;
                float wy = row * 1.2f;
                wave->setPosition(wx, wy, 0.0f);
                float t = (row + 3) / 6.0f;
                wave->setColor(Color(0.05f + t * 0.2f, 0.1f + t * 0.3f, 0.3f + t * 0.5f));
                wave->setScale(1.1f, 0.3f, 1.0f);
                wave->setName("Wave_" + std::to_string(row + 3) + "_" + std::to_string(col + 5));
            }
        }

        auto hull = addEntity<SpriteEntity>();
        hull->setPosition(0.0f, 0.0f, 0.3f);
        hull->setColor(Color::fromHex(0x8b4513));
        hull->setScale(1.0f, 0.3f, 1.0f);
        hull->setName("Hull");

        auto mast = addEntity<SpriteEntity>();
        mast->setPosition(0.0f, 0.4f, 0.35f);
        mast->setColor(Color::fromHex(0xdddddd));
        mast->setScale(0.06f, 0.7f, 1.0f);
        mast->setName("Mast");

        auto sail = addEntity<SpriteEntity>();
        sail->setPosition(0.2f, 0.5f, 0.32f);
        sail->setColor(Color::fromHex(0xffffee));
        sail->setScale(0.5f, 0.5f, 1.0f);
        sail->setName("Sail");

        for (int i = 0; i < 4; ++i) {
            auto gull = addEntity<SpriteEntity>();
            float gx = -3.0f + i * 2.5f;
            float gy = 2.5f + (i % 2) * 0.5f;
            gull->setPosition(gx, gy, 0.4f);
            gull->setColor(Color::fromHex(0xcccccc));
            gull->setScale(0.2f, 0.06f, 1.0f);
            gull->setName("Gull_" + std::to_string(i));
        }

        auto oceanSun = addEntity<SpriteEntity>();
        oceanSun->setPosition(6.5f, 3.0f, 0.05f);
        oceanSun->setColor(Color::fromHex(0xffdd44));
        oceanSun->setScale(0.9f, 0.9f, 1.0f);
        oceanSun->setName("OceanSun");
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);
        m_time += deltaTime;

        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 11; ++col) {
                auto* wave =
                    getEntityByName("Wave_" + std::to_string(row) + "_" + std::to_string(col));
                if (wave) {
                    float bx = (col - 5) * 1.3f;
                    float by = (row - 3) * 1.2f;
                    float wo = 0.15f * std::sin(m_time * 2.0f + bx * 0.6f + by * 0.4f);
                    float sy = 0.3f + 0.1f * std::sin(m_time * 1.8f + bx * 0.8f + by * 0.3f);
                    wave->setPosition(bx + wo * 0.3f, by + wo, wave->getPosition().z);
                    wave->setScale(1.1f, sy, 1.0f);
                }
            }
        }

        auto* hull = getEntityByName("Hull");
        auto* mast = getEntityByName("Mast");
        auto* sail = getEntityByName("Sail");
        if (hull) {
            float bob = 0.08f * std::sin(m_time * 2.5f);
            float drift = 1.5f * std::sin(m_time * 0.15f);
            hull->setPosition(drift, bob, 0.3f);
            if (mast)
                mast->setPosition(drift, 0.4f + bob, 0.35f);
            if (sail) {
                float flutter = 0.03f * std::sin(m_time * 4.0f);
                sail->setPosition(0.2f + drift + flutter, 0.5f + bob, 0.32f);
            }
        }

        for (int i = 0; i < 4; ++i) {
            auto* gull = getEntityByName("Gull_" + std::to_string(i));
            if (gull) {
                float bx = -3.0f + i * 2.5f;
                float by = 2.5f + (i % 2) * 0.5f;
                float gx = bx + 1.5f * std::sin(m_time * 0.4f + i * 1.5f);
                float gy = by + 0.3f * std::sin(m_time * 0.6f + i * 2.0f);
                gull->setPosition(gx, gy, 0.4f);
                float flapW = 0.2f + 0.05f * std::sin(m_time * 6.0f + i * 3.0f);
                gull->setScale(flapW, 0.06f, 1.0f);
            }
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Quad Viewport Game — uses SceneGroup with viewports
// ============================================================================

class QuadViewportDemo : public vde::Game {
  public:
    QuadViewportDemo() = default;
    ~QuadViewportDemo() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        cleanupImGui();
#endif
    }

    void onStart() override {
        // Set up input
        m_input = std::make_unique<QuadViewportInputHandler>();
        setInputHandler(m_input.get());

        // Create four scenes
        addScene("space", new SpaceScene());
        addScene("forest", new ForestScene());
        addScene("city", new CityScene());
        addScene("ocean", new OceanScene());

        // Activate all four with viewports
        auto group =
            SceneGroup::createWithViewports("quad", {
                                                        {"space", ViewportRect::topLeft()},
                                                        {"forest", ViewportRect::topRight()},
                                                        {"city", ViewportRect::bottomLeft()},
                                                        {"ocean", ViewportRect::bottomRight()},
                                                    });
        setActiveSceneGroup(group);

        // Default focus on space scene
        setFocusedScene("space");

        printHeader();

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
        ImGui::SetNextWindowSize(ImVec2(280, 140), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Info")) {
            ImGui::Text("FPS: %.1f", getFPS());
            ImGui::Text("Frame: %llu", getFrameCount());
            ImGui::Text("Delta: %.3f ms", getDeltaTime() * 1000.0f);
            ImGui::Text("DPI Scale: %.2f", getDPIScale());
            ImGui::Separator();
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
        if (m_failed)
            m_exitCode = 1;
    }

    void onUpdate(float /*deltaTime*/) override {
        if (!m_input)
            return;

        // Fail / quit keys
        if (m_input->isFailPressed()) {
            std::cerr << "\nTEST FAILED: User reported issue\n" << std::endl;
            m_failed = true;
            quit();
            return;
        }
        if (m_input->isEscapePressed()) {
            std::cout << "User requested early exit.\n";
            quit();
            return;
        }

        // Toggle individual quadrants (1-4)
        int toggle = m_input->consumeToggle();
        if (toggle >= 1 && toggle <= 4) {
            m_quadActive[toggle - 1] = !m_quadActive[toggle - 1];
            const char* names[] = {"Space", "Forest", "City", "Ocean"};
            std::cout << names[toggle - 1] << ": "
                      << (m_quadActive[toggle - 1] ? "RUNNING" : "PAUSED") << "\n";
            rebuildGroup();
        }

        // Reset
        if (m_input->consumeReset()) {
            for (auto& a : m_quadActive)
                a = true;
            rebuildGroup();
            std::cout << "Reset — all quadrants active\n";
        }

        // Status
        if (m_input->consumeSpace())
            printStatus();

        // Auto-terminate
        m_elapsed += getDeltaTime();
        if (m_elapsed >= 30.0f) {
            std::cout << "\nTEST PASSED: Demo completed (30s)\n";
            quit();
        }
    }

    int getExitCode() const { return m_exitCode; }

  private:
    std::unique_ptr<QuadViewportInputHandler> m_input;
    bool m_quadActive[4] = {true, true, true, true};
    float m_elapsed = 0.0f;
    int m_exitCode = 0;
    bool m_failed = false;

#ifdef VDE_EXAMPLE_USE_IMGUI
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;

    static VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
        VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}};
        VkDescriptorPoolCreateInfo poolInfo{};
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

    void rebuildGroup() {
        const char* names[] = {"space", "forest", "city", "ocean"};
        ViewportRect vps[] = {
            ViewportRect::topLeft(),
            ViewportRect::topRight(),
            ViewportRect::bottomLeft(),
            ViewportRect::bottomRight(),
        };

        std::vector<SceneGroupEntry> entries;
        for (int i = 0; i < 4; ++i) {
            if (m_quadActive[i])
                entries.push_back({names[i], vps[i]});
        }

        if (entries.empty()) {
            // At least show space
            entries.push_back({"space", ViewportRect::fullWindow()});
        }

        auto group = SceneGroup::createWithViewports("quad", entries);
        setActiveSceneGroup(group);
    }

    void printHeader() {
        std::cout << "\n========================================\n";
        std::cout << "  VDE Example: Quad Viewport (Split-Screen)\n";
        std::cout << "========================================\n\n";
        std::cout << "Features demonstrated:\n";
        std::cout << "  - Four independent scenes with per-scene viewports\n";
        std::cout << "  - Each scene has its own Camera2D\n";
        std::cout << "  - Multi-pass Vulkan rendering (one render pass per scene)\n";
        std::cout << "  - Per-quadrant pause/resume via SceneGroup rebuild\n\n";
        std::cout << "You should see:\n";
        std::cout << "  - Top-left (Space): Dark bg, yellow sun, orbiting colored planets, "
                     "twinkling stars\n";
        std::cout << "  - Top-right (Forest): Green bg, brown trunks with green canopies, colored "
                     "flowers\n";
        std::cout
            << "  - Bottom-left (City): Dark gray bg, building skyline, flickering windows, moon\n";
        std::cout
            << "  - Bottom-right (Ocean): Deep blue bg, undulating waves, brown boat, seagulls\n\n";
        std::cout << "Controls:\n";
        std::cout << "  1     - Toggle Space quadrant\n";
        std::cout << "  2     - Toggle Forest quadrant\n";
        std::cout << "  3     - Toggle City quadrant\n";
        std::cout << "  4     - Toggle Ocean quadrant\n";
        std::cout << "  R     - Reset all quadrants\n";
        std::cout << "  SPACE - Print status\n";
        std::cout << "  F     - Fail test\n";
        std::cout << "  ESC   - Exit early\n";
        std::cout << "  (Auto-closes in 30 seconds)\n\n";
    }

    void printStatus() {
        const char* names[] = {"Space (TL)", "Forest (TR)", "City (BL)", "Ocean (BR)"};
        std::cout << "\n--- Quad Viewport Status ---\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "  " << (i + 1) << ") " << names[i] << ": "
                      << (m_quadActive[i] ? "RUNNING" : "PAUSED") << "\n";
        }
        std::cout << "  Time: " << std::fixed << std::setprecision(1) << m_elapsed << "s\n";
        std::cout << "----------------------------\n\n";
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    QuadViewportDemo demo;

    vde::GameSettings settings;
    settings.gameName = "VDE Quad Viewport Demo";
    settings.display.windowWidth = 1280;
    settings.display.windowHeight = 720;
    settings.display.fullscreen = false;

    try {
        if (!demo.initialize(settings)) {
            std::cerr << "Failed to initialize demo!" << std::endl;
            return 1;
        }
        demo.run();
        return demo.getExitCode();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
