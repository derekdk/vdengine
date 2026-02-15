/**
 * @file main.cpp
 * @brief Physics demo example demonstrating VDE PhysicsEntity
 *
 * This example demonstrates:
 * - PhysicsSpriteEntity binding visual sprites to physics bodies
 * - Automatic PostPhysics transform sync with interpolation
 * - Dynamic bodies (boxes) falling under gravity
 * - Static ground platform
 * - AABB collision detection and resolution
 * - Collision callbacks
 * - Player entity with keyboard input (applyForce / applyImpulse)
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
        if (key == vde::KEY_LEFT)
            m_leftHeld = true;
        if (key == vde::KEY_RIGHT)
            m_rightHeld = true;
        if (key == vde::KEY_UP)
            m_jumpPressed = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT)
            m_leftHeld = false;
        if (key == vde::KEY_RIGHT)
            m_rightHeld = false;
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

    bool isLeftHeld() const { return m_leftHeld; }
    bool isRightHeld() const { return m_rightHeld; }

    bool isJumpPressed() {
        bool val = m_jumpPressed;
        m_jumpPressed = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_resetPressed = false;
    bool m_leftHeld = false;
    bool m_rightHeld = false;
    bool m_jumpPressed = false;
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

        // Create player-controlled entity
        createPlayer();

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

            // Player movement via forces
            if (m_player) {
                const float moveForce = 30.0f;
                if (input->isLeftHeld()) {
                    m_player->applyForce(glm::vec2(-moveForce, 0.0f));
                }
                if (input->isRightHeld()) {
                    m_player->applyForce(glm::vec2(moveForce, 0.0f));
                }
                if (input->isJumpPressed()) {
                    m_player->applyImpulse(glm::vec2(0.0f, 5.0f));
                }
            }
        }

        // NOTE: No manual sync needed — the PostPhysics scheduler task
        // automatically calls syncFromPhysics() on all PhysicsSpriteEntity
        // instances that have autoSync enabled.

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
    std::string getExampleName() const override { return "Physics Entities"; }

    std::vector<std::string> getFeatures() const override {
        return {"PhysicsSpriteEntity with auto-sync",
                "Interpolated transform from physics",
                "Player with applyForce/applyImpulse",
                "Dynamic falling boxes",
                "Static ground platform",
                "AABB collision detection",
                "Collision callbacks"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Dark background",
                "Green ground platform at bottom",
                "Colored boxes falling from above",
                "Boxes landing and stacking on the ground",
                "Cyan player box controllable with arrows",
                "Console output showing body count and collisions"};
    }

    std::vector<std::string> getControls() const override {
        return {"LEFT/RIGHT - Move player", "UP    - Jump", "SPACE      - Spawn a new box",
                "R          - Reset all boxes"};
    }

  private:
    std::vector<std::shared_ptr<vde::PhysicsSpriteEntity>> m_physicsSprites;
    std::shared_ptr<vde::PhysicsSpriteEntity> m_player;
    int m_collisionCount = 0;
    float m_statusTimer = 0.0f;

    void createGround() {
        // Ground uses PhysicsSpriteEntity too (static body, auto-sync is harmless)
        auto ground = addEntity<vde::PhysicsSpriteEntity>();
        ground->setColor(vde::Color(0.2f, 0.7f, 0.3f, 1.0f));
        ground->setScale(vde::Scale(12.0f, 0.6f, 1.0f));

        vde::PhysicsBodyDef groundDef;
        groundDef.type = vde::PhysicsBodyType::Static;
        groundDef.shape = vde::PhysicsShape::Box;
        groundDef.position = {0.0f, -2.0f};
        groundDef.extents = {6.0f, 0.3f};
        ground->createPhysicsBody(groundDef);
    }

    void createPlayer() {
        m_player = addEntity<vde::PhysicsSpriteEntity>();
        m_player->setColor(vde::Color(0.2f, 0.9f, 0.9f, 1.0f));
        m_player->setScale(vde::Scale(0.7f, 0.7f, 1.0f));

        vde::PhysicsBodyDef def;
        def.type = vde::PhysicsBodyType::Dynamic;
        def.shape = vde::PhysicsShape::Box;
        def.position = {-3.0f, 0.0f};
        def.extents = {0.35f, 0.35f};
        def.mass = 1.0f;
        def.restitution = 0.1f;
        def.friction = 0.5f;
        def.linearDamping = 0.05f;
        m_player->createPhysicsBody(def);
    }

    void spawnBoxes() {
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

        auto sprite = addEntity<vde::PhysicsSpriteEntity>();
        sprite->setColor(color);
        sprite->setScale(vde::Scale(halfSize * 2.0f, halfSize * 2.0f, 1.0f));

        vde::PhysicsBodyDef boxDef;
        boxDef.type = vde::PhysicsBodyType::Dynamic;
        boxDef.shape = vde::PhysicsShape::Box;
        boxDef.position = {x, y};
        boxDef.extents = {halfSize, halfSize};
        boxDef.mass = 1.0f;
        boxDef.restitution = 0.3f;
        boxDef.friction = 0.4f;
        boxDef.linearDamping = 0.01f;
        sprite->createPhysicsBody(boxDef);

        m_physicsSprites.push_back(sprite);
    }

    void resetScene() {
        // Remove all dynamic physics sprite entities
        for (auto& sprite : m_physicsSprites) {
            if (sprite) {
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

int main(int argc, char** argv) {
    PhysicsDemoGame demo;
    return vde::examples::runExample(demo, "VDE Physics Demo", 1280, 720, argc, argv);
}
