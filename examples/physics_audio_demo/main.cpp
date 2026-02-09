/**
 * @file main.cpp
 * @brief Physics + Audio demo demonstrating the full collision pipeline
 *
 * This example demonstrates:
 * - PhysicsScene with dynamic bodies falling and colliding
 * - Collision callbacks firing during physics step
 * - Phase callbacks (enablePhaseCallbacks): GameLogic → Audio → Visuals
 * - Game logic deciding outcome based on collision events
 * - Audio events queued from game logic, played during Audio phase
 * - Raycast to detect bodies below the player
 * - AABB query to highlight nearby bodies
 * - getEntityByPhysicsBody() to map collision events to game entities
 *
 * The demo spawns falling boxes onto a ground platform.  When boxes
 * collide, the game logic evaluates the collision and queues an
 * audio "click" (simulated via console output since we can't assume
 * audio assets exist).  A raycast is periodically fired downward
 * from a fixed point, and an AABB query highlights a region.
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "../ExampleBase.h"

// ============================================================================
// Input Handler
// ============================================================================

class PhysicsAudioInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_R)
            m_resetPressed = true;
        if (key == vde::KEY_Q)
            m_queryPressed = true;
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

    bool isQueryPressed() {
        bool val = m_queryPressed;
        m_queryPressed = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_resetPressed = false;
    bool m_queryPressed = false;
};

// ============================================================================
// Physics Audio Scene — uses phase callbacks
// ============================================================================

class PhysicsAudioScene : public vde::examples::BaseExampleScene {
  public:
    PhysicsAudioScene() : BaseExampleScene(15.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Enable phase callbacks for the 3-phase model
        enablePhaseCallbacks();

        // Enable physics
        vde::PhysicsConfig config;
        config.gravity = {0.0f, -9.81f};
        config.fixedTimestep = 1.0f / 60.0f;
        enablePhysics(config);

        // Camera
        auto* cam = new vde::OrbitCamera(vde::Position(0.0f, 3.0f, 0.0f), 12.0f);
        setCamera(std::unique_ptr<vde::GameCamera>(cam));

        // Lighting
        setLightBox(std::make_unique<vde::SimpleColorLightBox>(vde::Color::white()));
        setBackgroundColor(vde::Color(0.05f, 0.05f, 0.1f, 1.0f));

        // Create ground
        createGround();

        // Spawn initial boxes
        spawnBoxes();

        // Set up collision begin callback on the PhysicsScene
        getPhysicsScene()->setOnCollisionBegin([this](const vde::CollisionEvent& evt) {
            // Record the collision for processing in GameLogic phase
            m_pendingCollisions.push_back(evt);
        });

        // Set up collision end callback
        getPhysicsScene()->setOnCollisionEnd([this](const vde::CollisionEvent& evt) {
            m_collisionEndCount++;
            (void)evt;
        });

        // Perform initial raycast status
        std::cout << "[PhysicsAudioScene] Phase callbacks enabled (3-phase model)" << std::endl;
        std::cout << "[PhysicsAudioScene] Collision pipeline: Physics -> GameLogic -> Audio"
                  << std::endl;
    }

    // -----------------------------------------------------------------
    // Phase 1: Game Logic — process collisions, decide outcomes
    // -----------------------------------------------------------------
    void updateGameLogic(float deltaTime) override {
        m_elapsedTime += deltaTime;

        // Check for input
        auto* input = dynamic_cast<PhysicsAudioInputHandler*>(getInputHandler());
        if (input) {
            if (input->isFailPressed()) {
                handleTestFailure();
                return;
            }
            if (input->isEscapePressed()) {
                handleEarlyExit();
                return;
            }
            if (input->isSpacePressed()) {
                spawnSingleBox((static_cast<float>(rand()) / RAND_MAX - 0.5f) * 6.0f,
                               8.0f + static_cast<float>(rand()) / RAND_MAX * 4.0f);
                std::cout << "[GameLogic] Spawned extra box" << std::endl;
            }
            if (input->isResetPressed()) {
                resetBoxes();
            }
            if (input->isQueryPressed()) {
                performAABBQuery();
            }
        }

        // Auto-terminate
        if (m_elapsedTime >= m_autoTerminateSeconds) {
            handleTestSuccess();
            return;
        }

        // Process pending collisions from the physics step
        for (const auto& evt : m_pendingCollisions) {
            processCollision(evt);
        }
        m_pendingCollisions.clear();

        // Periodic raycast (every 2 seconds)
        m_raycastTimer += deltaTime;
        if (m_raycastTimer >= 2.0f) {
            m_raycastTimer = 0.0f;
            performRaycast();
        }

        // Periodic AABB query (every 3 seconds)
        m_queryTimer += deltaTime;
        if (m_queryTimer >= 3.0f) {
            m_queryTimer = 0.0f;
            performAABBQuery();
        }

        // Periodic status output
        m_statusTimer += deltaTime;
        if (m_statusTimer >= 4.0f) {
            m_statusTimer = 0.0f;
            printStatus();
        }
    }

    // -----------------------------------------------------------------
    // Phase 2: Audio — drain the event queue
    // -----------------------------------------------------------------
    void updateAudio(float deltaTime) override {
        // The base Scene::updateAudio drains the audio event queue via AudioManager.
        // We call it but also count how many events we processed.
        size_t queueSize = getAudioEventQueueSize();
        if (queueSize > 0) {
            m_totalAudioEventsProcessed += queueSize;
        }

        Scene::updateAudio(deltaTime);
    }

    // -----------------------------------------------------------------
    // Phase 3: Visuals — update visual feedback
    // -----------------------------------------------------------------
    void updateVisuals([[maybe_unused]] float deltaTime) override {
        // Flash collision indicator sprites, update UI, etc.
        // For this demo we keep it simple — visual updates are handled
        // by the physics entity auto-sync.
    }

  protected:
    std::string getExampleName() const override { return "Physics + Audio Pipeline"; }

    std::vector<std::string> getFeatures() const override {
        return {"Phase callbacks (GameLogic -> Audio -> Visuals)",
                "Collision begin/end callbacks",
                "Per-body collision callbacks",
                "Raycast queries",
                "AABB spatial queries",
                "getEntityByPhysicsBody() entity lookup",
                "Audio event queue (collision -> game logic -> audio)"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Dark background with falling colored boxes",
                "Boxes landing and stacking on a green ground platform",
                "Console output showing collision events being processed",
                "Console output showing raycast hits and AABB query results"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Spawn an extra box", "R     - Reset all boxes",
                "Q     - Manual AABB query"};
    }

  private:
    // Collision tracking
    std::vector<vde::CollisionEvent> m_pendingCollisions;
    size_t m_totalCollisions = 0;
    size_t m_collisionEndCount = 0;
    size_t m_totalAudioEventsProcessed = 0;

    // Timers
    float m_raycastTimer = 0.0f;
    float m_queryTimer = 0.0f;
    float m_statusTimer = 0.0f;

    // Physics entities
    std::vector<std::shared_ptr<vde::PhysicsSpriteEntity>> m_boxes;
    std::shared_ptr<vde::PhysicsSpriteEntity> m_ground;

    void createGround() {
        m_ground = addEntity<vde::PhysicsSpriteEntity>();
        m_ground->setName("Ground");
        m_ground->setColor(vde::Color(0.2f, 0.7f, 0.3f, 1.0f));
        m_ground->setScale(vde::Scale(12.0f, 0.5f, 1.0f));

        vde::PhysicsBodyDef groundDef;
        groundDef.type = vde::PhysicsBodyType::Static;
        groundDef.shape = vde::PhysicsShape::Box;
        groundDef.position = {0.0f, -2.0f};
        groundDef.extents = {6.0f, 0.25f};
        m_ground->createPhysicsBody(groundDef);

        // Per-body callback on the ground
        getPhysicsScene()->setBodyOnCollisionBegin(
            m_ground->getPhysicsBodyId(), [this](const vde::CollisionEvent& evt) {
                // When something hits the ground, queue a "thud" audio event
                m_groundHitCount++;

                // Look up the other entity
                vde::PhysicsBodyId otherId =
                    (evt.bodyA == m_ground->getPhysicsBodyId()) ? evt.bodyB : evt.bodyA;
                vde::Entity* otherEntity = getEntityByPhysicsBody(otherId);
                if (otherEntity) {
                    std::cout << "[PerBodyCB] Entity '" << otherEntity->getName()
                              << "' hit ground (depth=" << std::fixed << std::setprecision(3)
                              << evt.depth << ")" << std::endl;
                }
            });
    }

    void spawnBoxes() {
        float positions[][2] = {
            {-2.0f, 5.0f}, {-0.5f, 6.5f}, {1.0f, 5.5f}, {-1.5f, 8.0f}, {0.5f, 7.0f}, {2.0f, 9.0f},
        };

        for (int i = 0; i < 6; ++i) {
            spawnSingleBox(positions[i][0], positions[i][1]);
        }
    }

    void spawnSingleBox(float x, float y) {
        float halfSize = 0.2f + static_cast<float>(rand()) / RAND_MAX * 0.2f;

        // Random warm color
        float r = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
        float g = 0.2f + static_cast<float>(rand()) / RAND_MAX * 0.4f;
        float b = 0.1f + static_cast<float>(rand()) / RAND_MAX * 0.2f;

        auto sprite = addEntity<vde::PhysicsSpriteEntity>();
        sprite->setColor(vde::Color(r, g, b, 1.0f));
        sprite->setScale(vde::Scale(halfSize * 2.0f, halfSize * 2.0f, 1.0f));
        sprite->setName("Box_" + std::to_string(m_boxes.size()));

        vde::PhysicsBodyDef boxDef;
        boxDef.type = vde::PhysicsBodyType::Dynamic;
        boxDef.shape = vde::PhysicsShape::Box;
        boxDef.position = {x, y};
        boxDef.extents = {halfSize, halfSize};
        boxDef.mass = 1.0f;
        boxDef.restitution = 0.3f;
        boxDef.friction = 0.4f;
        boxDef.linearDamping = 0.02f;
        sprite->createPhysicsBody(boxDef);

        m_boxes.push_back(sprite);
    }

    void resetBoxes() {
        for (auto& box : m_boxes) {
            if (box) {
                removeEntity(box->getId());
            }
        }
        m_boxes.clear();
        m_totalCollisions = 0;
        m_collisionEndCount = 0;
        m_groundHitCount = 0;
        m_totalAudioEventsProcessed = 0;
        spawnBoxes();
        std::cout << "[GameLogic] Reset all boxes" << std::endl;
    }

    void processCollision(const vde::CollisionEvent& evt) {
        m_totalCollisions++;

        // Look up entities involved in the collision
        vde::Entity* entityA = getEntityByPhysicsBody(evt.bodyA);
        vde::Entity* entityB = getEntityByPhysicsBody(evt.bodyB);

        // Game logic decision: if collision is strong enough, queue audio
        if (evt.depth > 0.02f) {
            // Queue an audio event (PlaySFX) — the audio phase will process it.
            // Since we may not have actual audio assets, we simulate with the queue.
            vde::AudioEvent audioEvt;
            audioEvt.type = vde::AudioEventType::PlaySFX;
            audioEvt.volume = std::min(evt.depth * 5.0f, 1.0f);  // Louder for deeper collisions
            audioEvt.pitch = 0.8f + static_cast<float>(rand()) / RAND_MAX * 0.4f;
            queueAudioEvent(audioEvt);
        }

        // Log notable collisions (throttle to avoid spam)
        if (m_totalCollisions % 10 == 1) {
            std::string nameA =
                entityA ? entityA->getName() : ("body#" + std::to_string(evt.bodyA));
            std::string nameB =
                entityB ? entityB->getName() : ("body#" + std::to_string(evt.bodyB));
            std::cout << "[GameLogic] Collision #" << m_totalCollisions << ": " << nameA << " <-> "
                      << nameB << " (depth=" << std::fixed << std::setprecision(3) << evt.depth
                      << ", audio queued)" << std::endl;
        }
    }

    void performRaycast() {
        if (!getPhysicsScene())
            return;

        // Cast a ray downward from above the scene
        glm::vec2 origin = {0.0f, 10.0f};
        glm::vec2 direction = {0.0f, -1.0f};

        vde::RaycastHit hit;
        bool result = getPhysicsScene()->raycast(origin, direction, 20.0f, hit);

        if (result) {
            vde::Entity* hitEntity = getEntityByPhysicsBody(hit.bodyId);
            std::string name =
                hitEntity ? hitEntity->getName() : ("body#" + std::to_string(hit.bodyId));
            std::cout << "[Raycast] Hit '" << name << "' at y=" << std::fixed
                      << std::setprecision(2) << hit.point.y << " (dist=" << hit.distance << ")"
                      << std::endl;
        } else {
            std::cout << "[Raycast] No hit (clear sky)" << std::endl;
        }
    }

    void performAABBQuery() {
        if (!getPhysicsScene())
            return;

        // Query a region in the center of the scene
        glm::vec2 queryMin = {-2.0f, -3.0f};
        glm::vec2 queryMax = {2.0f, 3.0f};

        auto bodies = getPhysicsScene()->queryAABB(queryMin, queryMax);

        std::cout << "[AABB Query] Region (-2,-3)-(2,3): " << bodies.size() << " bodies found";
        if (!bodies.empty()) {
            std::cout << " [";
            for (size_t i = 0; i < bodies.size() && i < 5; ++i) {
                if (i > 0)
                    std::cout << ", ";
                vde::Entity* entity = getEntityByPhysicsBody(bodies[i]);
                if (entity) {
                    std::cout << entity->getName();
                } else {
                    std::cout << "body#" << bodies[i];
                }
            }
            if (bodies.size() > 5)
                std::cout << ", ...";
            std::cout << "]";
        }
        std::cout << std::endl;
    }

    void printStatus() {
        std::cout << "\n--- Status ---" << std::endl;
        std::cout << "  Boxes: " << m_boxes.size() << std::endl;
        std::cout << "  Total collisions (begin): " << m_totalCollisions << std::endl;
        std::cout << "  Collision ends: " << m_collisionEndCount << std::endl;
        std::cout << "  Ground hits (per-body CB): " << m_groundHitCount << std::endl;
        std::cout << "  Audio events processed: " << m_totalAudioEventsProcessed << std::endl;
        std::cout << "  Physics bodies: " << getPhysicsScene()->getActiveBodyCount() << std::endl;
        std::cout << "--------------\n" << std::endl;
    }

    int m_groundHitCount = 0;
};

// ============================================================================
// Game
// ============================================================================

class PhysicsAudioGame
    : public vde::examples::BaseExampleGame<PhysicsAudioInputHandler, PhysicsAudioScene> {
  public:
    PhysicsAudioGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    PhysicsAudioGame game;
    return vde::examples::runExample(game, "VDE Physics + Audio Pipeline Demo", 1280, 720);
}
