/**
 * @file main.cpp
 * @brief Materials & Lighting Demo for VDE Phase 4
 * 
 * This example demonstrates Phase 4 features:
 * - Material system (albedo, roughness, metallic, emission)
 * - Multiple light types (directional, point, spot)
 * - ThreePointLightBox preset
 * - Material factory methods
 * 
 * The demo auto-terminates after a configured time.
 * Press 'F' to fail the test and report to command line.
 * Press 'ESCAPE' to exit early.
 */

#include <vde/api/GameAPI.h>
#include <iostream>
#include <cmath>

// Configuration
constexpr float AUTO_TERMINATE_SECONDS = 5.0f;  // Auto-close after this many seconds

/**
 * @brief Input handler for the demo.
 */
class DemoInputHandler : public vde::InputHandler {
public:
    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_F) {
            m_failPressed = true;
        }
    }
    
    bool isEscapePressed() {
        bool val = m_escapePressed;
        m_escapePressed = false;
        return val;
    }
    
    bool isFailPressed() {
        bool val = m_failPressed;
        m_failPressed = false;
        return val;
    }

private:
    bool m_escapePressed = false;
    bool m_failPressed = false;
};

/**
 * @brief A cube that rotates over time and uses materials.
 */
class MaterialCube : public vde::MeshEntity {
public:
    MaterialCube() = default;
    
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }
    void setOrbitRadius(float radius) { m_orbitRadius = radius; }
    void setOrbitSpeed(float speed) { m_orbitSpeed = speed; }
    void setOrbitStartAngle(float angle) { m_orbitAngle = angle; }
    void enableOrbit(bool enabled) { m_orbiting = enabled; }
    
    void update(float deltaTime) override {
        // Rotate around Y axis
        auto rot = getRotation();
        rot.yaw += m_rotationSpeed * deltaTime;
        if (rot.yaw > 360.0f) rot.yaw -= 360.0f;
        setRotation(rot);
        
        // Orbit around origin if enabled
        if (m_orbiting) {
            m_orbitAngle += m_orbitSpeed * deltaTime;
            float x = std::cos(m_orbitAngle) * m_orbitRadius;
            float z = std::sin(m_orbitAngle) * m_orbitRadius;
            setPosition(x, getPosition().y, z);
        }
    }

private:
    float m_rotationSpeed = 45.0f;
    float m_orbitRadius = 2.0f;
    float m_orbitSpeed = 0.5f;
    float m_orbitAngle = 0.0f;
    bool m_orbiting = false;
};

/**
 * @brief Scene demonstrating materials and lighting.
 */
class MaterialsLightingScene : public vde::Scene {
public:
    MaterialsLightingScene() = default;
    
