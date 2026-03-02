/**
 * @file main.cpp
 * @brief Multi-Scene Demo - demonstrates scene management features
 *
 * This example demonstrates:
 * - Creating multiple scenes with different configurations
 * - Different world bounds per scene
 * - Different background colors per scene
 * - Different camera types (orbit 3D vs 2D) per scene
 * - Switching between scenes with number keys
 * - Scene stacking with pushScene/popScene
 * - Background scene updates (continueInBackground)
 * - Multi-scene groups via setActiveSceneGroup() (Phase 2)
 *   Press G to toggle dual-scene mode (Space + City simultaneously)
 */

#include <vde/api/GameAPI.h>
#include <vde/api/WorldBounds.h>
#include <vde/api/WorldUnits.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Forward declarations
// ============================================================================
class MultiSceneDemo;

// ============================================================================
// Input Handler
// ============================================================================

class MultiSceneInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == KEY_1)
            m_sceneSwitch = 1;
        if (key == KEY_2)
            m_sceneSwitch = 2;
        if (key == KEY_3)
            m_sceneSwitch = 3;
        if (key == KEY_4)
            m_sceneSwitch = 4;
        if (key == KEY_P)
            m_pushPressed = true;
        if (key == KEY_O)
            m_popPressed = true;
        if (key == KEY_B)
            m_toggleBackground = true;
        if (key == KEY_SPACE)
            m_spacePressed = true;
        if (key == KEY_TAB)
            m_tabPressed = true;
        if (key == KEY_G)
            m_groupPressed = true;
        if (key == KEY_V)
            m_viewportPressed = true;

        // Camera controls
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
        m_scrollDelta = static_cast<float>(yOffset);
    }

    // Consume methods (read and clear)
    int consumeSceneSwitch() {
        int v = m_sceneSwitch;
        m_sceneSwitch = 0;
        return v;
    }
    bool consumePush() {
        bool v = m_pushPressed;
        m_pushPressed = false;
        return v;
    }
    bool consumePop() {
        bool v = m_popPressed;
        m_popPressed = false;
        return v;
    }
    bool consumeToggleBackground() {
        bool v = m_toggleBackground;
        m_toggleBackground = false;
        return v;
    }
    bool consumeSpace() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }
    bool consumeTab() {
        bool v = m_tabPressed;
        m_tabPressed = false;
        return v;
    }
    bool consumeGroup() {
        bool v = m_groupPressed;
        m_groupPressed = false;
        return v;
    }
    bool consumeViewport() {
        bool v = m_viewportPressed;
        m_viewportPressed = false;
        return v;
    }
    float consumeScroll() {
        float v = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return v;
    }

    // Continuous state
    bool isUp() const { return m_up; }
    bool isDown() const { return m_down; }
    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }

  private:
    int m_sceneSwitch = 0;
    bool m_pushPressed = false;
    bool m_popPressed = false;
    bool m_toggleBackground = false;
    bool m_spacePressed = false;
    bool m_tabPressed = false;
    bool m_groupPressed = false;
    bool m_viewportPressed = false;
    float m_scrollDelta = 0.0f;
    bool m_up = false, m_down = false, m_left = false, m_right = false;
};

// ============================================================================
// Base class for demo scenes with background-simulation support
// ============================================================================

/**
 * @brief Extended scene base that tracks time-while-paused.
 *
 * Uses the engine's Scene::setContinueInBackground() so the scheduler
 * keeps updating this scene even when it's not the active/primary scene.
 * Also provides catch-up logic for cases where a scene was truly
 * suspended (e.g., pushed by another scene).
 */
class DemoScene : public vde::examples::BaseExampleScene {
  public:
    explicit DemoScene(const std::string& label, float autoTerminate = 120.0f)
        : BaseExampleScene(autoTerminate), m_label(label) {}

    // ------ Background simulation toggle ------

    /**
     * @brief When true, the engine scheduler will keep calling update()
     *        on this scene even when it's not in the active scene group.
     */
    void setContinueInBackground(bool enabled) {
        m_continueInBackground = enabled;
        // Also set the engine-level flag so the scheduler knows
        Scene::setContinueInBackground(enabled);
    }
    bool getContinueInBackground() const { return m_continueInBackground; }

