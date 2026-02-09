/**
 * @file main.cpp
 * @brief Physics demo example demonstrating VDE PhysicsScene
 *
 * This example demonstrates:
 * - Creating a PhysicsScene with gravity
 * - Dynamic bodies (boxes) falling under gravity
 * - Static ground platform
 * - AABB collision detection and resolution
 * - Collision callbacks
 * - Visual sprites driven by physics body positions
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "../ExampleBase.h"

// ============================================================================
// Input Handler
// ============================================================================

class PhysicsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_R)
            m_resetPressed = true;
    }

    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

    bool isResetPressed() {
        bool val = m_resetPressed;
        m_resetPressed = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_resetPressed = false;
};

// ============================================================================
// Physics-driven sprite entity
// ============================================================================

class PhysicsSprite : public vde::SpriteEntity {
  public:
    PhysicsSprite() = default;

    void setPhysicsBodyId(vde::PhysicsBodyId id) { m_bodyId = id; }
    vde::PhysicsBodyId getPhysicsBodyId() const { return m_bodyId; }

    void syncFromPhysics(vde::PhysicsScene* physics) {
        if (physics && m_bodyId != vde::INVALID_PHYSICS_BODY_ID && physics->hasBody(m_bodyId)) {
            auto state = physics->getBodyState(m_bodyId);
            setPosition(vde::Position(state.position.x, state.position.y, 0.0f));
        }
    }

  private:
    vde::PhysicsBodyId m_bodyId = vde::INVALID_PHYSICS_BODY_ID;
};

// ============================================================================
// Scene
// ============================================================================

class PhysicsDemoScene : public vde::examples::BaseExampleScene {
  public:
    PhysicsDemoScene() : BaseExampleScene(10.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Enable physics with default gravity
        vde::PhysicsConfig config;
        config.gravity = {0.0f, -9.81f};
        config.fixedTimestep = 1.0f / 60.0f;
        enablePhysics(config);

        // Camera setup (orthographic 2D view)
        auto* cam = new vde::OrbitCamera(vde::Position(0.0f, 3.0f, 0.0f), 12.0f);
        setCamera(std::unique_ptr<vde::GameCamera>(cam));

        // Lighting
        setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));

        // Background
        setBackgroundColor(vde::Color(0.1f, 0.1f, 0.15f, 1.0f));

        // Create static ground
        createGround();

        // Create falling boxes
        spawnBoxes();

        // Set up collision callback
        m_collisionCount = 0;
        getPhysicsScene()->setOnCollisionBegin(
            [this](const vde::CollisionEvent& /*evt*/) { m_collisionCount++; });

        std::cout << "Physics initialized with " << getPhysicsScene()->getBodyCount() << " bodies"
                  << std::endl;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<PhysicsInputHandler*>(getInputHandler());
        if (input) {
            if (input->isSpacePressed()) {
                spawnSingleBox(0.0f, 8.0f);
            }
            if (input->isResetPressed()) {
                resetScene();
            }
        }

        // Sync visual sprites to physics bodies
        if (hasPhysics()) {
            for (auto& sprite : m_physicsSprites) {
                if (sprite) {
                    sprite->syncFromPhysics(getPhysicsScene());
                }
            }
        }

        // Print status periodically
        m_statusTimer += deltaTime;
        if (m_statusTimer >= 2.0f) {
            m_statusTimer = 0.0f;
            if (hasPhysics()) {
                std::cout << "[Physics] Bodies: " << getPhysicsScene()->getActiveBodyCount()
                          << " | Steps/frame: " << getPhysicsScene()->getLastStepCount()
                          << " | Collisions: " << m_collisionCount << std::endl;
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "Physics Simulation"; }

    std::vector<std::string> getFeatures() const override {
        return {"PhysicsScene with gravity", "Dynamic falling boxes",
                "Static ground platform",    "AABB collision detection",
                "Collision callbacks",       "Visual sprites synced to physics"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Dark background", "Green ground platform at bottom",
                "Colored boxes falling from above", "Boxes landing and stacking on the ground",
                "Console output showing body count and collisions"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Spawn a new box", "R     - Reset all boxes"};
    }

  private:
    std::vector<std::shared_ptr<PhysicsSprite>> m_physicsSprites;
    vde::PhysicsBodyId m_groundId = vde::INVALID_PHYSICS_BODY_ID;
    int m_collisionCount = 0;
    float m_statusTimer = 0.0f;

    void createGround() {
        // Physics body for ground
        vde::PhysicsBodyDef groundDef;
        groundDef.type = vde::PhysicsBodyType::Static;
        groundDef.shape = vde::PhysicsShape::Box;
        groundDef.position = {0.0f, -2.0f};
        groundDef.extents = {6.0f, 0.3f};
        m_groundId = getPhysicsScene()->createBody(groundDef);

        // Visual sprite for ground
        auto groundSprite = addEntity<PhysicsSprite>();
        groundSprite->setColor(vde::Color(0.2f, 0.7f, 0.3f, 1.0f));
        groundSprite->setScale(vde::Scale(12.0f, 0.6f, 1.0f));
        groundSprite->setPosition(vde::Position(0.0f, -2.0f, 0.0f));
    }

    void spawnBoxes() {
        // Spawn several boxes at different heights
        float positions[][2] = {
            {-1.5f, 5.0f}, {0.0f, 6.0f},  {1.5f, 5.5f}, {-0.5f, 7.0f},
            {0.5f, 8.0f},  {-1.0f, 9.0f}, {1.0f, 7.5f},
        };

        vde::Color colors[] = {
            vde::Color(0.9f, 0.3f, 0.3f, 1.0f),  // Red
            vde::Color(0.3f, 0.5f, 0.9f, 1.0f),  // Blue
            vde::Color(0.9f, 0.8f, 0.2f, 1.0f),  // Yellow
            vde::Color(0.9f, 0.5f, 0.1f, 1.0f),  // Orange
            vde::Color(0.6f, 0.2f, 0.8f, 1.0f),  // Purple
            vde::Color(0.2f, 0.8f, 0.8f, 1.0f),  // Cyan
            vde::Color(0.8f, 0.4f, 0.6f, 1.0f),  // Pink
        };

        for (int i = 0; i < 7; ++i) {
            spawnSingleBox(positions[i][0], positions[i][1], colors[i]);
        }
    }

    void spawnSingleBox(float x, float y, vde::Color color = vde::Color(0.7f, 0.7f, 0.7f, 1.0f)) {
        float halfSize = 0.3f;

        // Physics body
        vde::PhysicsBodyDef boxDef;
        boxDef.type = vde::PhysicsBodyType::Dynamic;
        boxDef.shape = vde::PhysicsShape::Box;
        boxDef.position = {x, y};
        boxDef.extents = {halfSize, halfSize};
        boxDef.mass = 1.0f;
        boxDef.restitution = 0.3f;
        boxDef.friction = 0.4f;
        boxDef.linearDamping = 0.01f;

        vde::PhysicsBodyId bodyId = getPhysicsScene()->createBody(boxDef);

        // Visual sprite
        auto sprite = addEntity<PhysicsSprite>();
        sprite->setColor(color);
        sprite->setScale(vde::Scale(halfSize * 2.0f, halfSize * 2.0f, 1.0f));
        sprite->setPosition(vde::Position(x, y, 0.0f));
        sprite->setPhysicsBodyId(bodyId);

        m_physicsSprites.push_back(sprite);
    }

    void resetScene() {
        // Remove all physics sprite entities
        for (auto& sprite : m_physicsSprites) {
            if (sprite && sprite->getPhysicsBodyId() != vde::INVALID_PHYSICS_BODY_ID) {
                if (hasPhysics()) {
                    getPhysicsScene()->destroyBody(sprite->getPhysicsBodyId());
                }
                removeEntity(sprite->getId());
            }
        }
        m_physicsSprites.clear();
        m_collisionCount = 0;

        // Respawn boxes
        spawnBoxes();
        std::cout << "[Physics] Scene reset" << std::endl;
    }
};

// ============================================================================
// Game
// ============================================================================

class PhysicsDemoGame
    : public vde::examples::BaseExampleGame<PhysicsInputHandler, PhysicsDemoScene> {
  public:
    PhysicsDemoGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    PhysicsDemoGame demo;
    return vde::examples::runExample(demo, "VDE Physics Demo", 1280, 720);
}
