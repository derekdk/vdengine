/**
 * @file main.cpp
 * @brief Four Scene 3D Demo - Four simultaneous 3D scenes with focus indicator
 *
 * This example demonstrates rendering four independent 3D scenes simultaneously,
 * each with its own OrbitCamera and MeshEntity content.  The focused scene
 * (which receives keyboard/mouse input for camera manipulation) is indicated
 * by a bright blue border frame on its ground plane and a blue-tinted background.
 *
 *   +------------------+------------------+
 *   |   Top-Left:      |   Top-Right:     |
 *   |   CRYSTAL        |   METROPOLIS     |
 *   |   GARDEN         |   (city grid     |
 *   |   (gemstones)    |    of buildings) |
 *   +------------------+------------------+
 *   |   Bottom-Left:   |   Bottom-Right:  |
 *   |   NATURE         |   COSMOS         |
 *   |   PARK           |   (orbiting      |
 *   |   (trees)        |    planets)      |
 *   +------------------+------------------+
 *
 * Controls:
 *   TAB   - Cycle focus to the next scene
 *   1-4   - Focus a specific scene
 *   WASD  - Orbit the camera in the focused scene
 *   SCROLL- Zoom in/out in the focused scene
 *   F     - Report test failure
 *   ESC   - Exit early
 */

#include <vde/api/GameAPI.h>
#include <vde/api/WorldBounds.h>
#include <vde/api/WorldUnits.h>

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
// Constants
// ============================================================================

/// Half-extent of each scene's ground arena (total = 2 * kArenaHalf).
static constexpr float kArenaHalf = 8.0f;

/// Bright blue used for the focus border frame.
static const Color kBorderColor = Color::fromHex(0x4488ff);

// ============================================================================
// Input Handler
// ============================================================================

class FourScene3DInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_TAB)
            m_tabPressed = true;
        if (key == KEY_1)
            m_directFocus = 1;
        if (key == KEY_2)
            m_directFocus = 2;
        if (key == KEY_3)
            m_directFocus = 3;
        if (key == KEY_4)
            m_directFocus = 4;
        if (key == KEY_SPACE)
            m_spacePressed = true;

        // Camera orbit (continuous)
        if (key == KEY_W)
            m_up = true;
        if (key == KEY_S)
            m_down = true;
        if (key == KEY_A)
            m_left = true;
        if (key == KEY_D)
            m_right = true;
    }

    void onKeyRelease(int key) override {
        if (key == KEY_W)
            m_up = false;
        if (key == KEY_S)
            m_down = false;
        if (key == KEY_A)
            m_left = false;
        if (key == KEY_D)
            m_right = false;
    }

    void onMouseScroll(double /*xOffset*/, double yOffset) override {
        m_scrollDelta += static_cast<float>(yOffset);
    }

    // --- Consume (read-and-clear) methods ---

    bool consumeTab() {
        bool v = m_tabPressed;
        m_tabPressed = false;
        return v;
    }

    int consumeDirectFocus() {
        int v = m_directFocus;
        m_directFocus = 0;
        return v;
    }

    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }

    float consumeScroll() {
        float v = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return v;
    }

    // --- Continuous state ---

    bool isUp() const { return m_up; }
    bool isDown() const { return m_down; }
    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }

  private:
    bool m_tabPressed = false;
    int m_directFocus = 0;
    bool m_spacePressed = false;
    float m_scrollDelta = 0.0f;
    bool m_up = false;
    bool m_down = false;
    bool m_left = false;
    bool m_right = false;
};

// ============================================================================
// Focusable3DScene — base class with border frame and camera input
// ============================================================================

/**
 * @brief Base class for each of the four 3D scenes.
 *
 * Provides:
 * - A ground-plane border frame (4 thin cubes) toggled by focus state.
 * - Background colour switching between focused (blue) and unfocused.
 * - Camera orbit/zoom routing for the focused scene.
 */