    // ------ Lifecycle overrides ------

    void onPause() override {
        recordPauseTime();
        std::cout << "[" << m_label << "] paused"
                  << (m_continueInBackground ? " (will continue simulation)" : " (suspended)")
                  << std::endl;
    }

    void onResume() override {
        applyBackgroundTime();
        std::cout << "[" << m_label << "] resumed" << std::endl;
    }

    void onEnter() override {
        // Clear existing entities so subclass onEnter() doesn't duplicate them
        clearEntities();

        // If we were previously exited (via setActiveScene), apply catch-up time
        if (m_wasExited) {
            applyBackgroundTime();
            m_wasExited = false;
        }
        std::cout << "[" << m_label << "] entered"
                  << (m_accumulatedBackgroundTime > 0.01f
                          ? " (catching up " + formatTime(m_accumulatedBackgroundTime) + "s)"
                          : "")
                  << std::endl;
    }

    void onExit() override {
        recordPauseTime();
        m_wasExited = true;
        std::cout << "[" << m_label << "] exited"
                  << (m_continueInBackground ? " (simulation continues)" : " (suspended)")
                  << std::endl;
    }

    void update(float deltaTime) override {
        // If we have accumulated background time, drain it in capped steps
        // so physics doesn't explode from one giant delta.
        float effectiveDt = deltaTime;
        if (m_accumulatedBackgroundTime > 0.0f) {
            const float kMaxCatchupPerFrame = 0.5f;  // cap per-frame catch-up
            float catchup = std::min(m_accumulatedBackgroundTime, kMaxCatchupPerFrame);
            m_accumulatedBackgroundTime -= catchup;
            effectiveDt += catchup;
        }

        // Subclass-specific logic
        updateScene(effectiveDt);

        // Base handles ESC, F, auto-terminate
        BaseExampleScene::update(deltaTime);
    }

    const std::string& getLabel() const { return m_label; }

  protected:
    /**
     * @brief Override this instead of update() to receive the effective
     *        deltaTime that includes any catch-up time.
     */
    virtual void updateScene(float effectiveDt) = 0;

    double getCurrentGameTime() const {
        if (getGame())
            return getGame()->getTotalTime();
        return 0.0;
    }

  private:
    std::string m_label;
    bool m_continueInBackground = false;
    double m_pauseTimestamp = 0.0;
    float m_accumulatedBackgroundTime = 0.0f;
    bool m_wasExited = false;

    void recordPauseTime() { m_pauseTimestamp = getCurrentGameTime(); }

    void applyBackgroundTime() {
        if (m_continueInBackground && m_pauseTimestamp > 0.0) {
            double now = getCurrentGameTime();
            m_accumulatedBackgroundTime += static_cast<float>(now - m_pauseTimestamp);
        }
        m_pauseTimestamp = 0.0;
    }

    static std::string formatTime(float t) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << t;
        return oss.str();
    }
};

// ============================================================================
// Scene 1 – Space Scene  (3D orbit camera, dark background)
// ============================================================================

/**
 * @brief A rotating "planet" entity.
 */
class Planet : public MeshEntity {
  public:
    Planet() = default;

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

class SpaceScene : public DemoScene {
  public:
    SpaceScene() : DemoScene("Space") {}

