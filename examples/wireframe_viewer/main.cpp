/**
 * @file main.cpp
 * @brief Wireframe Viewer Demo for VDE
 *
 * Demonstrates:
 * - Switching between shapes (pyramid, cube, sphere) with 1/2/3 keys
 * - Toggle render modes (wireframe -> solid -> solid+wireframe) with S key
 * - Mouse wheel zoom
 * - Click-and-drag to rotate the object (only when clicking on the object)
 *
 * The wireframe is built via Mesh::createWireframe() which generates thin
 * tube geometry so it renders through the standard mesh pipeline.
 *
 * Press 'F' to fail the test, ESC to exit early.
 */

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

#include "../ExampleBase.h"

// =============================================================================
// Constants
// =============================================================================

enum class ShapeType { Pyramid = 0, Cube, Sphere };
enum class RenderMode { Wireframe = 0, Solid, SolidPlusWireframe };

static constexpr float kWireframeThickness = 0.015f;
static constexpr float kWireframeOverlayScale = 1.005f;

// =============================================================================
// Input Handler
// =============================================================================

class ViewerInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_1)
            m_shapeRequest = ShapeType::Pyramid;
        if (key == vde::KEY_2)
            m_shapeRequest = ShapeType::Cube;
        if (key == vde::KEY_3)
            m_shapeRequest = ShapeType::Sphere;
        if (key == vde::KEY_S)
            m_toggleMode = true;
    }

    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = true;
            m_newClick = true;
            m_clickX = x;
            m_clickY = y;
            m_prevMouseX = x;
            m_prevMouseY = y;
            m_dragDeltaX = 0.0;
            m_dragDeltaY = 0.0;
            m_hitObject = false;
        }
    }

    void onMouseButtonRelease(int button, [[maybe_unused]] double x,
                              [[maybe_unused]] double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = false;
            m_hitObject = false;
            m_newClick = false;
        }
    }

    void onMouseMove(double x, double y) override {
        if (m_mouseDown && m_hitObject) {
            m_dragDeltaX += (x - m_prevMouseX);
            m_dragDeltaY += (y - m_prevMouseY);
        }
        m_prevMouseX = x;
        m_prevMouseY = y;
    }

    void onMouseScroll([[maybe_unused]] double xOffset, double yOffset) override {
        m_scrollDelta += static_cast<float>(yOffset);
    }

    // --- queries (consume-on-read) ---

    bool hasShapeRequest(ShapeType& out) {
        if (m_shapeRequest.has_value()) {
            out = m_shapeRequest.value();
            m_shapeRequest.reset();
            return true;
        }
        return false;
    }

    bool shouldToggleMode() {
        bool v = m_toggleMode;
        m_toggleMode = false;
        return v;
    }

    float consumeScrollDelta() {
        float v = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return v;
    }

    bool consumeNewClick(double& outX, double& outY) {
        if (m_newClick) {
            m_newClick = false;
            outX = m_clickX;
            outY = m_clickY;
            return true;
        }
        return false;
    }

    void consumeDragDelta(double& dx, double& dy) {
        dx = m_dragDeltaX;
        dy = m_dragDeltaY;
        m_dragDeltaX = 0.0;
        m_dragDeltaY = 0.0;
    }

    bool isMouseDown() const { return m_mouseDown; }
    void setHitObject(bool hit) { m_hitObject = hit; }
    bool getHitObject() const { return m_hitObject; }

  private:
    std::optional<ShapeType> m_shapeRequest;
    bool m_toggleMode = false;
    float m_scrollDelta = 0.0f;

    bool m_mouseDown = false;
    bool m_newClick = false;
    bool m_hitObject = false;
    double m_clickX = 0.0;
    double m_clickY = 0.0;
    double m_prevMouseX = 0.0;
    double m_prevMouseY = 0.0;
    double m_dragDeltaX = 0.0;
    double m_dragDeltaY = 0.0;
};

// =============================================================================
// Scene
// =============================================================================

