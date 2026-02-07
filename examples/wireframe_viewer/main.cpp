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
 * The wireframe is built from thin tube geometry so it renders through
 * the standard mesh (VK_POLYGON_MODE_FILL) pipeline.
 *
 * Press 'F' to fail the test, ESC to exit early.
 */

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "../ExampleBase.h"

// =============================================================================
// Constants
// =============================================================================

enum class ShapeType { Pyramid = 0, Cube, Sphere };
enum class RenderMode { Wireframe = 0, Solid, SolidPlusWireframe };

static constexpr float kWireframeThickness = 0.015f;
static constexpr float kWireframeOverlayScale = 1.005f;

// =============================================================================
// Wireframe Geometry Helpers
// =============================================================================

/**
 * @brief Append a thin rectangular tube between two 3D points.
 *
 * Each tube has 4 side faces (8 vertices, 24 indices).
 * The vertex `color` field stores the outward-pointing normal
 * (required by the mesh shader for lighting).
 */
static void addEdge(std::vector<vde::Vertex>& vertices, std::vector<uint32_t>& indices,
                    const glm::vec3& start, const glm::vec3& end, float thickness) {
    glm::vec3 dir = end - start;
    float len = glm::length(dir);
    if (len < 0.0001f)
        return;
    dir /= len;

    // Build a perpendicular frame around the edge direction
    glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    glm::vec3 forward = glm::normalize(glm::cross(right, dir));

    float halfT = thickness * 0.5f;
    uint32_t base = static_cast<uint32_t>(vertices.size());
    glm::vec2 uv(0.0f, 0.0f);

    // Four corner offsets and their outward normals
    glm::vec3 offsets[4] = {
        right * halfT + forward * halfT,
        right * halfT - forward * halfT,
        -right * halfT - forward * halfT,
        -right * halfT + forward * halfT,
    };
    glm::vec3 normals[4] = {
        glm::normalize(right + forward),
        glm::normalize(right - forward),
        glm::normalize(-right - forward),
        glm::normalize(-right + forward),
    };

    // 8 vertices: 4 at start, 4 at end
    for (int i = 0; i < 4; i++) {
        vertices.push_back({start + offsets[i], normals[i], uv});
    }
    for (int i = 0; i < 4; i++) {
        vertices.push_back({end + offsets[i], normals[i], uv});
    }

    // 4 side quads (2 tris each)
    for (int i = 0; i < 4; i++) {
        uint32_t next = static_cast<uint32_t>((i + 1) % 4);
        indices.push_back(base + i);
        indices.push_back(base + i + 4);
        indices.push_back(base + next + 4);
        indices.push_back(base + i);
        indices.push_back(base + next + 4);
        indices.push_back(base + next);
    }
}

/**
 * @brief Build a wireframe mesh from a set of points and edge pairs.
 */
static std::shared_ptr<vde::Mesh> buildWireframeMesh(const std::vector<glm::vec3>& points,
                                                     const std::vector<std::pair<int, int>>& edges,
                                                     float thickness) {
    std::vector<vde::Vertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& [a, b] : edges) {
        addEdge(vertices, indices, points[a], points[b], thickness);
    }

    auto mesh = std::make_shared<vde::Mesh>();
    mesh->setData(vertices, indices);
    return mesh;
}

// =============================================================================
// Pyramid Mesh Creation
// =============================================================================

static std::shared_ptr<vde::Mesh> createPyramidSolid() {
    float halfBase = 0.5f;
    float baseY = -0.25f;
    float apexY = 0.75f;

    glm::vec3 bl(-halfBase, baseY, -halfBase);
    glm::vec3 br(halfBase, baseY, -halfBase);
    glm::vec3 fr(halfBase, baseY, halfBase);
    glm::vec3 fl(-halfBase, baseY, halfBase);
    glm::vec3 apex(0.0f, apexY, 0.0f);

    glm::vec2 uv(0.0f, 0.0f);
    std::vector<vde::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Helper: add a triangle with a computed face normal stored in the
    // vertex color field (the mesh shader uses color-as-normal).
    auto addTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({a, normal, uv});
        vertices.push_back({b, normal, uv});
        vertices.push_back({c, normal, uv});
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    };

    // Base (two triangles, normal facing down)
    addTri(bl, br, fr);
    addTri(bl, fr, fl);

    // Side faces (normals face outward automatically)
    addTri(bl, apex, br);  // back
    addTri(br, apex, fr);  // right
    addTri(fr, apex, fl);  // front
    addTri(fl, apex, bl);  // left

    auto mesh = std::make_shared<vde::Mesh>();
    mesh->setData(vertices, indices);
    return mesh;
}

static std::shared_ptr<vde::Mesh> createPyramidWireframe(float thickness) {
    float halfBase = 0.5f;
    float baseY = -0.25f;
    float apexY = 0.75f;

    std::vector<glm::vec3> pts = {
        {-halfBase, baseY, -halfBase},  // 0 BL
        {halfBase, baseY, -halfBase},   // 1 BR
        {halfBase, baseY, halfBase},    // 2 FR
        {-halfBase, baseY, halfBase},   // 3 FL
        {0.0f, apexY, 0.0f},            // 4 Apex
    };

    std::vector<std::pair<int, int>> edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},  // base
        {0, 4}, {1, 4}, {2, 4}, {3, 4},  // sides to apex
    };

    return buildWireframeMesh(pts, edges, thickness);
}

// =============================================================================
// Cube Wireframe
// =============================================================================

