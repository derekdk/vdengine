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

#include <cmath>
#include <iostream>

#include "../ExampleBase.h"

/**
 * @brief Input handler for the demo.
 */
class DemoInputHandler : public vde::examples::BaseExampleInputHandler {};

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
        if (rot.yaw > 360.0f)
            rot.yaw -= 360.0f;
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
class MaterialsLightingScene : public vde::examples::BaseExampleScene {
  public:
    MaterialsLightingScene() : BaseExampleScene(5.0f) {}

    void onEnter() override {
        // Print standard header
        printExampleHeader();

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
    }

    void onExit() override { std::cout << "MaterialsLightingScene: Exiting" << std::endl; }

    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);

        // Slowly rotate camera around scene
        auto* camera = dynamic_cast<vde::OrbitCamera*>(getCamera());
        if (camera) {
            float yaw = camera->getYaw() + 15.0f * deltaTime;
            camera->setYaw(yaw);
        }
    }

  protected:
    std::string getExampleName() const override { return "Materials & Lighting"; }

    std::vector<std::string> getFeatures() const override {
        return {"PBR Materials (albedo, roughness, metallic)",
                "Emissive materials (self-illumination)", "Three-point lighting setup",
                "Multiple material types"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"5 rotating cubes with different materials", "Center: White default material",
                "Orbiting: Red, Blue metallic, Green, Yellow emissive"};
    }

    std::string getFailureMessage() const override {
        return "User could not see expected output:\n    - 5 rotating cubes with different "
               "materials\n    - Three-point lighting illumination";
    }
};

/**
 * @brief Main game class for the demo.
 */
class MaterialsLightingDemo
    : public vde::examples::BaseExampleGame<DemoInputHandler, MaterialsLightingScene> {
  public:
    void onStart() override {
        std::cout << "Starting Materials & Lighting Demo..." << std::endl;
        BaseExampleGame::onStart();
    }

    void onShutdown() override {
        BaseExampleGame::onShutdown();
        std::cout << "Demo shutdown complete." << std::endl;
    }
};

int main() {
    MaterialsLightingDemo demo;
    return vde::examples::runExample(demo, "VDE Materials & Lighting Demo", 1280, 720);
}