    void onEnter() override {
        DemoScene::onEnter();

        // Small 50m world
        setWorldBounds(WorldBounds::fromDirectionalLimits(25_m, WorldBounds::south(25_m),
                                                          WorldBounds::west(25_m), 25_m, 25_m,
                                                          WorldBounds::down(25_m)));

        setBackgroundColor(Color::fromHex(0x050510));

        setCamera(new OrbitCamera(Position(0, 0, 0), 12.0f, 30.0f, 0.0f));
        setLightBox(new SimpleColorLightBox(Color(0.6f, 0.6f, 0.8f)));

        // Central "sun"
        auto sun = addEntity<MeshEntity>();
        sun->setName("Sun");
        sun->setMesh(Mesh::createCube(1.5f));
        sun->setColor(Color::fromHex(0xffcc00));
        sun->setPosition(0, 0, 0);

        // Orbiting planets
        auto p1 = addEntity<Planet>();
        p1->setName("RedPlanet");
        p1->setMesh(Mesh::createCube(0.6f));
        p1->setColor(Color::fromHex(0xff4444));
        p1->setOrbitRadius(3.5f);
        p1->setOrbitSpeed(0.8f);
        p1->setSelfRotationSpeed(90.0f);

        auto p2 = addEntity<Planet>();
        p2->setName("BluePlanet");
        p2->setMesh(Mesh::createCube(0.5f));
        p2->setColor(Color::fromHex(0x4488ff));
        p2->setOrbitRadius(6.0f);
        p2->setOrbitSpeed(0.4f);
        p2->setSelfRotationSpeed(120.0f);

        auto p3 = addEntity<Planet>();
        p3->setName("GreenPlanet");
        p3->setMesh(Mesh::createCube(0.8f));
        p3->setColor(Color::fromHex(0x44ff88));
        p3->setOrbitRadius(9.0f);
        p3->setOrbitSpeed(0.25f);
        p3->setSelfRotationSpeed(45.0f);
    }

    void updateScene(float dt) override {
        // Planets update themselves via Entity::update
        (void)dt;

        auto* input = dynamic_cast<MultiSceneInputHandler*>(getInputHandler());
        if (!input)
            return;

        auto* cam = dynamic_cast<OrbitCamera*>(getCamera());
        if (!cam)
            return;

        float scroll = input->consumeScroll();
        if (scroll != 0.0f) {
            cam->zoom(scroll * 0.5f);
        }
        // Gentle auto-rotation
        cam->rotate(0.0f, 8.0f * dt);
    }

  protected:
    std::string getExampleName() const override { return "Space Scene (3D)"; }

    std::vector<std::string> getFeatures() const override {
        return {"3D OrbitCamera", "Rotating planet entities", "Small 50m world bounds",
                "Dark space background"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Yellow cube 'sun' at center", "Red, blue, and green cubes orbiting the sun",
                "Very dark background (near-black)"};
    }

    std::vector<std::string> getControls() const override {
        return {"SCROLL - Zoom camera", "Camera auto-rotates"};
    }
};

// ============================================================================
// Scene 2 – Forest Scene  (2D camera, green background)
// ============================================================================

class ForestScene : public DemoScene {
  public:
    ForestScene() : DemoScene("Forest") {}

    void onEnter() override {
        DemoScene::onEnter();

        // Medium 100m x 100m flat world
        setWorldBounds(
            WorldBounds::flat(50_m, WorldBounds::south(50_m), WorldBounds::west(50_m), 50_m));

        setBackgroundColor(Color::fromHex(0x1a3a1a));

        auto* cam = new Camera2D(30.0f, 17.0f);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);

        setLightBox(new SimpleColorLightBox(Color(0.8f, 1.0f, 0.7f)));

        // Create "trees" as colored sprites at various positions
        const Color treeColors[] = {
            Color::fromHex(0x228b22),  // Forest green
            Color::fromHex(0x2e8b57),  // Sea green
            Color::fromHex(0x006400),  // Dark green
            Color::fromHex(0x32cd32),  // Lime green
        };

        for (int i = 0; i < 30; ++i) {
            auto tree = addEntity<SpriteEntity>();
            float x = static_cast<float>((i * 37 + 13) % 60) - 30.0f;
            float y = static_cast<float>((i * 53 + 7) % 40) - 20.0f;
            tree->setPosition(x, y, 0.0f);
            tree->setColor(treeColors[i % 4]);
            float size = 0.5f + (i % 5) * 0.2f;
            tree->setScale(size, size * 1.5f, 1.0f);
            tree->setName("Tree_" + std::to_string(i));
        }

