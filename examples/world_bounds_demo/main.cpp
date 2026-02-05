/**
 * @file main.cpp
 * @brief World Bounds Demo - demonstrates Phase 2.5 functionality
 *
 * This example demonstrates:
 * - Type-safe world units (Meters, Pixels)
 * - Cardinal direction-based world bounds
 * - CameraBounds2D for screen-to-world coordinate conversion
 * - Integrating world bounds with Scene
 */

#include <vde/api/CameraBounds.h>
#include <vde/api/GameAPI.h>
#include <vde/api/WorldBounds.h>
#include <vde/api/WorldUnits.h>

#include <cmath>
#include <iomanip>
#include <iostream>

using namespace vde;

// Configuration
constexpr float AUTO_TERMINATE_SECONDS = 15.0f;

/**
 * @brief Input handler that tracks mouse position
 */
class DemoInputHandler : public InputHandler {
  public:
    void onKeyPress(int key) override {
        if (key == KEY_ESCAPE)
            m_escapePressed = true;
        if (key == KEY_F)
            m_failPressed = true;
        if (key == KEY_SPACE)
            m_spacePressed = true;
        if (key == KEY_W)
            m_up = true;
        if (key == KEY_S)
            m_down = true;
        if (key == KEY_A)
            m_left = true;
        if (key == KEY_D)
            m_right = true;
        if (key == KEY_Q)
            m_zoomOut = true;
        if (key == KEY_E)
            m_zoomIn = true;
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
        if (key == KEY_Q)
            m_zoomOut = false;
        if (key == KEY_E)
            m_zoomIn = false;
    }

    void onMouseMove(double x, double y) override {
        m_mouseX = static_cast<float>(x);
        m_mouseY = static_cast<float>(y);
    }

    void onMouseButtonPress(int button, double x, double y) override {
        if (button == 0) {
            m_clicked = true;
        }
    }

    bool isEscapePressed() {
        bool v = m_escapePressed;
        m_escapePressed = false;
        return v;
    }
    bool isFailPressed() {
        bool v = m_failPressed;
        m_failPressed = false;
        return v;
    }
    bool isSpacePressed() {
        bool v = m_spacePressed;
        m_spacePressed = false;
        return v;
    }
    bool wasClicked() {
        bool v = m_clicked;
        m_clicked = false;
        return v;
    }

    float getMouseX() const { return m_mouseX; }
    float getMouseY() const { return m_mouseY; }

    bool isMovingUp() const { return m_up; }
    bool isMovingDown() const { return m_down; }
    bool isMovingLeft() const { return m_left; }
    bool isMovingRight() const { return m_right; }
    bool isZoomingIn() const { return m_zoomIn; }
    bool isZoomingOut() const { return m_zoomOut; }

  private:
    bool m_escapePressed = false;
    bool m_failPressed = false;
    bool m_spacePressed = false;
    bool m_clicked = false;
    bool m_up = false, m_down = false, m_left = false, m_right = false;
    bool m_zoomIn = false, m_zoomOut = false;
    float m_mouseX = 0.0f, m_mouseY = 0.0f;
};

/**
 * @brief A simple entity that represents a marker at a world position
 */
class WorldMarker : public SpriteEntity {
  public:
    WorldMarker(const std::string& label = "") : m_label(label) {}

    void setLabel(const std::string& label) { m_label = label; }
    const std::string& getLabel() const { return m_label; }

  private:
    std::string m_label;
};

/**
 * @brief Scene demonstrating world bounds and coordinate systems
 */
class WorldBoundsScene : public Scene {
  public:
    WorldBoundsScene() = default;