class Focusable3DScene : public Scene {
  public:
    virtual ~Focusable3DScene() = default;

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        bool focused = getGame() && getGame()->getFocusedScene() == this;

        // Update border visibility & background colour
        if (focused != m_wasFocused) {
            setBorderVisible(focused);
            setBackgroundColor(focused ? getFocusedBg() : getUnfocusedBg());
            m_wasFocused = focused;
        }

        // Process camera input only when focused
        if (focused) {
            processOrbitalInput(deltaTime);
        }

        // Subclass animation
        animateContent(deltaTime);
    }

  protected:
    // --- Subclass customisation points ---

    /// Background colour when this scene is NOT focused.
    virtual Color getUnfocusedBg() const = 0;

    /// Background colour when this scene IS focused (a blue tint).
    virtual Color getFocusedBg() const = 0;

    /// Override to animate scene-specific content each frame.
    virtual void animateContent(float deltaTime) { (void)deltaTime; }

    // --- Helpers available to subclasses ---

    /**
     * @brief Create the four ground-level border cubes.
     *
     * Call this at the end of onEnter() after content is set up.
     * The border starts hidden and is shown when the scene receives focus.
     */
    void createBorderFrame() {
        auto mesh = Mesh::createCube(1.0f);
        const float height = 0.4f;
        const float thickness = 0.25f;
        const float halfExt = kArenaHalf;
        const float extent = halfExt * 2 + thickness;

        struct BorderDef {
            const char* name;
            float px, pz;
            float sx, sz;
        };
        BorderDef borders[] = {
            {"Border_N", 0.0f, halfExt, extent, thickness},
            {"Border_S", 0.0f, -halfExt, extent, thickness},
            {"Border_E", halfExt, 0.0f, thickness, extent},
            {"Border_W", -halfExt, 0.0f, thickness, extent},
        };

        for (auto& b : borders) {
            auto entity = addEntity<MeshEntity>();
            entity->setMesh(mesh);
            entity->setPosition(b.px, height * 0.5f, b.pz);
            entity->setScale(b.sx, height, b.sz);
            entity->setColor(kBorderColor);
            entity->setName(b.name);
            entity->setVisible(false);
        }
    }

    /**
     * @brief Create a standard ground plane for the scene.
     */
    void createGroundPlane(const Color& color, float size = kArenaHalf * 2.0f) {
        auto ground = addEntity<MeshEntity>();
        ground->setMesh(Mesh::createCube(1.0f));
        ground->setPosition(0, -0.05f, 0);
        ground->setScale(size, 0.1f, size);
        ground->setColor(color);
        ground->setName("Ground");
    }

  private:
    bool m_wasFocused = false;

    void setBorderVisible(bool visible) {
        const char* names[] = {"Border_N", "Border_S", "Border_E", "Border_W"};
        for (auto& name : names) {
            auto* e = getEntityByName(name);
            if (e)
                e->setVisible(visible);
        }
    }

    void processOrbitalInput(float dt) {
        auto* input = dynamic_cast<FourScene3DInputHandler*>(getInputHandler());
        if (!input)
            return;

        auto* cam = dynamic_cast<OrbitCamera*>(getCamera());
        if (!cam)
            return;

        // Orbit with WASD
        const float rotSpeed = 50.0f;
        if (input->isLeft())
            cam->rotate(0.0f, -rotSpeed * dt);
        if (input->isRight())
            cam->rotate(0.0f, rotSpeed * dt);
        if (input->isUp())
            cam->rotate(-rotSpeed * dt * 0.5f, 0.0f);
        if (input->isDown())
            cam->rotate(rotSpeed * dt * 0.5f, 0.0f);

        // Zoom with scroll
        float scroll = input->consumeScroll();
        if (scroll != 0.0f) {
            cam->zoom(scroll * 0.6f);
        }
    }
};

// ============================================================================
// Crystal Garden Scene (Top-Left)
// ============================================================================

/**
 * @brief Rotating cubes, pyramids, and spheres arranged in a garden layout.
 */