static std::shared_ptr<vde::Mesh> createCubeWireframe(float size, float thickness) {
    float h = size * 0.5f;

    std::vector<glm::vec3> pts = {
        {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h},  // bottom
        {-h, h, -h},  {h, h, -h},  {h, h, h},  {-h, h, h},   // top
    };

    std::vector<std::pair<int, int>> edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},  // bottom
        {4, 5}, {5, 6}, {6, 7}, {7, 4},  // top
        {0, 4}, {1, 5}, {2, 6}, {3, 7},  // vertical
    };

    return buildWireframeMesh(pts, edges, thickness);
}

// =============================================================================
// Sphere Wireframe (latitude / longitude lines)
// =============================================================================

static std::shared_ptr<vde::Mesh> createSphereWireframe(float radius, int latLines, int lonLines,
                                                        int segmentsPerCircle, float thickness) {
    std::vector<vde::Vertex> vertices;
    std::vector<uint32_t> indices;

    const float pi = glm::pi<float>();
    const float twoPi = 2.0f * pi;

    // Latitude lines (horizontal circles)
    for (int i = 1; i <= latLines; i++) {
        float phi = pi * static_cast<float>(i) / static_cast<float>(latLines + 1);
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        for (int j = 0; j < segmentsPerCircle; j++) {
            float t1 = twoPi * static_cast<float>(j) / static_cast<float>(segmentsPerCircle);
            float t2 = twoPi * static_cast<float>(j + 1) / static_cast<float>(segmentsPerCircle);

            glm::vec3 p1(r * std::cos(t1), y, r * std::sin(t1));
            glm::vec3 p2(r * std::cos(t2), y, r * std::sin(t2));
            addEdge(vertices, indices, p1, p2, thickness);
        }
    }

    // Longitude lines (vertical great-circle arcs)
    for (int j = 0; j < lonLines; j++) {
        float theta = twoPi * static_cast<float>(j) / static_cast<float>(lonLines);

        for (int i = 0; i < segmentsPerCircle; i++) {
            float phi1 = pi * static_cast<float>(i) / static_cast<float>(segmentsPerCircle);
            float phi2 = pi * static_cast<float>(i + 1) / static_cast<float>(segmentsPerCircle);

            glm::vec3 p1(radius * std::sin(phi1) * std::cos(theta), radius * std::cos(phi1),
                         radius * std::sin(phi1) * std::sin(theta));
            glm::vec3 p2(radius * std::sin(phi2) * std::cos(theta), radius * std::cos(phi2),
                         radius * std::sin(phi2) * std::sin(theta));
            addEdge(vertices, indices, p1, p2, thickness);
        }
    }

    auto mesh = std::make_shared<vde::Mesh>();
    mesh->setData(vertices, indices);
    return mesh;
}

// =============================================================================
// Ray-Sphere Hit Testing
// =============================================================================

/**
 * @brief Test whether a ray from the mouse cursor intersects a bounding sphere.
 *
 * The mouse position is unprojected into a world-space ray using the
 * inverse of the view-projection matrix.  Vulkan NDC conventions apply
 * (Y points down, Z depth 0..1).
 */
static bool hitTestSphere(double mouseX, double mouseY, float screenW, float screenH,
                          const glm::mat4& viewProj, const glm::vec3& sphereCenter,
                          float sphereRadius) {
    // Mouse -> Vulkan NDC (Y down, Z 0..1)
    float ndcX = (2.0f * static_cast<float>(mouseX) / screenW) - 1.0f;
    float ndcY = (2.0f * static_cast<float>(mouseY) / screenH) - 1.0f;

    glm::mat4 invVP = glm::inverse(viewProj);

    glm::vec4 nearClip = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    glm::vec4 farClip = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearClip /= nearClip.w;
    farClip /= farClip.w;

    glm::vec3 rayOrigin(nearClip);
    glm::vec3 rayDir = glm::normalize(glm::vec3(farClip) - rayOrigin);

    // Analytical ray-sphere intersection
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    return (b * b - 4.0f * a * c) >= 0.0f;
}

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

        // Pre-create all meshes
        m_pyramidSolid = createPyramidSolid();
        m_pyramidWireframe = createPyramidWireframe(kWireframeThickness);
        m_cubeSolid = vde::Mesh::createCube(1.0f);
        m_cubeWireframe = createCubeWireframe(1.0f, kWireframeThickness);
        m_sphereSolid = vde::Mesh::createSphere(0.5f, 32, 16);
        m_sphereWireframe = createSphereWireframe(0.5f, 6, 8, 48, kWireframeThickness);

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
    float m_boundingRadius = 0.8f;

    // -----------------------------------------------------------------

    void switchShape(ShapeType shape) {
        m_currentShape = shape;

        switch (shape) {
        case ShapeType::Pyramid:
            m_solidEntity->setMesh(m_pyramidSolid);
            m_wireframeEntity->setMesh(m_pyramidWireframe);
            m_boundingRadius = 0.8f;
            break;
        case ShapeType::Cube:
            m_solidEntity->setMesh(m_cubeSolid);
            m_wireframeEntity->setMesh(m_cubeWireframe);
            m_boundingRadius = 0.87f;
            break;
        case ShapeType::Sphere:
            m_solidEntity->setMesh(m_sphereSolid);
            m_wireframeEntity->setMesh(m_sphereWireframe);
            m_boundingRadius = 0.55f;
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
        glm::mat4 vp = cam->getViewProjectionMatrix();

        return hitTestSphere(mouseX, mouseY, w, h, vp, glm::vec3(0.0f), m_boundingRadius);
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