    void onEnter() override {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Example: World Bounds System" << std::endl;
        std::cout << "========================================\n" << std::endl;

        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "  - Type-safe world units (Meters)" << std::endl;
        std::cout << "  - Cardinal direction-based bounds" << std::endl;
        std::cout << "  - Screen-to-world coordinate mapping" << std::endl;
        std::cout << "  - CameraBounds2D for panning/zooming" << std::endl;

        std::cout << "\nYou should see:" << std::endl;
        std::cout << "  - Grid of colored markers" << std::endl;
        std::cout << "  - White center marker" << std::endl;
        std::cout << "  - Cardinal direction markers (N/S/E/W)" << std::endl;
        std::cout << "  - Dark blue background" << std::endl;

        std::cout << "\nControls:" << std::endl;
        std::cout << "  WASD   - Pan camera" << std::endl;
        std::cout << "  Q/E    - Zoom out/in" << std::endl;
        std::cout << "  Click  - Print world coordinates" << std::endl;
        std::cout << "  Space  - Toggle constraint bounds" << std::endl;
        std::cout << "  F      - Fail test (if visuals are incorrect)" << std::endl;
        std::cout << "  ESC    - Exit early" << std::endl;
        std::cout << "  (Auto-closes in " << AUTO_TERMINATE_SECONDS << " seconds)\n" << std::endl;

        // =========================================================
        // Demonstrate world bounds with cardinal directions
        // =========================================================

        // Create a 200m x 200m x 30m world using cardinal directions
        // This is more intuitive than raw min/max coordinates
        WorldBounds worldBounds = WorldBounds::fromDirectionalLimits(
            100_m, WorldBounds::south(100_m),  // north: +100m, south: -100m
            WorldBounds::west(100_m), 100_m,   // west: -100m, east: +100m
            20_m, WorldBounds::down(10_m)      // up: +20m, down: -10m
        );

        setWorldBounds(worldBounds);

        // =========================================================
        // Set up 2D camera with pixel-to-world mapping
        // =========================================================

        m_cameraBounds.setScreenSize(1280_px, 720_px);
        m_cameraBounds.setWorldWidth(40_m);  // Show 40 meters across screen
        m_cameraBounds.centerOn(0_m, 0_m);

        // Create constraint bounds (camera can't see outside this area)
        m_constraintBounds = WorldBounds2D::fromCenter(0_m, 0_m, 80_m, 80_m);

        // =========================================================
        // Use a 2D camera for rendering
        // =========================================================

        auto* camera = new Camera2D(40.0f, 22.5f);
        camera->setPosition(0.0f, 0.0f);
        setCamera(camera);

        // Background color
        setBackgroundColor(Color::fromHex(0x1a1a2e));

        // =========================================================
        // Create markers at cardinal positions
        // =========================================================

        // Center marker
        auto center = addEntity<WorldMarker>("Origin");
        center->setPosition(0.0f, 0.0f, 0.0f);
        center->setColor(Color::white());
        center->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(center);

        // North marker (at +Z in world, but +Y in our 2D view)
        auto north = addEntity<WorldMarker>("North");
        north->setPosition(0.0f, 20.0f, 0.0f);      // 20m north
        north->setColor(Color::fromHex(0x00ff88));  // Green
        north->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(north);

        // South marker
        auto south = addEntity<WorldMarker>("South");
        south->setPosition(0.0f, -20.0f, 0.0f);     // 20m south
        south->setColor(Color::fromHex(0xff8800));  // Orange
        south->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(south);

        // East marker
        auto east = addEntity<WorldMarker>("East");
        east->setPosition(20.0f, 0.0f, 0.0f);      // 20m east
        east->setColor(Color::fromHex(0x0088ff));  // Blue
        east->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(east);

        // West marker
        auto west = addEntity<WorldMarker>("West");
        west->setPosition(-20.0f, 0.0f, 0.0f);     // 20m west
        west->setColor(Color::fromHex(0xff0088));  // Magenta
        west->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(west);

        // Grid markers
        for (int x = -30; x <= 30; x += 10) {
            for (int y = -30; y <= 30; y += 10) {
                if (x == 0 && y == 0)
                    continue;  // Skip center
                if (std::abs(x) == 20 && y == 0)
                    continue;  // Skip E/W
                if (std::abs(y) == 20 && x == 0)
                    continue;  // Skip N/S

                auto marker = addEntity<WorldMarker>();
                marker->setPosition(static_cast<float>(x), static_cast<float>(y), 0.0f);
                marker->setColor(Color(0.3f, 0.3f, 0.4f, 0.5f));
                marker->setScale(0.2f, 0.2f, 1.0f);
            }
        }

        // Click marker (invisible initially)
        m_clickMarker = addEntity<WorldMarker>("Click");
        m_clickMarker->setColor(Color::yellow());
        m_clickMarker->setScale(0.3f, 0.3f, 1.0f);
        m_clickMarker->setVisible(false);

        m_elapsedTime = 0.0f;
    }

    void update(float deltaTime) override {
        m_elapsedTime += deltaTime;

        auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Check for fail key
        if (input->isFailPressed()) {
            std::cerr << "\n========================================" << std::endl;
            std::cerr << "  TEST FAILED: User reported issue" << std::endl;
            std::cerr << "  Expected: Grid of markers, camera panning/zooming" << std::endl;
            std::cerr << "========================================\n" << std::endl;
            m_testFailed = true;
            if (getGame())
                getGame()->quit();
            return;
        }

        // Check for escape key
        if (input->isEscapePressed()) {
            std::cout << "User requested early exit." << std::endl;
            if (getGame())
                getGame()->quit();
            return;
        }

        // Auto-terminate after configured time
        if (m_elapsedTime >= AUTO_TERMINATE_SECONDS) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "  TEST PASSED: Demo completed successfully" << std::endl;
            std::cout << "  Duration: " << m_elapsedTime << " seconds" << std::endl;
            std::cout << "========================================\n" << std::endl;
            if (getGame())
                getGame()->quit();
            return;
        }