class CrystalScene : public Focusable3DScene {
  public:
    void onEnter() override {
        setWorldBounds(WorldBounds::fromDirectionalLimits(25_m, WorldBounds::south(25_m),
                                                          WorldBounds::west(25_m), 25_m, 25_m,
                                                          WorldBounds::down(25_m)));

        setBackgroundColor(getUnfocusedBg());
        setCamera(new OrbitCamera(Position(0, 0, 0), 18.0f, 35.0f, 20.0f));
        setLightBox(new SimpleColorLightBox(Color(1.0f, 0.9f, 0.8f)));

        createGroundPlane(Color::fromHex(0x2a1a2a));

        // Central pedestal pyramid
        auto pedestal = addEntity<MeshEntity>();
        pedestal->setMesh(Mesh::createPyramid(3.0f, 2.0f));
        pedestal->setPosition(0, 1.0f, 0);
        pedestal->setColor(Color::fromHex(0xccaa44));
        pedestal->setName("Pedestal");

        // Top jewel (sphere)
        auto jewel = addEntity<MeshEntity>();
        jewel->setMesh(Mesh::createSphere(0.6f, 16, 12));
        jewel->setPosition(0, 2.8f, 0);
        jewel->setColor(Color::fromHex(0xff3366));
        jewel->setName("TopJewel");

        // Ring of rotating gemstones
        const uint32_t gemColors[] = {0xff4444, 0x44ff44, 0x4444ff, 0xff44ff, 0x44ffff, 0xffff44};
        for (int i = 0; i < 6; ++i) {
            float angle = static_cast<float>(i) * 3.14159f * 2.0f / 6.0f;
            float r = 5.0f;
            float x = r * std::cos(angle);
            float z = r * std::sin(angle);

            auto gem = addEntity<MeshEntity>();
            gem->setMesh(Mesh::createCube(0.8f));
            gem->setPosition(x, 1.0f, z);
            gem->setColor(Color::fromHex(gemColors[i]));
            gem->setName("Gem_" + std::to_string(i));
        }

        // Outer pillars (cylinders)
        for (int i = 0; i < 4; ++i) {
            float angle = static_cast<float>(i) * 3.14159f * 0.5f + 0.4f;
            float r = 7.0f;

            auto pillar = addEntity<MeshEntity>();
            pillar->setMesh(Mesh::createCylinder(0.3f, 2.5f, 12));
            pillar->setPosition(r * std::cos(angle), 1.25f, r * std::sin(angle));
            pillar->setColor(Color::fromHex(0xaa8866));
            pillar->setName("Pillar_" + std::to_string(i));

            // Small sphere on top
            auto orb = addEntity<MeshEntity>();
            orb->setMesh(Mesh::createSphere(0.25f, 12, 8));
            orb->setPosition(r * std::cos(angle), 2.7f, r * std::sin(angle));
            orb->setColor(Color::fromHex(0x88ccff));
            orb->setName("Orb_" + std::to_string(i));
        }

        createBorderFrame();
    }

  protected:
    Color getUnfocusedBg() const override { return Color::fromHex(0x1a0a1a); }
    Color getFocusedBg() const override { return Color::fromHex(0x0a1540); }