        // Ground marker at origin
        auto origin = addEntity<SpriteEntity>();
        origin->setPosition(0, 0, 0.1f);
        origin->setColor(Color::fromHex(0xccaa44));
        origin->setScale(0.3f, 0.3f, 1.0f);
        origin->setName("Origin");

        m_camX = 0.0f;
        m_camY = 0.0f;
    }

    void updateScene(float dt) override {
        auto* input = dynamic_cast<MultiSceneInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Pan camera
        const float panSpeed = 15.0f;
        if (input->isRight())
            m_camX += panSpeed * dt;
        if (input->isLeft())
            m_camX -= panSpeed * dt;
        if (input->isUp())
            m_camY += panSpeed * dt;
        if (input->isDown())
            m_camY -= panSpeed * dt;

        auto* cam = dynamic_cast<Camera2D*>(getCamera());
        if (cam) {
            cam->setPosition(m_camX, m_camY);
        }

        // Gentle sway animation on trees
        m_swayTime += dt;
        auto& entities = getEntities();
        for (auto& e : entities) {
            if (e->getName().find("Tree_") == 0) {
                auto pos = e->getPosition();
                // Small horizontal sway
                float sway = 0.15f * std::sin(m_swayTime * 1.5f + pos.x * 0.5f);
                e->setPosition(pos.x + sway * dt, pos.y, pos.z);
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Forest Scene (2D)"; }

    std::vector<std::string> getFeatures() const override {
        return {"2D Camera", "Sprite entities as trees", "Medium 100m world bounds",
                "Dark green background", "Tree sway animation"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Colored rectangles representing trees on dark green background",
                "Yellow marker at origin", "Trees gently sway"};
    }

    std::vector<std::string> getControls() const override { return {"WASD - Pan camera"}; }

  private:
    float m_camX = 0.0f;
    float m_camY = 0.0f;
    float m_swayTime = 0.0f;
};

// ============================================================================
// Scene 3 – City Scene  (3D orbit camera, gray background)
// ============================================================================

class CityScene : public DemoScene {
  public:
    CityScene() : DemoScene("City") {}

    void onEnter() override {
        DemoScene::onEnter();

        // Large 500m world
        setWorldBounds(WorldBounds::fromDirectionalLimits(250_m, WorldBounds::south(250_m),
                                                          WorldBounds::west(250_m), 250_m, 100_m,
                                                          WorldBounds::down(10_m)));

        setBackgroundColor(Color::fromHex(0x404050));

        setCamera(new OrbitCamera(Position(0, 0, 0), 25.0f, 50.0f, 30.0f));
        setLightBox(new SimpleColorLightBox(Color(0.9f, 0.85f, 0.8f)));

        // Create a grid of "buildings"
        const Color buildingColors[] = {
            Color::fromHex(0x888899), Color::fromHex(0x777788), Color::fromHex(0x666677),
            Color::fromHex(0x9999aa), Color::fromHex(0xaaaabb),
        };

        int idx = 0;
        for (int x = -3; x <= 3; ++x) {
            for (int z = -3; z <= 3; ++z) {
                if (x == 0 && z == 0)
                    continue;  // Leave center open
                float height = 1.0f + static_cast<float>((idx * 7 + 3) % 5);
                auto building = addEntity<MeshEntity>();
                building->setMesh(Mesh::createCube(1.0f));
                building->setPosition(x * 3.5f, height * 0.5f, z * 3.5f);
                building->setScale(1.5f, height, 1.5f);
                building->setColor(buildingColors[idx % 5]);
                building->setName("Building_" + std::to_string(idx));
                idx++;
            }
        }

        // Ground plane (flat cube)
        auto ground = addEntity<MeshEntity>();
        ground->setMesh(Mesh::createCube(1.0f));
        ground->setPosition(0, -0.05f, 0);
        ground->setScale(30.0f, 0.1f, 30.0f);
        ground->setColor(Color::fromHex(0x555560));
        ground->setName("Ground");

        m_buildingCount = idx;
    }

    void updateScene(float dt) override {
        auto* input = dynamic_cast<MultiSceneInputHandler*>(getInputHandler());
        if (!input)
            return;

        auto* cam = dynamic_cast<OrbitCamera*>(getCamera());
        if (!cam)
            return;

        float scroll = input->consumeScroll();
        if (scroll != 0.0f) {
            cam->zoom(scroll * 0.8f);
        }

        // Orbit with WASD
        float rotSpeed = 40.0f;
        if (input->isLeft())
            cam->rotate(0.0f, -rotSpeed * dt);
        if (input->isRight())
            cam->rotate(0.0f, rotSpeed * dt);
        if (input->isUp())
            cam->rotate(-rotSpeed * dt * 0.5f, 0.0f);
        if (input->isDown())
            cam->rotate(rotSpeed * dt * 0.5f, 0.0f);

        // Pulse building colors over time
        m_colorTime += dt;
        auto& entities = getEntities();
        for (auto& e : entities) {
            if (e->getName().find("Building_") == 0) {
                auto* meshEnt = dynamic_cast<MeshEntity*>(e.get());
                if (meshEnt) {
                    float pulse =
                        0.05f * std::sin(m_colorTime * 2.0f + meshEnt->getPosition().x * 0.3f);
                    auto baseColor = meshEnt->getColor();
                    meshEnt->setColor(Color(std::clamp(baseColor.r + pulse, 0.0f, 1.0f),
                                            std::clamp(baseColor.g + pulse, 0.0f, 1.0f),
                                            std::clamp(baseColor.b + pulse * 1.5f, 0.0f, 1.0f)));
                }
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "City Scene (3D)"; }

    std::vector<std::string> getFeatures() const override {
        return {"3D OrbitCamera with manual control", "Grid of cube buildings",
                "Large 500m world bounds", "Gray cityscape background", "Pulsing building colors"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Grid of differently-sized gray cubes as buildings", "Flat ground plane",
                "Buildings subtly pulse", "Medium-gray background"};
    }

    std::vector<std::string> getControls() const override {
        return {"WASD   - Orbit camera", "SCROLL - Zoom"};
    }

  private:
    int m_buildingCount = 0;
    float m_colorTime = 0.0f;
};

// ============================================================================
// Scene 4 – Ocean Scene  (2D camera, blue background, animated waves)
// ============================================================================

class OceanScene : public DemoScene {
  public:
    OceanScene() : DemoScene("Ocean") {}

    void onEnter() override {
        DemoScene::onEnter();

        // Medium-large 200m flat world
        setWorldBounds(
            WorldBounds::flat(100_m, WorldBounds::south(100_m), WorldBounds::west(100_m), 100_m));

        setBackgroundColor(Color::fromHex(0x0a1628));

        auto* cam = new Camera2D(40.0f, 22.5f);
        cam->setPosition(0.0f, 0.0f);
        setCamera(cam);

        setLightBox(new SimpleColorLightBox(Color(0.5f, 0.6f, 0.9f)));

        // Create wave rows
        for (int row = -5; row <= 5; ++row) {
            for (int col = -10; col <= 10; ++col) {
                auto wave = addEntity<SpriteEntity>();
                float x = col * 2.0f;
                float y = row * 3.0f;
                wave->setPosition(x, y, 0.0f);

                // Gradient from dark to light blue
                float t = (row + 5) / 10.0f;
                wave->setColor(Color(0.1f + t * 0.3f, 0.2f + t * 0.4f, 0.5f + t * 0.5f));
                wave->setScale(1.8f, 0.4f, 1.0f);
                wave->setName("Wave_" + std::to_string(row) + "_" + std::to_string(col));
            }
        }

        // "Boat" entity
        auto boat = addEntity<SpriteEntity>();
        boat->setPosition(0.0f, 0.0f, 0.2f);
        boat->setColor(Color::fromHex(0x8b4513));
        boat->setScale(1.2f, 0.6f, 1.0f);
        boat->setName("Boat");

        m_waveTime = 0.0f;
    }

    void updateScene(float dt) override {
        m_waveTime += dt;

        auto* input = dynamic_cast<MultiSceneInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Move boat
        auto* boat = getEntityByName("Boat");
        if (boat) {
            const float boatSpeed = 8.0f;
            auto pos = boat->getPosition();
            if (input->isRight())
                pos.x += boatSpeed * dt;
            if (input->isLeft())
                pos.x -= boatSpeed * dt;
            if (input->isUp())
                pos.y += boatSpeed * dt;
            if (input->isDown())
                pos.y -= boatSpeed * dt;
            // Bob up and down
            pos.z = 0.2f + 0.1f * std::sin(m_waveTime * 3.0f);
            boat->setPosition(pos);

            // Camera follows boat
            auto* cam = dynamic_cast<Camera2D*>(getCamera());
            if (cam)
                cam->setPosition(pos.x, pos.y);
        }

        // Animate waves
        auto& entities = getEntities();
        for (auto& e : entities) {
            if (e->getName().find("Wave_") == 0) {
                auto pos = e->getPosition();
                float wave = 0.3f * std::sin(m_waveTime * 2.0f + pos.x * 0.5f + pos.y * 0.3f);
                // Vertical oscillation
                e->setScale(1.8f, 0.4f + 0.15f * std::sin(m_waveTime * 1.5f + pos.x * 0.7f), 1.0f);
                e->setPosition(pos.x + wave * dt, pos.y, pos.z);
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Ocean Scene (2D)"; }

    std::vector<std::string> getFeatures() const override {
        return {"2D Camera following boat", "Animated wave sprites", "200m world bounds",
                "Deep blue background", "Boat entity with controls"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Rows of blue rectangles as waves, oscillating",
                "Brown rectangle 'boat' in the center", "Very dark blue background"};
    }

    std::vector<std::string> getControls() const override {
        return {"WASD - Move boat (camera follows)"};
    }

  private:
    float m_waveTime = 0.0f;
};

// ============================================================================
// HUD Scene – Pushed on top to show scene status info
// ============================================================================

class HUDScene : public vde::Scene {
  public:
    HUDScene() = default;

    void onEnter() override {
        setBackgroundColor(Color(0, 0, 0, 0));  // Transparent (engine may not support alpha clear)

        auto label = addEntity<SpriteEntity>();
        label->setPosition(0, 8.0f, 0.5f);
        label->setColor(Color::fromHex(0xffffff));
        label->setScale(12.0f, 1.0f, 1.0f);
        label->setName("HUDBar");

        setCamera(new Camera2D(30.0f, 17.0f));
        setLightBox(new SimpleColorLightBox(Color::white()));
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        m_displayTime += deltaTime;
        // Auto-pop after 3 seconds
        if (m_displayTime >= 3.0f) {
            if (getGame())
                getGame()->popScene();
        }
    }

  private:
    float m_displayTime = 0.0f;
};

// ============================================================================
// Game class
// ============================================================================

class MultiSceneDemo : public vde::Game {
  public:
    void onStart() override {
        m_inputHandler = std::make_unique<MultiSceneInputHandler>();
        setInputHandler(m_inputHandler.get());

        // Create scenes
        auto* space = new SpaceScene();
        auto* forest = new ForestScene();
        auto* city = new CityScene();
        auto* ocean = new OceanScene();

        m_demoScenes[0] = space;
        m_demoScenes[1] = forest;
        m_demoScenes[2] = city;
        m_demoScenes[3] = ocean;

        addScene("space", space);
        addScene("forest", forest);
        addScene("city", city);
        addScene("ocean", ocean);
        addScene("hud", new HUDScene());

        // Default: Ocean and City continue in background
        ocean->setContinueInBackground(true);
        city->setContinueInBackground(true);

        m_activeIndex = 0;
        setActiveScene("space");

        printMasterHeader();
    }

    void onUpdate(float /*deltaTime*/) override {
        auto* input = m_inputHandler.get();
        if (!input)
            return;

        // --- Scene switching with number keys ---
        int sw = input->consumeSceneSwitch();
        if (sw >= 1 && sw <= 4) {
            int idx = sw - 1;
            if (idx != m_activeIndex) {
                m_activeIndex = idx;
                setActiveScene(sceneNames[idx]);
                std::cout << "\n>> Switched to: " << m_demoScenes[idx]->getLabel()
                          << " (background="
                          << (m_demoScenes[idx]->getContinueInBackground() ? "ON" : "OFF") << ")"
                          << std::endl;
            }
        }

        // --- Tab: cycle to next scene ---
        if (input->consumeTab()) {
            m_activeIndex = (m_activeIndex + 1) % 4;
            setActiveScene(sceneNames[m_activeIndex]);
            std::cout << "\n>> Cycled to: " << m_demoScenes[m_activeIndex]->getLabel() << std::endl;
        }

        // --- P: push HUD overlay onto current scene ---
        if (input->consumePush()) {
            std::cout << "\n>> Pushing HUD overlay onto " << m_demoScenes[m_activeIndex]->getLabel()
                      << std::endl;
            pushScene("hud");
        }

        // --- O: pop scene ---
        if (input->consumePop()) {
            std::cout << "\n>> Popping scene stack" << std::endl;
            popScene();
        }

        // --- B: toggle background simulation for current scene ---
        if (input->consumeToggleBackground()) {
            auto* scene = m_demoScenes[m_activeIndex];
            bool newVal = !scene->getContinueInBackground();
            scene->setContinueInBackground(newVal);
            std::cout << "\n>> " << scene->getLabel()
                      << " background simulation: " << (newVal ? "ON" : "OFF") << std::endl;
        }

        // --- G: toggle scene group mode (Space + City rendered together) ---
        if (input->consumeGroup()) {
            m_groupMode = !m_groupMode;
            m_viewportMode = false;
            if (m_groupMode) {
                auto group = vde::SceneGroup::create("dual", {"space", "city"});
                setActiveSceneGroup(group);
                std::cout << "\n>> SCENE GROUP MODE: Space + City rendering simultaneously"
                          << std::endl;
                std::cout << "   (Space is primary camera/background, City entities overlay)"
                          << std::endl;
            } else {
                // Return to single-scene mode
                m_activeIndex = 0;
                setActiveScene("space");
                std::cout << "\n>> SINGLE SCENE MODE: Switched back to Space only" << std::endl;
            }
        }

        // --- V: toggle viewport split mode (Space left, City right) ---
        if (input->consumeViewport()) {
            m_viewportMode = !m_viewportMode;
            m_groupMode = false;
            if (m_viewportMode) {
                auto group = vde::SceneGroup::createWithViewports(
                    "split", {
                                 {"space", vde::ViewportRect::leftHalf()},
                                 {"city", vde::ViewportRect::rightHalf()},
                             });
                setActiveSceneGroup(group);
                std::cout << "\n>> VIEWPORT MODE: Space (left) + City (right) in split-screen"
                          << std::endl;
            } else {
                m_activeIndex = 0;
                setActiveScene("space");
                std::cout << "\n>> SINGLE SCENE MODE: Switched back to Space only" << std::endl;
            }
        }

        // --- SPACE: print status ---
        if (input->consumeSpace()) {
            printStatus();
        }
    }

    void onShutdown() override {
        // Check if any scene reported failure
        for (int i = 0; i < 4; ++i) {
            if (m_demoScenes[i] && m_demoScenes[i]->didTestFail()) {
                m_exitCode = 1;
                return;
            }
        }
    }

    int getExitCode() const { return m_exitCode; }

  private:
    void printMasterHeader() {
        std::cout << "\n================================================================"
                  << std::endl;
        std::cout << "  VDE Multi-Scene Demo" << std::endl;
        std::cout << "================================================================\n"
                  << std::endl;
        std::cout << "This demo creates 4 scenes with different configurations:" << std::endl;
        std::cout << "  1) Space  - 3D orbit camera, dark background, 50m world, orbiting planets"
                  << std::endl;
        std::cout << "  2) Forest - 2D camera, green background, 100m world, swaying trees"
                  << std::endl;
        std::cout << "  3) City   - 3D orbit camera, gray background, 500m world, pulsing buildings"
                  << std::endl;
        std::cout << "  4) Ocean  - 2D camera, blue background, 200m world, animated waves"
                  << std::endl;

        std::cout << "\nBackground simulation (continues physics while scene is inactive):"
                  << std::endl;
        std::cout << "  City  = ON  (buildings keep pulsing while away)" << std::endl;
        std::cout << "  Ocean = ON  (waves keep moving while away)" << std::endl;
        std::cout << "  Space = OFF (planets pause when you leave)" << std::endl;
        std::cout << "  Forest= OFF (trees pause when you leave)" << std::endl;

        std::cout << "\nMulti-Scene Group (Phase 2):" << std::endl;
        std::cout << "  G     - Toggle dual-scene group (Space + City rendered together)"
                  << std::endl;
        std::cout << "          Space is the primary scene (camera/background)" << std::endl;
        std::cout << "          City entities are rendered as overlay" << std::endl;

        std::cout << "\nSplit-Screen Viewports (Phase 3):" << std::endl;
        std::cout << "  V     - Toggle viewport mode (Space left, City right)" << std::endl;
        std::cout << "          Each scene has its own camera and viewport" << std::endl;

        std::cout << "\nControls:" << std::endl;
        std::cout << "  1-4   - Switch to scene 1/2/3/4" << std::endl;
        std::cout << "  TAB   - Cycle to next scene" << std::endl;
        std::cout << "  G     - Toggle scene group mode (Space + City)" << std::endl;
        std::cout << "  V     - Toggle split-screen viewport mode" << std::endl;
        std::cout << "  P     - Push HUD overlay (tests pushScene)" << std::endl;
        std::cout << "  O     - Pop overlay (tests popScene)" << std::endl;
        std::cout << "  B     - Toggle background simulation for current scene" << std::endl;
        std::cout << "  SPACE - Print status of all scenes" << std::endl;
        std::cout << "  WASD  - Camera/movement controls (per scene)" << std::endl;
        std::cout << "  SCROLL- Zoom (3D scenes)" << std::endl;
        std::cout << "  F     - Report test failure" << std::endl;
        std::cout << "  ESC   - Exit" << std::endl;
        std::cout << "  (Auto-terminates after 120 seconds)\n" << std::endl;
    }

    void printStatus() {
        std::cout << "\n--- Scene Status ---" << std::endl;
        std::string modeStr = "SINGLE";
        if (m_groupMode)
            modeStr = "GROUP (Space + City)";
        if (m_viewportMode)
            modeStr = "VIEWPORT (Space | City)";
        std::cout << "  Mode: " << modeStr << std::endl;
        const auto& group = getActiveSceneGroup();
        std::cout << "  Active group: \"" << group.name << "\" [";
        for (size_t i = 0; i < group.sceneNames.size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << group.sceneNames[i];
        }
        std::cout << "]" << std::endl;
        for (int i = 0; i < 4; ++i) {
            const char* active = (sceneNames[i] == getActiveScene()->getName()) ? " [PRIMARY]" : "";
            // Check if scene is in the active group
            bool inGroup = false;
            for (const auto& gn : group.sceneNames) {
                if (gn == sceneNames[i]) {
                    inGroup = true;
                    break;
                }
            }
            std::cout << "  " << (i + 1) << ") " << m_demoScenes[i]->getLabel() << " | background="
                      << (m_demoScenes[i]->getContinueInBackground() ? "ON " : "OFF")
                      << " | bounds=" << m_demoScenes[i]->getWorldBounds().width().value << "m wide"
                      << (inGroup ? " [IN GROUP]" : "") << active << std::endl;
        }
        std::cout << "--------------------\n" << std::endl;
    }

    static constexpr const char* sceneNames[] = {"space", "forest", "city", "ocean"};

    std::unique_ptr<MultiSceneInputHandler> m_inputHandler;
    DemoScene* m_demoScenes[4] = {};
    int m_activeIndex = 0;
    int m_exitCode = 0;
    bool m_groupMode = false;
    bool m_viewportMode = false;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    MultiSceneDemo demo;
    return vde::examples::runExample(demo, "VDE Multi-Scene Demo", 1280, 720, argc, argv);
}
