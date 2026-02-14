/**
 * @file main.cpp
 * @brief Hex Prism Stacks Demo with mouse picking and drag-drop.
 *
 * This example demonstrates:
 * - Multiple stacks of texture-mapped hexagonal prisms
 * - Mouse picking using camera screen-to-world rays
 * - Drag-and-drop movement of selected prisms
 * - Blue outline highlight for current selection
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

class HexPrismStacksInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override { BaseExampleInputHandler::onKeyPress(key); }

    void onMouseMove(double x, double y) override {
        m_mouseX = static_cast<float>(x);
        m_mouseY = static_cast<float>(y);
    }

    void onMouseButtonPress(int button, [[maybe_unused]] double x,
                            [[maybe_unused]] double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_leftDown = true;
            m_leftPressed = true;
        }
    }

    void onMouseButtonRelease(int button, [[maybe_unused]] double x,
                              [[maybe_unused]] double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_leftDown = false;
            m_leftReleased = true;
        }
    }

    bool consumeLeftPressed() {
        bool pressed = m_leftPressed;
        m_leftPressed = false;
        return pressed;
    }

    bool consumeLeftReleased() {
        bool released = m_leftReleased;
        m_leftReleased = false;
        return released;
    }

    bool isLeftDown() const { return m_leftDown; }
    float getMouseX() const { return m_mouseX; }
    float getMouseY() const { return m_mouseY; }

  private:
    bool m_leftDown = false;
    bool m_leftPressed = false;
    bool m_leftReleased = false;
    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;
};

class HexPrismStacksScene : public vde::examples::BaseExampleScene {
  public:
    HexPrismStacksScene() : BaseExampleScene(45.0f) {}

    void onEnter() override {
        printExampleHeader();

        setBackgroundColor(Color::fromHex(0x151a24));

        auto* camera = new OrbitCamera(Position(0.0f, 0.0f, 0.0f), 24.0f, 48.0f, 35.0f);
        camera->setNearPlane(0.1f);
        camera->setFarPlane(200.0f);
        setCamera(camera);

        auto* lightBox = new ThreePointLightBox(Color::white(), 1.1f);
        lightBox->setAmbientColor(Color(0.18f, 0.2f, 0.26f));
        lightBox->setAmbientIntensity(1.0f);
        setLightBox(lightBox);

        createCheckerTexture();
        createPrismMeshes();
        createPrismStacks();

        std::cout << "Created " << m_prisms.size() << " draggable prisms in stacked groups."
                  << std::endl;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<HexPrismStacksInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        if (input->consumeLeftPressed()) {
            handleLeftPress(*input);
        }

        if (m_dragging && input->isLeftDown()) {
            updateDrag(*input);
        }

        if (input->consumeLeftReleased()) {
            m_dragging = false;
        }

        (void)deltaTime;
    }

  protected:
    std::string getExampleName() const override { return "Hex Prism Stacks (Pick + Drag)"; }

    std::vector<std::string> getFeatures() const override {
        return {"Multiple stacks of textured hexagonal prisms",
                "Mouse picking with world-space ray tests", "Drag selected prism to a new location",
                "Blue outline on selected prism"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Several stacks of textured six-sided prisms",
                "Only one prism at a time with a blue outline",
                "Selected prism follows mouse while left button is held"};
    }

    std::vector<std::string> getControls() const override {
        return {"Left Click      - Select prism", "Hold Left Click - Drag selected prism",
                "Release Left    - Drop prism"};
    }

  private:
    struct PrismEntry {
        std::shared_ptr<MeshEntity> body;
        std::shared_ptr<MeshEntity> outline;
        float pickRadius = 1.0f;
    };

    std::vector<PrismEntry> m_prisms;
    std::shared_ptr<Texture> m_checkerTexture;
    std::shared_ptr<Mesh> m_prismMesh;
    std::shared_ptr<Mesh> m_outlineMesh;

    int m_selectedIndex = -1;
    bool m_dragging = false;
    float m_dragPlaneY = 0.0f;
    glm::vec3 m_dragOffset{0.0f};

    static bool intersectRayPlaneY(const Ray& ray, float planeY, glm::vec3& outPoint) {
        float denom = ray.direction.y;
        if (std::abs(denom) < 1e-5f) {
            return false;
        }

        float t = (planeY - ray.origin.y) / denom;
        if (t < 0.0f) {
            return false;
        }

        outPoint = ray.origin + ray.direction * t;
        return true;
    }

    static bool intersectRaySphere(const Ray& ray, const glm::vec3& center, float radius,
                                   float& outT) {
        glm::vec3 oc = ray.origin - center;
        float a = glm::dot(ray.direction, ray.direction);
        float b = 2.0f * glm::dot(oc, ray.direction);
        float c = glm::dot(oc, oc) - radius * radius;
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) {
            return false;
        }

        float sqrtDisc = std::sqrt(disc);
        float t0 = (-b - sqrtDisc) / (2.0f * a);
        float t1 = (-b + sqrtDisc) / (2.0f * a);

        float t = (t0 >= 0.0f) ? t0 : t1;
        if (t < 0.0f) {
            return false;
        }

        outT = t;
        return true;
    }

    Ray getMouseRay(const HexPrismStacksInputHandler& input) const {
        auto* game = getGame();
        auto* window = game ? game->getWindow() : nullptr;
        auto* camera = getCamera();

        if (!window || !camera) {
            return Ray{};
        }

        float screenWidth = static_cast<float>(window->getWidth());
        float screenHeight = static_cast<float>(window->getHeight());

        return camera->screenToWorldRay(input.getMouseX(), input.getMouseY(), screenWidth,
                                        screenHeight);
    }

    void handleLeftPress(const HexPrismStacksInputHandler& input) {
        Ray ray = getMouseRay(input);

        int hitIndex = -1;
        float bestT = std::numeric_limits<float>::max();

        for (size_t i = 0; i < m_prisms.size(); ++i) {
            const auto& prism = m_prisms[i];
            glm::vec3 center = m_prisms[i].body->getPosition().toVec3();

            float t = 0.0f;
            if (intersectRaySphere(ray, center, prism.pickRadius, t) && t < bestT) {
                bestT = t;
                hitIndex = static_cast<int>(i);
            }
        }

        setSelectedIndex(hitIndex);

        if (m_selectedIndex < 0) {
            m_dragging = false;
            return;
        }

        m_dragPlaneY = m_prisms[m_selectedIndex].body->getPosition().y;

        glm::vec3 hitPoint;
        if (!intersectRayPlaneY(ray, m_dragPlaneY, hitPoint)) {
            m_dragging = false;
            return;
        }

        glm::vec3 selectedPos = m_prisms[m_selectedIndex].body->getPosition().toVec3();
        m_dragOffset = selectedPos - hitPoint;
        m_dragging = true;
    }

    void updateDrag(const HexPrismStacksInputHandler& input) {
        if (m_selectedIndex < 0) {
            m_dragging = false;
            return;
        }

        Ray ray = getMouseRay(input);
        glm::vec3 hitPoint;
        if (!intersectRayPlaneY(ray, m_dragPlaneY, hitPoint)) {
            return;
        }

        glm::vec3 target = hitPoint + m_dragOffset;
        auto& prism = m_prisms[m_selectedIndex];

        prism.body->setPosition(target.x, m_dragPlaneY, target.z);
        prism.outline->setPosition(target.x, m_dragPlaneY, target.z);
    }

    void setSelectedIndex(int index) {
        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_prisms.size())) {
            m_prisms[m_selectedIndex].outline->setVisible(false);
        }

        m_selectedIndex = index;

        if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_prisms.size())) {
            m_prisms[m_selectedIndex].outline->setVisible(true);
        }
    }

    void createCheckerTexture() {
        constexpr int kSize = 128;
        constexpr int kChannels = 4;
        std::vector<uint8_t> pixels(kSize * kSize * kChannels, 255);

        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                int idx = (y * kSize + x) * kChannels;
                bool checker = ((x / 16) + (y / 16)) % 2 == 0;

                if (checker) {
                    pixels[idx + 0] = 236;
                    pixels[idx + 1] = 194;
                    pixels[idx + 2] = 120;
                } else {
                    pixels[idx + 0] = 107;
                    pixels[idx + 1] = 163;
                    pixels[idx + 2] = 196;
                }
                pixels[idx + 3] = 255;
            }
        }

        m_checkerTexture = std::make_shared<Texture>();
        m_checkerTexture->loadFromData(pixels.data(), kSize, kSize);
        m_checkerTexture->uploadToGPU(getGame()->getVulkanContext());
    }

    void createPrismMeshes() {
        constexpr float kHexRadius = 0.85f;
        constexpr float kHeight = 1.0f;
        constexpr int kHexSides = 6;

        m_prismMesh = Mesh::createCylinder(kHexRadius, kHeight, kHexSides);
        m_outlineMesh = Mesh::createWireframe(m_prismMesh, 0.045f);
    }

    void addPrism(const glm::vec3& position) {
        auto body = addEntity<MeshEntity>();
        body->setMesh(m_prismMesh);
        body->setTexture(m_checkerTexture);
        body->setColor(Color::white());
        body->setPosition(position.x, position.y, position.z);

        auto outline = addEntity<MeshEntity>();
        outline->setMesh(m_outlineMesh);
        outline->setColor(Color::fromHex(0x2b6cff));
        outline->setPosition(position.x, position.y, position.z);
        outline->setVisible(false);

        float pickRadius = std::max(1.0f, m_prismMesh->getBoundingRadius());
        m_prisms.push_back({body, outline, pickRadius});
    }

    void createPrismStacks() {
        struct StackDef {
            glm::vec2 center;
            int levels;
        };

        const std::vector<StackDef> stacks = {
            {{-8.0f, -6.0f}, 4}, {{-2.5f, 5.0f}, 5}, {{4.5f, -3.5f}, 3}, {{9.0f, 4.0f}, 4}};

        constexpr float kPrismHeight = 1.0f;
        constexpr float kHalfHeight = 0.5f;

        for (const auto& stack : stacks) {
            for (int level = 0; level < stack.levels; ++level) {
                float y = kHalfHeight + static_cast<float>(level) * kPrismHeight;
                addPrism(glm::vec3(stack.center.x, y, stack.center.y));
            }
        }
    }
};

class HexPrismStacksDemo
    : public vde::examples::BaseExampleGame<HexPrismStacksInputHandler, HexPrismStacksScene> {};

int main() {
    HexPrismStacksDemo demo;
    return vde::examples::runExample(demo, "VDE Hex Prism Stacks Demo", 1280, 720);
}