        // Pan camera
        float panSpeed = 20.0f * deltaTime;
        Meters dx = 0_m, dy = 0_m;

        if (input->isMovingRight())
            dx = Meters(panSpeed);
        if (input->isMovingLeft())
            dx = Meters(-panSpeed);
        if (input->isMovingUp())
            dy = Meters(panSpeed);
        if (input->isMovingDown())
            dy = Meters(-panSpeed);

        if (dx.value != 0.0f || dy.value != 0.0f) {
            m_cameraBounds.move(dx, dy);

            // Update render camera to match
            auto* camera = dynamic_cast<Camera2D*>(getCamera());
            if (camera) {
                glm::vec2 center = m_cameraBounds.getCenter();
                camera->setPosition(center.x, center.y);
            }
        }

        // Zoom
        if (input->isZoomingIn()) {
            float newZoom = m_cameraBounds.getZoom() * (1.0f + deltaTime);
            m_cameraBounds.setZoom(newZoom);
        }
        if (input->isZoomingOut()) {
            float newZoom = m_cameraBounds.getZoom() * (1.0f - deltaTime);
            m_cameraBounds.setZoom(std::max(0.1f, newZoom));
        }

        // Toggle constraints
        if (input->isSpacePressed()) {
            m_constraintsEnabled = !m_constraintsEnabled;
            if (m_constraintsEnabled) {
                m_cameraBounds.setConstraintBounds(m_constraintBounds);
                std::cout << "Camera constraints ENABLED" << std::endl;
            } else {
                m_cameraBounds.clearConstraintBounds();
                std::cout << "Camera constraints DISABLED" << std::endl;
            }
        }

        // Handle click - convert screen coords to world coords
        if (input->wasClicked()) {
            Pixels mouseX(input->getMouseX());
            Pixels mouseY(input->getMouseY());

            glm::vec2 worldPos = m_cameraBounds.screenToWorld(mouseX, mouseY);

            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Click at screen (" << mouseX.value << ", " << mouseY.value << "px) "
                      << "-> world (" << worldPos.x << ", " << worldPos.y << "m)" << std::endl;

            // Show marker at click position
            m_clickMarker->setPosition(worldPos.x, worldPos.y, 0.1f);
            m_clickMarker->setVisible(true);

            // Check visibility
            if (m_cameraBounds.isVisible(Meters(worldPos.x), Meters(worldPos.y))) {
                std::cout << "  Point is within visible bounds" << std::endl;
            }
        }

        // Update marker visibility based on camera bounds
        for (auto& marker : m_markers) {
            Position pos = marker->getPosition();
            bool visible = m_cameraBounds.isVisible(Meters(pos.x), Meters(pos.y));
            // Markers should always be visible in this demo, but demonstrate the check
            // marker->setVisible(visible);
        }

        Scene::update(deltaTime);
    }

    bool didTestFail() const { return m_testFailed; }

  private:
    CameraBounds2D m_cameraBounds;
    WorldBounds2D m_constraintBounds;
    bool m_constraintsEnabled = false;

    std::vector<std::shared_ptr<WorldMarker>> m_markers;
    std::shared_ptr<WorldMarker> m_clickMarker;

    float m_elapsedTime = 0.0f;
    bool m_testFailed = false;
};

/**
 * @brief Game class for the demo.
 */
class WorldBoundsDemo : public Game {
  public:
    void onStart() override {
        // Set up input handler
        m_inputHandler = std::make_unique<DemoInputHandler>();
        setInputHandler(m_inputHandler.get());

        // Create scene
        auto* scene = new WorldBoundsScene();
        m_scenePtr = scene;
        addScene("main", scene);
        setActiveScene("main");
    }

    void onShutdown() override {
        if (m_scenePtr && m_scenePtr->didTestFail()) {
            m_exitCode = 1;
        }
    }

    int getExitCode() const { return m_exitCode; }

  private:
    std::unique_ptr<DemoInputHandler> m_inputHandler;
    WorldBoundsScene* m_scenePtr = nullptr;
    int m_exitCode = 0;
};

/**
 * @brief Main entry point
 */
int main() {
    WorldBoundsDemo demo;

    GameSettings settings;
    settings.gameName = "World Bounds Demo";
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