class WireframeViewerScene : public vde::examples::BaseExampleScene {
  public:
    WireframeViewerScene() : BaseExampleScene(600.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Orbit camera
        auto* camera = new vde::OrbitCamera(vde::Position(0, 0.15f, 0), 3.5f, 25.0f, 30.0f);
        camera->setZoomLimits(1.0f, 20.0f);
        setCamera(camera);

        // Three-point lighting
        auto* lightBox = new vde::ThreePointLightBox(vde::Color::white(), 1.0f);
        lightBox->setAmbientColor(vde::Color(0.2f, 0.2f, 0.25f));
        lightBox->setAmbientIntensity(1.0f);
        setLightBox(lightBox);

        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));

        // --- Create solid meshes using API factory methods ---
        m_pyramidSolid = vde::Mesh::createPyramid(1.0f, 1.0f);
        m_cubeSolid = vde::Mesh::createCube(1.0f);
        m_sphereSolid = vde::Mesh::createSphere(0.5f, 32, 16);

        // --- Create wireframe meshes from solids ---
        m_pyramidWireframe = vde::Mesh::createWireframe(m_pyramidSolid, kWireframeThickness);
        m_cubeWireframe = vde::Mesh::createWireframe(m_cubeSolid, kWireframeThickness);
        m_sphereWireframe = vde::Mesh::createWireframe(m_sphereSolid, kWireframeThickness);

        // Pre-create materials
        m_solidMaterial =
            std::make_shared<vde::Material>(vde::Color::fromHex(0x4a90d9), 0.4f, 0.0f);
        m_wireframeBrightMaterial = vde::Material::createColored(vde::Color(0.0f, 1.0f, 0.8f));
        m_wireframeBrightMaterial->setRoughness(0.6f);
        m_wireframeDarkMaterial = vde::Material::createColored(vde::Color(0.08f, 0.08f, 0.08f));
        m_wireframeDarkMaterial->setRoughness(0.9f);

        // Create the two persistent entities
        m_solidEntity = addEntity<vde::MeshEntity>();
        m_solidEntity->setName("SolidShape");
        m_solidEntity->setPosition(0.0f, 0.0f, 0.0f);
        m_solidEntity->setMaterial(m_solidMaterial);

        m_wireframeEntity = addEntity<vde::MeshEntity>();
        m_wireframeEntity->setName("WireframeShape");
        m_wireframeEntity->setPosition(0.0f, 0.0f, 0.0f);
        m_wireframeEntity->setMaterial(m_wireframeBrightMaterial);

        // Show initial shape (pyramid in wireframe mode)
        switchShape(ShapeType::Pyramid);
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<ViewerInputHandler*>(getInputHandler());
        if (!input)
            return;

        // --- Shape switching (1/2/3) ---
        ShapeType requested;
        if (input->hasShapeRequest(requested)) {
            switchShape(requested);
        }

        // --- Mode toggle (S) ---
        if (input->shouldToggleMode()) {
            cycleRenderMode();
        }

        // --- Zoom (scroll wheel) ---
        float scroll = input->consumeScrollDelta();
        if (scroll != 0.0f) {
            auto* cam = dynamic_cast<vde::OrbitCamera*>(getCamera());
            if (cam) {
                cam->zoom(scroll * 0.5f);
            }
        }

        // --- Hit test on new click ---
        double clickX, clickY;
        if (input->consumeNewClick(clickX, clickY)) {
            bool hit = performHitTest(clickX, clickY);
            input->setHitObject(hit);
        }

        // --- Drag rotation ---
        if (input->isMouseDown() && input->getHitObject()) {
            double dx, dy;
            input->consumeDragDelta(dx, dy);
            if (dx != 0.0 || dy != 0.0) {
                m_objectYaw += static_cast<float>(dx) * 0.3f;
                m_objectPitch += static_cast<float>(dy) * 0.3f;
                m_objectPitch = glm::clamp(m_objectPitch, -89.0f, 89.0f);
                applyRotation();
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Wireframe Viewer"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "Wireframe / solid / solid+wireframe rendering",
            "Pyramid, cube, and sphere shapes",
            "Click-and-drag rotation (object only)",
            "Scroll wheel zoom",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "A wireframe pyramid centered on screen (initial)",
            "Shapes switch when pressing 1/2/3",
            "Render mode changes when pressing S",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "1     - Show pyramid",  "2     - Show cube",
            "3     - Show sphere",   "S     - Cycle: wireframe -> solid -> solid+wireframe",
            "SCROLL- Zoom in / out", "DRAG  - Rotate object (click must be on the shape)",
        };
    }

  private:
    // Pre-built meshes
    std::shared_ptr<vde::Mesh> m_pyramidSolid;
    std::shared_ptr<vde::Mesh> m_pyramidWireframe;
    std::shared_ptr<vde::Mesh> m_cubeSolid;
    std::shared_ptr<vde::Mesh> m_cubeWireframe;
    std::shared_ptr<vde::Mesh> m_sphereSolid;
    std::shared_ptr<vde::Mesh> m_sphereWireframe;

    // Materials
    std::shared_ptr<vde::Material> m_solidMaterial;
    std::shared_ptr<vde::Material> m_wireframeBrightMaterial;
    std::shared_ptr<vde::Material> m_wireframeDarkMaterial;

    // Scene entities
    std::shared_ptr<vde::MeshEntity> m_solidEntity;
    std::shared_ptr<vde::MeshEntity> m_wireframeEntity;

    // State
    ShapeType m_currentShape = ShapeType::Pyramid;
    RenderMode m_currentMode = RenderMode::Wireframe;
    float m_objectPitch = 0.0f;
    float m_objectYaw = 0.0f;

    // -----------------------------------------------------------------

    void switchShape(ShapeType shape) {
        m_currentShape = shape;

        switch (shape) {
        case ShapeType::Pyramid:
            m_solidEntity->setMesh(m_pyramidSolid);
            m_wireframeEntity->setMesh(m_pyramidWireframe);
            break;
        case ShapeType::Cube:
            m_solidEntity->setMesh(m_cubeSolid);
            m_wireframeEntity->setMesh(m_cubeWireframe);
            break;
        case ShapeType::Sphere:
            m_solidEntity->setMesh(m_sphereSolid);
            m_wireframeEntity->setMesh(m_sphereWireframe);
            break;
        }

        // Reset rotation
        m_objectPitch = 0.0f;
        m_objectYaw = 0.0f;
        applyRotation();
        applyRenderMode();

        const char* names[] = {"Pyramid", "Cube", "Sphere"};
        std::cout << "Shape: " << names[static_cast<int>(shape)] << std::endl;
    }

    void cycleRenderMode() {
        switch (m_currentMode) {
        case RenderMode::Wireframe:
            m_currentMode = RenderMode::Solid;
            break;
        case RenderMode::Solid:
            m_currentMode = RenderMode::SolidPlusWireframe;
            break;
        case RenderMode::SolidPlusWireframe:
            m_currentMode = RenderMode::Wireframe;
            break;
        }
        applyRenderMode();

        const char* modes[] = {"Wireframe", "Solid", "Solid + Wireframe"};
        std::cout << "Mode: " << modes[static_cast<int>(m_currentMode)] << std::endl;
    }

    void applyRenderMode() {
        switch (m_currentMode) {
        case RenderMode::Wireframe:
            m_solidEntity->setVisible(false);
            m_wireframeEntity->setVisible(true);
            m_wireframeEntity->setScale(1.0f);
            m_wireframeEntity->setMaterial(m_wireframeBrightMaterial);
            break;
        case RenderMode::Solid:
            m_solidEntity->setVisible(true);
            m_wireframeEntity->setVisible(false);
            break;
        case RenderMode::SolidPlusWireframe:
            m_solidEntity->setVisible(true);
            m_wireframeEntity->setVisible(true);
            m_wireframeEntity->setScale(kWireframeOverlayScale);
            m_wireframeEntity->setMaterial(m_wireframeDarkMaterial);
            break;
        }
    }

    void applyRotation() {
        vde::Rotation rot(m_objectPitch, m_objectYaw, 0.0f);
        m_solidEntity->setRotation(rot);
        m_wireframeEntity->setRotation(rot);
    }

    bool performHitTest(double mouseX, double mouseY) {
        auto* cam = getCamera();
        auto* game = getGame();
        if (!cam || !game || !game->getWindow())
            return false;

        float w = static_cast<float>(game->getWindow()->getWidth());
        float h = static_cast<float>(game->getWindow()->getHeight());

        // Use camera's screenToWorldRay + the solid mesh's bounding radius
        vde::Ray ray =
            cam->screenToWorldRay(static_cast<float>(mouseX), static_cast<float>(mouseY), w, h);

        auto solidMesh = m_solidEntity->getMesh();
        float radius = solidMesh ? solidMesh->getBoundingRadius() : 0.5f;
        return ray.hitsSphere(glm::vec3(0.0f), radius);
    }
};

// =============================================================================
// Game
// =============================================================================

class WireframeViewerGame
    : public vde::examples::BaseExampleGame<ViewerInputHandler, WireframeViewerScene> {
  public:
    void onStart() override { BaseExampleGame::onStart(); }
};

// =============================================================================
// Main
// =============================================================================

int main() {
    WireframeViewerGame game;
    return vde::examples::runExample(game, "VDE Wireframe Viewer", 1280, 720);
}