    void onEnter() override {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Phase 4: Materials & Lighting Demo" << std::endl;
        std::cout << "========================================\n" << std::endl;
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "  - PBR Materials (albedo, roughness, metallic)" << std::endl;
        std::cout << "  - Emissive materials (self-illumination)" << std::endl;
        std::cout << "  - Three-point lighting setup" << std::endl;
        std::cout << "  - Multiple material types\n" << std::endl;
        std::cout << "You should see:" << std::endl;
        std::cout << "  - 5 rotating cubes with different materials" << std::endl;
        std::cout << "  - Center: White default material" << std::endl;
        std::cout << "  - Orbiting: Red, Blue metallic, Green, Yellow emissive\n" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  F     - Fail test (if you don't see cubes)" << std::endl;
        std::cout << "  ESC   - Exit early" << std::endl;
        std::cout << "  (Auto-closes in " << AUTO_TERMINATE_SECONDS << " seconds)\n" << std::endl;
        
        // Set up orbit camera
        auto* camera = new vde::OrbitCamera(vde::Position(0, 0, 0), 8.0f, 25.0f, 45.0f);
        setCamera(camera);
        
        // Set up three-point lighting
        auto* lightBox = new vde::ThreePointLightBox(vde::Color::white(), 1.2f);
        lightBox->setAmbientColor(vde::Color(0.15f, 0.15f, 0.2f));
        lightBox->setAmbientIntensity(1.0f);
        setLightBox(lightBox);
        
        // Dark blue background
        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
        
        // Create center cube with default white material
        auto centerCube = addEntity<MaterialCube>();
        centerCube->setName("CenterCube");
        centerCube->setPosition(0.0f, 0.0f, 0.0f);
        centerCube->setMesh(vde::Mesh::createCube(1.0f));
        centerCube->setMaterial(vde::Material::createDefault());
        centerCube->setRotationSpeed(20.0f);
        
        // Red matte cube (low roughness) - starts at 0 degrees
        auto redCube = addEntity<MaterialCube>();
        redCube->setName("RedCube");
        redCube->setMesh(vde::Mesh::createCube(0.7f));
        auto redMat = vde::Material::createColored(vde::Color::red());
        redMat->setRoughness(0.2f);  // Smooth/shiny
        redCube->setMaterial(redMat);
        redCube->setRotationSpeed(35.0f);
        redCube->enableOrbit(true);
        redCube->setOrbitRadius(2.5f);
        redCube->setOrbitSpeed(0.4f);
        redCube->setOrbitStartAngle(0.0f);  // 0 degrees
        
        // Blue metallic cube - starts at 90 degrees
        auto blueCube = addEntity<MaterialCube>();
        blueCube->setName("BlueCube");
        blueCube->setMesh(vde::Mesh::createCube(0.7f));
        blueCube->setMaterial(vde::Material::createMetallic(vde::Color::fromHex(0x4a90d9), 0.3f));
        blueCube->setRotationSpeed(40.0f);
        blueCube->enableOrbit(true);
        blueCube->setOrbitRadius(2.5f);
        blueCube->setOrbitSpeed(0.4f);
        blueCube->setOrbitStartAngle(1.5708f);  // 90 degrees (PI/2)
        
        // Green rough cube - starts at 180 degrees
        auto greenCube = addEntity<MaterialCube>();
        greenCube->setName("GreenCube");
        greenCube->setMesh(vde::Mesh::createCube(0.7f));
        auto greenMat = vde::Material::createColored(vde::Color::green());
        greenMat->setRoughness(0.9f);  // Very rough/matte
        greenCube->setMaterial(greenMat);
        greenCube->setRotationSpeed(30.0f);
        greenCube->enableOrbit(true);
        greenCube->setOrbitRadius(2.5f);
        greenCube->setOrbitSpeed(0.4f);
        greenCube->setOrbitStartAngle(3.1416f);  // 180 degrees (PI)
        
        // Yellow emissive cube (glowing) - starts at 270 degrees
        auto yellowCube = addEntity<MaterialCube>();
        yellowCube->setName("YellowCube");
        yellowCube->setMesh(vde::Mesh::createCube(0.7f));
        yellowCube->setMaterial(vde::Material::createEmissive(vde::Color::yellow(), 0.8f));
        yellowCube->setRotationSpeed(25.0f);
        yellowCube->enableOrbit(true);
        yellowCube->setOrbitRadius(2.5f);
        yellowCube->setOrbitSpeed(0.4f);
        yellowCube->setOrbitStartAngle(4.7124f);  // 270 degrees (3*PI/2);
        
        m_startTime = 0.0f;
    }
    
    void onExit() override {
        std::cout << "MaterialsLightingScene: Exiting" << std::endl;
    }
    
    void update(float deltaTime) override {
        Scene::update(deltaTime);
        
        m_elapsedTime += deltaTime;
        
        // Check for fail key
        auto* input = dynamic_cast<DemoInputHandler*>(getInputHandler());
        if (input) {
            if (input->isFailPressed()) {
                std::cerr << "\n========================================" << std::endl;
                std::cerr << "  TEST FAILED: User reported issue" << std::endl;
                std::cerr << "  User could not see expected output:" << std::endl;
                std::cerr << "    - 5 rotating cubes with different materials" << std::endl;
                std::cerr << "    - Three-point lighting illumination" << std::endl;
                std::cerr << "========================================\n" << std::endl;
                m_testFailed = true;
                if (m_game) m_game->quit();
                return;
            }
            
            if (input->isEscapePressed()) {
                std::cout << "User requested early exit." << std::endl;
                if (m_game) m_game->quit();
                return;
            }
        }
        
        // Auto-terminate after configured time
        if (m_elapsedTime >= AUTO_TERMINATE_SECONDS) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "  TEST PASSED: Demo completed successfully" << std::endl;
            std::cout << "  Duration: " << m_elapsedTime << " seconds" << std::endl;
            std::cout << "========================================\n" << std::endl;
            if (m_game) m_game->quit();
        }
        
        // Slowly rotate camera around scene
        auto* camera = dynamic_cast<vde::OrbitCamera*>(getCamera());
        if (camera) {
            float yaw = camera->getYaw() + 15.0f * deltaTime;
            camera->setYaw(yaw);
        }
    }
    
    bool didTestFail() const { return m_testFailed; }

private:
    float m_elapsedTime = 0.0f;
    float m_startTime = 0.0f;
    bool m_testFailed = false;
};

/**
 * @brief Main game class for the demo.
 */
class MaterialsLightingDemo : public vde::Game {
public:
    void onStart() override {
        std::cout << "Starting Materials & Lighting Demo..." << std::endl;
        
        // Set up input handler
        m_inputHandler = std::make_unique<DemoInputHandler>();
        setInputHandler(m_inputHandler.get());
        
        // Create and activate the demo scene
        // Note: addScene takes ownership, so we pass a raw new pointer
        auto* scene = new MaterialsLightingScene();
        m_scenePtr = scene;  // Keep a reference for checking test status
        addScene("main", scene);
        setActiveScene("main");
    }
    
    void onShutdown() override {
        // Check if test failed
        if (m_scenePtr && m_scenePtr->didTestFail()) {
            m_exitCode = 1;
        }
        std::cout << "Demo shutdown complete." << std::endl;
    }
    
    int getExitCode() const { return m_exitCode; }

private:
    std::unique_ptr<DemoInputHandler> m_inputHandler;
    MaterialsLightingScene* m_scenePtr = nullptr;  // Non-owning reference
    int m_exitCode = 0;
};

int main() {
    MaterialsLightingDemo demo;
    
    vde::GameSettings settings;
    settings.gameName = "VDE Materials & Lighting Demo";
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
