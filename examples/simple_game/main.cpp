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

/**
 * @brief Custom input handler for the game.
 */
class GameInputHandler : public vde::InputHandler {
public:
    GameInputHandler() = default;
    
    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
        if (key == vde::KEY_W) m_moveForward = true;
        if (key == vde::KEY_S) m_moveBackward = true;
        if (key == vde::KEY_A) m_moveLeft = true;
        if (key == vde::KEY_D) m_moveRight = true;
    }
    
    void onKeyRelease(int key) override {
        if (key == vde::KEY_W) m_moveForward = false;
        if (key == vde::KEY_S) m_moveBackward = false;
        if (key == vde::KEY_A) m_moveLeft = false;
        if (key == vde::KEY_D) m_moveRight = false;
    }
    
    void onMouseMove(double x, double y) override {
        m_mouseX = x;
        m_mouseY = y;
    }
    
    void onMouseScroll(double /* xOffset */, double yOffset) override {
        m_scrollDelta = static_cast<float>(yOffset);
    }
    
    // Query methods
    bool isEscapePressed() { 
        bool val = m_escapePressed;
        m_escapePressed = false;
        return val;
    }
    
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
    bool m_escapePressed = false;
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
        if (rot.yaw > 360.0f) rot.yaw -= 360.0f;
        setRotation(rot);
    }
    
private:
    float m_rotationSpeed = 45.0f; // degrees per second
};

/**
 * @brief Main game scene with a rotating cube.
 */
class MainScene : public vde::Scene {
public:
    MainScene() = default;
    
    void onEnter() override {
        std::cout << "MainScene: onEnter" << std::endl;
        
        // Set up an orbit camera looking at the origin
        setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 5.0f, 20.0f, 45.0f));
        
        // LightBox is optional - if not set, default white ambient is used
        // Uncomment to customize: setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));
        
        // Set background color to dark blue
        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
        
        // Create a rotating cube entity
        m_cube = addEntity<RotatingCube>();
        m_cube->setName("MainCube");
        m_cube->setPosition(0.0f, 0.0f, 0.0f);
        m_cube->setColor(vde::Color::fromHex(0x4a90d9));
        m_cube->setRotationSpeed(30.0f);
        
        // Optionally set a mesh directly (built-in primitives don't require resource loading)
        // m_cube->setMesh(vde::Mesh::createCube(1.0f));
    }
    
    void onExit() override {
        std::cout << "MainScene: onExit" << std::endl;
    }
    
    void update(float deltaTime) override {
        // Handle input
        auto* input = dynamic_cast<GameInputHandler*>(getInputHandler());
        if (input) {
            // Check for quit
            if (input->isEscapePressed()) {
                getGame()->quit();
                return;
            }
            
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
                std::cout << "Rotation speed: " << (m_speedMultiplier == 1.0f ? "normal" : "fast") << std::endl;
            }
        }
        
        // Update all entities (including the cube)
        vde::Scene::update(deltaTime);
    }
    
    void render() override {
        // Let the base class handle entity rendering
        vde::Scene::render();
    }
    
private:
    std::shared_ptr<RotatingCube> m_cube;
    float m_speedMultiplier = 1.0f;
};

/**
 * @brief Menu scene shown at startup.
 */
class MenuScene : public vde::Scene {
public:
    MenuScene() = default;
    
    void onEnter() override {
        std::cout << "MenuScene: Press SPACE to start the game" << std::endl;
        setBackgroundColor(vde::Color::fromHex(0x0f0f23));
        // No camera or lightbox needed for menu - defaults are fine
    }
    
    void update(float /* deltaTime */) override {
        auto* input = dynamic_cast<GameInputHandler*>(getInputHandler());
        if (input) {
            if (input->isEscapePressed()) {
                getGame()->quit();
                return;
            }
            if (input->isSpacePressed()) {
                // Switch to main scene
                getGame()->setActiveScene("main");
            }
        }
    }
};

/**
 * @brief Main entry point.
 */
int main() {
    // Create game instance
    vde::Game game;
    
    // Configure game settings
    vde::GameSettings settings;
    settings.gameName = "VDE Simple Game Example";
    settings.setWindowSize(1280, 720);
    settings.display.resizable = true;
    settings.display.vsync = vde::VSyncMode::On;
    settings.graphics.quality = vde::GraphicsQuality::Medium;
    settings.debug.enableValidation = true;
    settings.debug.showFPS = true;
    
    // Initialize the game
    if (!game.initialize(settings)) {
        std::cerr << "Failed to initialize game!" << std::endl;
        return 1;
    }
    
    std::cout << "VDE Simple Game Example" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  SPACE - Start game / Toggle rotation speed" << std::endl;
    std::cout << "  SCROLL - Zoom camera in/out" << std::endl;
    std::cout << "  ESC - Quit" << std::endl;
    std::cout << std::endl;
    
    // Create input handler
    GameInputHandler inputHandler;
    game.setInputHandler(&inputHandler);
    
    // Add scenes
    game.addScene("menu", new MenuScene());
    game.addScene("main", new MainScene());
    
    // Start with menu scene
    game.setActiveScene("menu");
    
    // Run the game loop
    game.run();
    
    // Cleanup
    game.shutdown();
    
    std::cout << "Game ended. Goodbye!" << std::endl;
    return 0;
}