    void animateContent(float dt) override {
        m_time += dt;

        // Rotate gemstones around center
        for (int i = 0; i < 6; ++i) {
            auto* gem = getEntityByName("Gem_" + std::to_string(i));
            if (!gem)
                continue;

            float baseAngle = static_cast<float>(i) * 3.14159f * 2.0f / 6.0f;
            float angle = baseAngle + m_time * 0.4f;
            float r = 5.0f;
            gem->setPosition(r * std::cos(angle), 1.0f + 0.3f * std::sin(m_time * 2.0f + i),
                             r * std::sin(angle));

            auto rot = gem->getRotation();
            rot.yaw += 60.0f * dt;
            if (rot.yaw > 360.0f)
                rot.yaw -= 360.0f;
            gem->setRotation(rot);
        }

        // Bob the top jewel
        auto* jewel = getEntityByName("TopJewel");
        if (jewel) {
            jewel->setPosition(0, 2.8f + 0.2f * std::sin(m_time * 1.5f), 0);
            auto rot = jewel->getRotation();
            rot.yaw += 30.0f * dt;
            if (rot.yaw > 360.0f)
                rot.yaw -= 360.0f;
            jewel->setRotation(rot);
        }

        // Glow orbs on pillars
        for (int i = 0; i < 4; ++i) {
            auto* orb = dynamic_cast<MeshEntity*>(getEntityByName("Orb_" + std::to_string(i)));
            if (orb) {
                float g = 0.6f + 0.4f * std::sin(m_time * 3.0f + i * 1.5f);
                orb->setColor(Color(0.4f * g, 0.7f * g, 1.0f * g));
            }
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Metropolis Scene (Top-Right)
// ============================================================================

/**
 * @brief A city grid of cube buildings with pulsing colours.
 */
class MetropolisScene : public Focusable3DScene {
  public:
    void onEnter() override {
        setWorldBounds(WorldBounds::fromDirectionalLimits(50_m, WorldBounds::south(50_m),
                                                          WorldBounds::west(50_m), 50_m, 50_m,
                                                          WorldBounds::down(10_m)));

        setBackgroundColor(getUnfocusedBg());
        setCamera(new OrbitCamera(Position(0, 0, 0), 22.0f, 50.0f, 30.0f));
        setLightBox(new SimpleColorLightBox(Color(0.9f, 0.85f, 0.8f)));

        createGroundPlane(Color::fromHex(0x333340), 18.0f);

        const uint32_t buildingHexes[] = {0x667788, 0x778899, 0x556677, 0x889aab, 0x99aabb};

        int idx = 0;
        for (int x = -3; x <= 3; ++x) {
            for (int z = -3; z <= 3; ++z) {
                if (x == 0 && z == 0)
                    continue;

                float height = 1.0f + static_cast<float>((idx * 7 + 3) % 5);
                auto building = addEntity<MeshEntity>();
                building->setMesh(Mesh::createCube(1.0f));
                building->setPosition(x * 2.2f, height * 0.5f, z * 2.2f);
                building->setScale(1.0f, height, 1.0f);
                building->setColor(Color::fromHex(buildingHexes[idx % 5]));
                building->setName("Bldg_" + std::to_string(idx));
                ++idx;
            }
        }
        m_buildingCount = idx;

        // Central tall tower (cylinder)
        auto tower = addEntity<MeshEntity>();
        tower->setMesh(Mesh::createCylinder(0.6f, 7.0f, 16));
        tower->setPosition(0, 3.5f, 0);
        tower->setColor(Color::fromHex(0xddddee));
        tower->setName("Tower");

        // Antenna sphere on tower
        auto antenna = addEntity<MeshEntity>();
        antenna->setMesh(Mesh::createSphere(0.3f, 12, 8));
        antenna->setPosition(0, 7.3f, 0);
        antenna->setColor(Color::fromHex(0xff4444));
        antenna->setName("Antenna");

        createBorderFrame();
    }

  protected:
    Color getUnfocusedBg() const override { return Color::fromHex(0x151520); }
    Color getFocusedBg() const override { return Color::fromHex(0x0a1535); }

    void animateContent(float dt) override {
        m_time += dt;

        // Pulse building colours
        for (int i = 0; i < m_buildingCount; ++i) {
            auto* b = dynamic_cast<MeshEntity*>(getEntityByName("Bldg_" + std::to_string(i)));
            if (!b)
                continue;

            float pulse = 0.04f * std::sin(m_time * 2.0f + b->getPosition().x * 0.5f +
                                           b->getPosition().z * 0.3f);
            const uint32_t baseHex[] = {0x667788, 0x778899, 0x556677, 0x889aab, 0x99aabb};
            Color c = Color::fromHex(baseHex[i % 5]);
            b->setColor(Color(std::clamp(c.r + pulse, 0.0f, 1.0f),
                              std::clamp(c.g + pulse, 0.0f, 1.0f),
                              std::clamp(c.b + pulse * 1.5f, 0.0f, 1.0f)));
        }

        // Blink antenna
        auto* antenna = dynamic_cast<MeshEntity*>(getEntityByName("Antenna"));
        if (antenna) {
            float blink = 0.5f + 0.5f * std::sin(m_time * 4.0f);
            antenna->setColor(Color(1.0f * blink, 0.2f * blink, 0.2f * blink));
        }
    }

  private:
    float m_time = 0.0f;
    int m_buildingCount = 0;
};

// ============================================================================
// Nature Park Scene (Bottom-Left)
// ============================================================================

/**
 * @brief Trees (cylinders + spheres) and bushes in a green park.
 */
class NatureScene : public Focusable3DScene {
  public:
    void onEnter() override {
        setWorldBounds(WorldBounds::fromDirectionalLimits(25_m, WorldBounds::south(25_m),
                                                          WorldBounds::west(25_m), 25_m, 15_m,
                                                          WorldBounds::down(5_m)));

        setBackgroundColor(getUnfocusedBg());
        setCamera(new OrbitCamera(Position(0, 0, 0), 20.0f, 40.0f, 10.0f));
        setLightBox(new SimpleColorLightBox(Color(0.8f, 1.0f, 0.7f)));

        createGroundPlane(Color::fromHex(0x2d5a1e));

        const uint32_t canopyColors[] = {0x228b22, 0x2e8b57, 0x006400, 0x32cd32, 0x3cb371};

        // Trees
        for (int i = 0; i < 14; ++i) {
            float tx = static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f;
            float tz = static_cast<float>((i * 29 + 11) % 130) / 10.0f - 6.5f;
            float trunkH = 1.5f + (i % 3) * 0.5f;

            auto trunk = addEntity<MeshEntity>();
            trunk->setMesh(Mesh::createCylinder(0.12f, trunkH, 8));
            trunk->setPosition(tx, trunkH * 0.5f, tz);
            trunk->setColor(Color::fromHex(0x8b4513));
            trunk->setName("Trunk_" + std::to_string(i));

            float canopyR = 0.5f + (i % 4) * 0.15f;
            auto canopy = addEntity<MeshEntity>();
            canopy->setMesh(Mesh::createSphere(canopyR, 12, 8));
            canopy->setPosition(tx, trunkH + canopyR * 0.5f, tz);
            canopy->setColor(Color::fromHex(canopyColors[i % 5]));
            canopy->setName("Canopy_" + std::to_string(i));
        }

        // Bushes (small spheres)
        for (int i = 0; i < 10; ++i) {
            float bx = static_cast<float>((i * 53 + 17) % 140) / 10.0f - 7.0f;
            float bz = static_cast<float>((i * 37 + 23) % 140) / 10.0f - 7.0f;

            auto bush = addEntity<MeshEntity>();
            bush->setMesh(Mesh::createSphere(0.3f, 10, 6));
            bush->setPosition(bx, 0.3f, bz);
            bush->setColor(Color::fromHex(canopyColors[(i + 2) % 5]));
            bush->setName("Bush_" + std::to_string(i));
        }

        // A small pond (flat blue cylinder)
        auto pond = addEntity<MeshEntity>();
        pond->setMesh(Mesh::createCylinder(1.5f, 0.05f, 24));
        pond->setPosition(2.0f, 0.03f, -2.0f);
        pond->setColor(Color::fromHex(0x3388cc));
        pond->setName("Pond");

        createBorderFrame();
    }

  protected:
    Color getUnfocusedBg() const override { return Color::fromHex(0x0a1a0a); }
    Color getFocusedBg() const override { return Color::fromHex(0x0a1530); }

    void animateContent(float dt) override {
        m_time += dt;

        // Gentle canopy sway
        for (int i = 0; i < 14; ++i) {
            auto* canopy = getEntityByName("Canopy_" + std::to_string(i));
            if (!canopy)
                continue;

            float baseX = static_cast<float>((i * 41 + 5) % 130) / 10.0f - 6.5f;
            float baseZ = static_cast<float>((i * 29 + 11) % 130) / 10.0f - 6.5f;
            float sway = 0.08f * std::sin(m_time * 1.5f + baseX * 0.8f + i * 0.4f);
            auto pos = canopy->getPosition();
            canopy->setPosition(baseX + sway, pos.y, baseZ);
        }

        // Bush breathing
        for (int i = 0; i < 10; ++i) {
            auto* bush = getEntityByName("Bush_" + std::to_string(i));
            if (!bush)
                continue;
            float s = 1.0f + 0.05f * std::sin(m_time * 2.0f + i * 1.3f);
            bush->setScale(s, s, s);
        }

        // Pond shimmer — subtle color oscillation
        auto* pond = dynamic_cast<MeshEntity*>(getEntityByName("Pond"));
        if (pond) {
            float g = 0.5f + 0.1f * std::sin(m_time * 2.5f);
            pond->setColor(Color(0.2f * g, 0.53f * g + 0.2f, 0.8f * g + 0.1f));
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Cosmos Scene (Bottom-Right)
// ============================================================================

/**
 * @brief A rotating "star" entity — cubes orbiting a central body.
 */
class PlanetEntity : public MeshEntity {
  public:
    PlanetEntity() = default;

    void setOrbitRadius(float r) { m_orbitRadius = r; }
    void setOrbitSpeed(float s) { m_orbitSpeed = s; }
    void setSelfRotationSpeed(float s) { m_selfRotSpeed = s; }

    void update(float deltaTime) override {
        m_angle += m_orbitSpeed * deltaTime;
        float x = m_orbitRadius * std::cos(m_angle);
        float z = m_orbitRadius * std::sin(m_angle);
        setPosition(x, 0.0f, z);

        auto rot = getRotation();
        rot.yaw += m_selfRotSpeed * deltaTime;
        if (rot.yaw > 360.0f)
            rot.yaw -= 360.0f;
        setRotation(rot);
    }

  private:
    float m_angle = 0.0f;
    float m_orbitRadius = 3.0f;
    float m_orbitSpeed = 0.5f;
    float m_selfRotSpeed = 60.0f;
};

/**
 * @brief Solar system — star at centre, orbiting planets.
 */
class CosmosScene : public Focusable3DScene {
  public:
    void onEnter() override {
        setWorldBounds(WorldBounds::fromDirectionalLimits(30_m, WorldBounds::south(30_m),
                                                          WorldBounds::west(30_m), 30_m, 30_m,
                                                          WorldBounds::down(30_m)));

        setBackgroundColor(getUnfocusedBg());
        setCamera(new OrbitCamera(Position(0, 0, 0), 18.0f, 30.0f, 0.0f));
        setLightBox(new SimpleColorLightBox(Color(0.6f, 0.6f, 0.8f)));

        // No ground plane — this is space!

        // Star (large bright sphere)
        auto star = addEntity<MeshEntity>();
        star->setMesh(Mesh::createSphere(1.2f, 24, 16));
        star->setPosition(0, 0, 0);
        star->setColor(Color::fromHex(0xffcc00));
        star->setName("Star");

        // Orbiting planets
        struct PlanetDef {
            const char* name;
            uint32_t color;
            float radius;
            float speed;
            float size;
        };
        PlanetDef planets[] = {
            {"Planet_0", 0xff4444, 3.0f, 0.9f, 0.35f},  {"Planet_1", 0x4488ff, 5.0f, 0.55f, 0.5f},
            {"Planet_2", 0x44ff88, 7.0f, 0.35f, 0.6f},  {"Planet_3", 0xff88ff, 9.0f, 0.2f, 0.4f},
            {"Planet_4", 0xffaa44, 11.0f, 0.12f, 0.3f},
        };

        for (auto& def : planets) {
            auto p = addEntity<PlanetEntity>();
            p->setMesh(Mesh::createSphere(def.size, 16, 12));
            p->setPosition(def.radius, 0.0f, 0.0f);
            p->setColor(Color::fromHex(def.color));
            p->setOrbitRadius(def.radius);
            p->setOrbitSpeed(def.speed);
            p->setSelfRotationSpeed(90.0f);
            p->setName(def.name);
        }

        // Distant "stars" (tiny cubes)
        for (int i = 0; i < 30; ++i) {
            auto s = addEntity<MeshEntity>();
            float sx = static_cast<float>((i * 37 + 13) % 240) / 10.0f - 12.0f;
            float sy = static_cast<float>((i * 53 + 7) % 200) / 10.0f - 10.0f;
            float sz = static_cast<float>((i * 71 + 3) % 240) / 10.0f - 12.0f;
            s->setMesh(Mesh::createCube(0.08f));
            s->setPosition(sx, sy, sz);
            float brightness = 0.4f + (i % 5) * 0.12f;
            s->setColor(Color(brightness, brightness, brightness + 0.1f));
            s->setName("BgStar_" + std::to_string(i));
        }

        createBorderFrame();
    }

  protected:
    Color getUnfocusedBg() const override { return Color::fromHex(0x050510); }
    Color getFocusedBg() const override { return Color::fromHex(0x050530); }

    void animateContent(float dt) override {
        m_time += dt;

        // Planets auto-update via PlanetEntity::update()

        // Pulse star colour
        auto* star = dynamic_cast<MeshEntity*>(getEntityByName("Star"));
        if (star) {
            float g = 0.85f + 0.15f * std::sin(m_time * 1.2f);
            star->setColor(Color(1.0f * g, 0.8f * g, 0.2f * g));
        }

        // Twinkle background stars
        for (int i = 0; i < 30; ++i) {
            auto* s = dynamic_cast<MeshEntity*>(getEntityByName("BgStar_" + std::to_string(i)));
            if (s) {
                float tw = 0.3f + 0.3f * std::sin(m_time * 3.0f + i * 0.9f);
                s->setColor(Color(tw, tw, tw + 0.08f));
            }
        }
    }

  private:
    float m_time = 0.0f;
};

// ============================================================================
// Four Scene 3D Demo — Game class
// ============================================================================

class FourScene3DDemo : public vde::Game {
  public:
    FourScene3DDemo() = default;
    ~FourScene3DDemo() override {
#ifdef VDE_EXAMPLE_USE_IMGUI
        cleanupImGui();
#endif
    }

    void onStart() override {
        m_input = std::make_unique<FourScene3DInputHandler>();
        setInputHandler(m_input.get());

        addScene("crystal", new CrystalScene());
        addScene("metropolis", new MetropolisScene());
        addScene("nature", new NatureScene());
        addScene("cosmos", new CosmosScene());

        auto group =
            SceneGroup::createWithViewports("quad", {
                                                        {"crystal", ViewportRect::topLeft()},
                                                        {"metropolis", ViewportRect::topRight()},
                                                        {"nature", ViewportRect::bottomLeft()},
                                                        {"cosmos", ViewportRect::bottomRight()},
                                                    });
        setActiveSceneGroup(group);

        // Default focus on crystal garden
        setFocusedScene("crystal");
        m_focusIndex = 0;

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
        ImGui::SetNextWindowSize(ImVec2(280, 160), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Info")) {
            ImGui::Text("FPS: %.1f", getFPS());
            ImGui::Text("Frame: %llu", getFrameCount());
            ImGui::Text("Delta: %.3f ms", getDeltaTime() * 1000.0f);
            ImGui::Text("DPI Scale: %.2f", getDPIScale());
            ImGui::Separator();
            ImGui::Text("Focused: %s", kDisplayNames[m_focusIndex]);
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

        // Tab — cycle focus
        if (m_input->consumeTab()) {
            m_focusIndex = (m_focusIndex + 1) % 4;
            setFocusedScene(kSceneNames[m_focusIndex]);
            std::cout << "Focus -> " << kDisplayNames[m_focusIndex] << "\n";
        }

        // Direct focus 1-4
        int direct = m_input->consumeDirectFocus();
        if (direct >= 1 && direct <= 4) {
            m_focusIndex = direct - 1;
            setFocusedScene(kSceneNames[m_focusIndex]);
            std::cout << "Focus -> " << kDisplayNames[m_focusIndex] << "\n";
        }

        // Status
        if (m_input->consumeSpace())
            printStatus();

        // Auto-terminate
        m_elapsed += getDeltaTime();
        if (m_elapsed >= 60.0f) {
            std::cout << "\nTEST PASSED: Demo completed (60s)\n";
            quit();
        }
    }

    int getExitCode() const { return m_exitCode; }

  private:
    std::unique_ptr<FourScene3DInputHandler> m_input;
    int m_focusIndex = 0;
    float m_elapsed = 0.0f;
    int m_exitCode = 0;
    bool m_failed = false;

    static constexpr const char* kSceneNames[] = {"crystal", "metropolis", "nature", "cosmos"};
    static constexpr const char* kDisplayNames[] = {"Crystal Garden (TL)", "Metropolis (TR)",
                                                    "Nature Park (BL)", "Cosmos (BR)"};

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
    void printHeader() {
        std::cout << "\n========================================\n";
        std::cout << "  VDE Example: Four Scene 3D Demo\n";
        std::cout << "========================================\n\n";
        std::cout << "Features demonstrated:\n";
        std::cout << "  - Four independent 3D scenes with OrbitCamera\n";
        std::cout << "  - Per-scene viewports (split-screen quad layout)\n";
        std::cout << "  - Focus indicator (blue border frame + blue background)\n";
        std::cout << "  - Camera orbit/zoom routed to focused scene only\n";
        std::cout << "  - MeshEntity with cubes, spheres, cylinders, pyramids\n\n";
        std::cout << "You should see:\n";
        std::cout << "  - Top-left (Crystal Garden): Rotating gemstones on pedestal, "
                     "pillars with orbs\n";
        std::cout << "  - Top-right (Metropolis): Grid of buildings with central tower, "
                     "pulsing colours\n";
        std::cout << "  - Bottom-left (Nature Park): Trees, bushes, and a pond on green ground\n";
        std::cout << "  - Bottom-right (Cosmos): Star with orbiting planets, twinkling stars\n";
        std::cout << "  - Focused scene has BLUE BORDER and blue-tinted background\n\n";
        std::cout << "Controls:\n";
        std::cout << "  TAB   - Cycle focus to next scene\n";
        std::cout << "  1-4   - Focus specific scene directly\n";
        std::cout << "  WASD  - Orbit camera (focused scene)\n";
        std::cout << "  SCROLL- Zoom in/out (focused scene)\n";
        std::cout << "  SPACE - Print status\n";
        std::cout << "  F     - Fail test\n";
        std::cout << "  ESC   - Exit early\n";
        std::cout << "  (Auto-closes in 60 seconds)\n\n";
    }

    void printStatus() {
        std::cout << "\n--- Four Scene 3D Status ---\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "  " << (i + 1) << ") " << kDisplayNames[i] << ": "
                      << (i == m_focusIndex ? "FOCUSED" : "running") << "\n";
        }
        std::cout << "  Time: " << std::fixed << std::setprecision(1) << m_elapsed << "s\n";
        std::cout << "----------------------------\n\n";
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    FourScene3DDemo demo;

    vde::GameSettings settings;
    settings.gameName = "VDE Four Scene 3D Demo";
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
