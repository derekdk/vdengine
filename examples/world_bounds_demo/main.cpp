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

#include <vde/api/GameAPI.h>
#include <vde/api/WorldUnits.h>
#include <vde/api/WorldBounds.h>
#include <vde/api/CameraBounds.h>
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace vde;

/**
 * @brief Input handler that tracks mouse position
 */
class DemoInputHandler : public InputHandler {
public:
    void onKeyPress(int key) override {
        if (key == KEY_ESCAPE) m_quit = true;
        if (key == KEY_SPACE) m_spacePressed = true;
        if (key == KEY_W) m_up = true;
        if (key == KEY_S) m_down = true;
        if (key == KEY_A) m_left = true;
        if (key == KEY_D) m_right = true;
        if (key == KEY_Q) m_zoomOut = true;
        if (key == KEY_E) m_zoomIn = true;
    }
    
    void onKeyRelease(int key) override {
        if (key == KEY_W) m_up = false;
        if (key == KEY_S) m_down = false;
        if (key == KEY_A) m_left = false;
        if (key == KEY_D) m_right = false;
        if (key == KEY_Q) m_zoomOut = false;
        if (key == KEY_E) m_zoomIn = false;
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
    
    bool shouldQuit() const { return m_quit; }
    bool isSpacePressed() { bool v = m_spacePressed; m_spacePressed = false; return v; }
    bool wasClicked() { bool v = m_clicked; m_clicked = false; return v; }
    
    float getMouseX() const { return m_mouseX; }
    float getMouseY() const { return m_mouseY; }
    
    bool isMovingUp() const { return m_up; }
    bool isMovingDown() const { return m_down; }
    bool isMovingLeft() const { return m_left; }
    bool isMovingRight() const { return m_right; }
    bool isZoomingIn() const { return m_zoomIn; }
    bool isZoomingOut() const { return m_zoomOut; }
    
private:
    bool m_quit = false;
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
        std::cout << "=== World Bounds Demo ===" << std::endl;
        std::cout << std::endl;
        std::cout << "This demo shows Phase 2.5 features:" << std::endl;
        std::cout << "  - Type-safe world units (Meters)" << std::endl;
        std::cout << "  - Cardinal direction-based bounds" << std::endl;
        std::cout << "  - Screen-to-world coordinate mapping" << std::endl;
        std::cout << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD: Pan camera" << std::endl;
        std::cout << "  Q/E: Zoom out/in" << std::endl;
        std::cout << "  Click: Print world coordinates" << std::endl;
        std::cout << "  Space: Toggle constraint bounds" << std::endl;
        std::cout << "  ESC: Quit" << std::endl;
        std::cout << std::endl;
        
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
        
        std::cout << "World Bounds:" << std::endl;
        std::cout << "  North limit: " << worldBounds.northLimit().value << "m" << std::endl;
        std::cout << "  South limit: " << worldBounds.southLimit().value << "m" << std::endl;
        std::cout << "  East limit:  " << worldBounds.eastLimit().value << "m" << std::endl;
        std::cout << "  West limit:  " << worldBounds.westLimit().value << "m" << std::endl;
        std::cout << "  Width:       " << worldBounds.width().value << "m" << std::endl;
        std::cout << "  Depth:       " << worldBounds.depth().value << "m" << std::endl;
        std::cout << "  Height:      " << worldBounds.height().value << "m" << std::endl;
        std::cout << std::endl;
        
        // =========================================================
        // Set up 2D camera with pixel-to-world mapping
        // =========================================================
        
        m_cameraBounds.setScreenSize(1280_px, 720_px);
        m_cameraBounds.setWorldWidth(40_m);  // Show 40 meters across screen
        m_cameraBounds.centerOn(0_m, 0_m);
        
        // Create constraint bounds (camera can't see outside this area)
        m_constraintBounds = WorldBounds2D::fromCenter(0_m, 0_m, 80_m, 80_m);
        
        std::cout << "Camera Setup:" << std::endl;
        std::cout << "  Visible width:  " << m_cameraBounds.getVisibleWidth().value << "m" << std::endl;
        std::cout << "  Visible height: " << m_cameraBounds.getVisibleHeight().value << "m" << std::endl;
        std::cout << std::endl;
        
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
        north->setPosition(0.0f, 20.0f, 0.0f);  // 20m north
        north->setColor(Color::fromHex(0x00ff88));  // Green
        north->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(north);
        
        // South marker
        auto south = addEntity<WorldMarker>("South");
        south->setPosition(0.0f, -20.0f, 0.0f);  // 20m south
        south->setColor(Color::fromHex(0xff8800));  // Orange
        south->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(south);
        
        // East marker
        auto east = addEntity<WorldMarker>("East");
        east->setPosition(20.0f, 0.0f, 0.0f);  // 20m east
        east->setColor(Color::fromHex(0x0088ff));  // Blue
        east->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(east);
        
        // West marker
        auto west = addEntity<WorldMarker>("West");
        west->setPosition(-20.0f, 0.0f, 0.0f);  // 20m west
        west->setColor(Color::fromHex(0xff0088));  // Magenta
        west->setScale(0.5f, 0.5f, 1.0f);
        m_markers.push_back(west);
        
        // Grid markers
        for (int x = -30; x <= 30; x += 10) {
            for (int y = -30; y <= 30; y += 10) {
                if (x == 0 && y == 0) continue;  // Skip center
                if (std::abs(x) == 20 && y == 0) continue;  // Skip E/W
                if (std::abs(y) == 20 && x == 0) continue;  // Skip N/S
                
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
    }
    
    void update(float deltaTime) override {
        auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
        if (!input) return;
        
        // Pan camera
        float panSpeed = 20.0f * deltaTime;
        Meters dx = 0_m, dy = 0_m;
        
        if (input->isMovingRight()) dx = Meters(panSpeed);
        if (input->isMovingLeft()) dx = Meters(-panSpeed);
        if (input->isMovingUp()) dy = Meters(panSpeed);
        if (input->isMovingDown()) dy = Meters(-panSpeed);
        
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
    
private:
    CameraBounds2D m_cameraBounds;
    WorldBounds2D m_constraintBounds;
    bool m_constraintsEnabled = false;
    
    std::vector<std::shared_ptr<WorldMarker>> m_markers;
    std::shared_ptr<WorldMarker> m_clickMarker;
};

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "VDE World Bounds Demo" << std::endl;
    std::cout << "=====================" << std::endl;
    std::cout << std::endl;
    
    // Demonstrate unit literals
    std::cout << "Unit literal examples:" << std::endl;
    Meters distance = 100_m;
    Meters halfDist = distance / 2.0f;
    std::cout << "  100_m / 2 = " << halfDist.value << " meters" << std::endl;
    
    Meters sum = 50_m + 25_m;
    std::cout << "  50_m + 25_m = " << sum.value << " meters" << std::endl;
    
    Pixels screenWidth = 1920_px;
    std::cout << "  Screen width: " << screenWidth.value << " pixels" << std::endl;
    std::cout << std::endl;
    
    // Demonstrate WorldPoint with directions
    WorldPoint pt = WorldPoint::fromDirections(100_m, 50_m, 20_m);
    std::cout << "WorldPoint from directions (100m N, 50m E, 20m up):" << std::endl;
    std::cout << "  X (east): " << pt.x.value << "m" << std::endl;
    std::cout << "  Y (up):   " << pt.y.value << "m" << std::endl;
    std::cout << "  Z (north): " << pt.z.value << "m" << std::endl;
    std::cout << std::endl;
    
    // Demonstrate PixelToWorldMapping
    PixelToWorldMapping mapping = PixelToWorldMapping::fitWidth(20_m, 1920_px);
    std::cout << "Mapping for 20m across 1920px:" << std::endl;
    std::cout << "  Pixels per meter: " << mapping.getPixelsPerMeter() << std::endl;
    std::cout << "  100px = " << mapping.toWorld(100_px).value << " meters" << std::endl;
    std::cout << "  5m = " << mapping.toPixels(5_m).value << " pixels" << std::endl;
    std::cout << std::endl;
    
    // Create and run the game
    Game game;
    DemoInputHandler inputHandler;
    
    GameSettings settings;
    settings.gameName = "World Bounds Demo";
    settings.setWindowSize(1280, 720);
    settings.display.resizable = true;
    
    try {
        game.initialize(settings);
        game.setInputHandler(&inputHandler);
        
        auto scene = std::make_unique<WorldBoundsScene>();
        scene->setInputHandler(&inputHandler);
        game.addScene("main", scene.release());
        game.setActiveScene("main");
        
        std::cout << "Starting game loop..." << std::endl;
        std::cout << std::endl;
        
        // Note: Game::run() is blocking, so quit handling is done via game.quit()
        // The ESC key handling in DemoInputHandler would need to call game.quit()
        game.run();
        
        game.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Demo complete!" << std::endl;
    return 0;
}
