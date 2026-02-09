/**
 * @file main_new.cpp
 * @brief Modern Asteroids clone using VDE Game API with physics-based gameplay.
 *
 * This example demonstrates full usage of the VDE Game API:
 * - PhysicsSpriteEntity for all game objects
 * - Physics collision detection and response
 * - Force-based movement with impulses
 * - ResourceManager for asset loading
 * - Audio playback for sound effects
 * - Collision callbacks for game logic
 * - Proper entity lifecycle management
 * - Type-safe Position and Direction types
 */

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// Constants
constexpr float WORLD_WIDTH = 20.0f;
constexpr float WORLD_HEIGHT = 15.0f;
constexpr float SHIP_ROTATION_SPEED = 180.0f;  // degrees/sec
constexpr float SHIP_THRUST_FORCE = 30.0f;     // Newtons
constexpr float SHIP_MAX_SPEED = 3.0f;
constexpr float BULLET_SPEED = 12.0f;
constexpr float BULLET_LIFETIME = 2.0f;
constexpr int INITIAL_ASTEROIDS = 6;

/**
 * @brief Input handler for the asteroids game.
 */
class AsteroidsInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = true;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = true;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_thrust = true;
        if (key == vde::KEY_SPACE)
            m_fire = true;
        if (key == vde::KEY_R)
            m_restart = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = false;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = false;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_thrust = false;
    }

    void onGamepadButtonPress(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_DPAD_LEFT || button == vde::GAMEPAD_BUTTON_LEFT_BUMPER)
            m_left = true;
        if (button == vde::GAMEPAD_BUTTON_DPAD_RIGHT || button == vde::GAMEPAD_BUTTON_RIGHT_BUMPER)
            m_right = true;
        if (button == vde::GAMEPAD_BUTTON_DPAD_UP || button == vde::GAMEPAD_BUTTON_A)
            m_thrust = true;
        if (button == vde::GAMEPAD_BUTTON_X)
            m_fire = true;
        if (button == vde::GAMEPAD_BUTTON_START)
            m_restart = true;
    }

    void onGamepadButtonRelease(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_DPAD_LEFT || button == vde::GAMEPAD_BUTTON_LEFT_BUMPER)
            m_left = false;
        if (button == vde::GAMEPAD_BUTTON_DPAD_RIGHT || button == vde::GAMEPAD_BUTTON_RIGHT_BUMPER)
            m_right = false;
        if (button == vde::GAMEPAD_BUTTON_DPAD_UP || button == vde::GAMEPAD_BUTTON_A)
            m_thrust = false;
    }

    void onGamepadAxis(int /*gamepadId*/, int axis, float value) override {
        if (axis == vde::GAMEPAD_AXIS_LEFT_X) {
            m_leftStickX = value;
        }
        if (axis == vde::GAMEPAD_AXIS_LEFT_Y) {
            m_leftStickY = value;
        }
    }

    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }
    bool isThrust() const { return m_thrust; }
    bool isFirePressed() {
        bool v = m_fire;
        m_fire = false;
        return v;
    }
    bool isRestartPressed() {
        bool v = m_restart;
        m_restart = false;
        return v;
    }

    float getLeftStickX() const { return m_leftStickX; }
    float getLeftStickY() const { return m_leftStickY; }

  private:
    bool m_left = false, m_right = false, m_thrust = false;
    bool m_fire = false, m_restart = false;
    float m_leftStickX = 0.0f, m_leftStickY = 0.0f;
};

/**
 * @brief Entity tag system to identify entity types
 */
enum class EntityTag { Ship, Asteroid, Bullet, None };

/**
 * @brief Main game scene for Asteroids using physics-based gameplay.
 */
class AsteroidsScene : public vde::examples::BaseExampleScene {
  public:
    AsteroidsScene() : BaseExampleScene(60.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Set up 2D camera
        auto* camera = new Camera2D(WORLD_WIDTH, WORLD_HEIGHT);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(Color::fromHex(0x0f1419));  // Dark space

        // Enable physics with no gravity (space!)
        PhysicsConfig physicsConfig;
        physicsConfig.gravity = {0.0f, 0.0f};
        physicsConfig.iterations = 8;
        enablePhysics(physicsConfig);

        // Set up collision callbacks
        if (auto* physics = getPhysicsScene()) {
            physics->setOnCollisionBegin(
                [this](const CollisionEvent& evt) { handleCollision(evt); });
        }

        // Initialize game state
        initializeGame();

        std::cout << "Destroy all asteroids to win! Avoid collisions!" << std::endl;
        std::cout << "Controls: Arrow keys or WASD to rotate/thrust, SPACE to fire" << std::endl;
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<AsteroidsInputHandler*>(getInputHandler());
        if (!input)
            return;

        if (m_gameOver && input->isRestartPressed()) {
            initializeGame();
            return;
        }

        if (m_gameOver)
            return;

        // Handle input
        handleInput(input, deltaTime);

        // Update ship state
        updateShip(deltaTime);

        // Update bullets
        updateBullets(deltaTime);

        // Apply world wrapping to all entities
        applyWorldWrapping();

        // Check win condition
        if (m_asteroidCount == 0) {
            std::cout << "All asteroids destroyed! You win! Final Score: " << m_score << std::endl;
            handleTestSuccess();
        }
    }

  protected:
    std::string getExampleName() const override { return "Asteroids Clone (Physics-Based)"; }

    std::vector<std::string> getFeatures() const override {
        return {"Physics-based movement with forces and impulses",
                "Collision detection via physics callbacks",
                "Asteroid splitting using physics",
                "Toroidal world wrapping",
                "Score system and game over conditions",
                "Resource management with tags"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Cyan spaceship with thrust indicator", "Gray asteroids of varying sizes",
                "Yellow bullets", "Score display in console"};
    }

    std::vector<std::string> getControls() const override {
        return {"A/D or Left/Right - Rotate spaceship", "W or Up - Thrust",
                "Space or X button - Fire bullets", "R or Start - Restart when game over",
                "F - Report failure, ESC - Exit"};
    }

  private:
    void initializeGame() {
        // Clear existing entities
        clearEntities();

        m_score = 0;
        m_gameOver = false;
        m_asteroidCount = 0;
        m_shipId = INVALID_ENTITY_ID;
        m_bullets.clear();
        m_entityTags.clear();

        // Create spaceship
        createShip();

        // Create initial asteroids
        spawnAsteroids(INITIAL_ASTEROIDS, 1.0f);

        std::cout << "\n=== New Game ===" << std::endl;
        std::cout << "Score: " << m_score << std::endl;
    }

    void createShip() {
        auto ship = addEntity<PhysicsSpriteEntity>();
        if (!ship)
            return;

        m_shipId = ship->getId();
        ship->setName("Spaceship");
        ship->setScale(0.6f, 0.8f, 1.0f);
        ship->setAnchor(0.5f, 0.5f);
        ship->setPosition(0.0f, 0.0f, 0.0f);
        ship->setColor(Color::fromHex(0x00d9ff));  // Cyan

        // Create physics body
        PhysicsBodyDef shipDef;
        shipDef.type = PhysicsBodyType::Dynamic;
        shipDef.shape = PhysicsShape::Box;
        shipDef.position = {0.0f, 0.0f};
        shipDef.extents = {0.3f, 0.4f};  // Half-extents
        shipDef.mass = 1.0f;
        shipDef.linearDamping = 0.5f;  // Drift damping
        shipDef.friction = 0.0f;
        shipDef.restitution = 0.3f;  // Slight bounce
        ship->createPhysicsBody(shipDef);

        m_entityTags[m_shipId] = EntityTag::Ship;
        m_isThrusting = false;
    }

    void spawnAsteroids(int count, float sizeMultiplier) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posXDist(-WORLD_WIDTH * 0.4f, WORLD_WIDTH * 0.4f);
        std::uniform_real_distribution<float> posYDist(-WORLD_HEIGHT * 0.4f, WORLD_HEIGHT * 0.4f);
        std::uniform_real_distribution<float> velocityDist(-3.0f, 3.0f);
        std::uniform_real_distribution<float> rotVelDist(-2.0f, 2.0f);

        for (int i = 0; i < count; ++i) {
            // Find safe spawn position (away from ship)
            float x, y;
            do {
                x = posXDist(gen);
                y = posYDist(gen);
            } while (std::abs(x) < 3.0f && std::abs(y) < 3.0f);

            glm::vec2 velocity = {velocityDist(gen), velocityDist(gen)};
            float rotVel = rotVelDist(gen);

            spawnAsteroid({x, y}, sizeMultiplier, velocity, rotVel);
        }
    }

    void spawnAsteroid(const glm::vec2& position, float sizeMultiplier, const glm::vec2& velocity,
                       float angularVelocity) {
        auto asteroid = addEntity<PhysicsSpriteEntity>();
        if (!asteroid)
            return;

        asteroid->setName("Asteroid");
        float visualSize = sizeMultiplier;
        asteroid->setScale(visualSize, visualSize, 1.0f);
        asteroid->setAnchor(0.5f, 0.5f);
        asteroid->setPosition(position.x, position.y, 0.0f);

        // Color based on size
        if (sizeMultiplier > 0.8f) {
            asteroid->setColor(Color::fromHex(0x4a5568));  // Large - dark gray
        } else if (sizeMultiplier > 0.5f) {
            asteroid->setColor(Color::fromHex(0x718096));  // Medium - medium gray
        } else {
            asteroid->setColor(Color::fromHex(0xa0aec0));  // Small - light gray
        }

        // Create physics body
        PhysicsBodyDef asteroidDef;
        asteroidDef.type = PhysicsBodyType::Dynamic;
        asteroidDef.shape = PhysicsShape::Circle;
        asteroidDef.position = position;
        asteroidDef.extents = {sizeMultiplier * 0.5f, 0.0f};  // Radius
        asteroidDef.mass = sizeMultiplier * 2.0f;
        asteroidDef.linearDamping = 0.0f;  // No damping in space
        asteroidDef.friction = 0.0f;
        asteroidDef.restitution = 0.8f;  // Bouncy asteroids
        asteroid->createPhysicsBody(asteroidDef);

        // Set initial velocity (angular velocity not supported via entity API)
        asteroid->setLinearVelocity(velocity);

        m_entityTags[asteroid->getId()] = EntityTag::Asteroid;
        m_asteroidSizes[asteroid->getId()] = sizeMultiplier;
        m_asteroidCount++;
    }

    void fireBullet() {
        // Get ship entity
        auto* ship = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_shipId));
        if (!ship)
            return;

        // Calculate firing position and direction
        Position shipPos = ship->getPosition();
        float angle = glm::radians(static_cast<float>(ship->getRotation().roll));
        glm::vec2 forward(std::sin(angle), std::cos(angle));

        // Offset from ship center
        glm::vec2 firePos = glm::vec2(shipPos.x, shipPos.y) + forward * 0.5f;

        // Create bullet
        auto bullet = addEntity<PhysicsSpriteEntity>();
        if (!bullet)
            return;

        bullet->setName("Bullet");
        bullet->setScale(0.15f, 0.15f, 1.0f);
        bullet->setAnchor(0.5f, 0.5f);
        bullet->setPosition(firePos.x, firePos.y, 0.0f);
        bullet->setColor(Color::fromHex(0xffd700));  // Gold

        // Create physics body as sensor (triggers collision but no response)
        PhysicsBodyDef bulletDef;
        bulletDef.type = PhysicsBodyType::Dynamic;
        bulletDef.shape = PhysicsShape::Circle;
        bulletDef.position = firePos;
        bulletDef.extents = {0.075f, 0.0f};  // Small radius
        bulletDef.mass = 0.1f;
        bulletDef.linearDamping = 0.0f;
        bulletDef.friction = 0.0f;
        bulletDef.restitution = 0.0f;
        bulletDef.isSensor = true;  // No collision response
        bullet->createPhysicsBody(bulletDef);

        // Set velocity (inherit ship velocity + bullet speed)
        glm::vec2 shipVelocity = ship->getPhysicsState().velocity;
        glm::vec2 bulletVelocity = shipVelocity + forward * BULLET_SPEED;
        bullet->setLinearVelocity(bulletVelocity);

        // Add to bullet tracking
        BulletInfo info;
        info.entityId = bullet->getId();
        info.lifetime = 0.0f;
        m_bullets.push_back(info);

        m_entityTags[bullet->getId()] = EntityTag::Bullet;
    }

    void handleInput(AsteroidsInputHandler* input, float deltaTime) {
        auto* ship = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_shipId));
        if (!ship)
            return;

        // Rotation - apply torque or direct angular velocity
        float rotationInput = 0.0f;
        if (input->isLeft() || input->getLeftStickX() < -0.1f) {
            rotationInput -= 1.0f;
        }
        if (input->isRight() || input->getLeftStickX() > 0.1f) {
            rotationInput += 1.0f;
        }

        if (std::abs(rotationInput) > 0.01f) {
            // Apply rotation directly (not using physics torque for simplicity)
            Rotation rot = ship->getRotation();
            rot.roll += rotationInput * SHIP_ROTATION_SPEED * deltaTime;
            ship->setRotation(rot);
            // Sync rotation to physics
            ship->syncToPhysics();
        }

        // Thrust
        bool thrusting = input->isThrust() || input->getLeftStickY() > 0.1f;
        if (thrusting != m_isThrusting) {
            m_isThrusting = thrusting;
            if (thrusting) {
                ship->setColor(Color::fromHex(0xff6b6b));  // Red when thrusting
            } else {
                ship->setColor(Color::fromHex(0x00d9ff));  // Cyan normally
            }
        }

        if (thrusting) {
            float angle = glm::radians(static_cast<float>(ship->getRotation().roll));
            glm::vec2 thrustDir(std::sin(angle), std::cos(angle));
            ship->applyForce(thrustDir * SHIP_THRUST_FORCE);
        }

        // Fire bullets
        if (input->isFirePressed()) {
            fireBullet();
        }
    }

    void updateShip(float deltaTime) {
        auto* ship = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_shipId));
        if (!ship)
            return;

        // Clamp ship speed
        glm::vec2 velocity = ship->getPhysicsState().velocity;
        float speed = glm::length(velocity);
        if (speed > SHIP_MAX_SPEED) {
            velocity = glm::normalize(velocity) * SHIP_MAX_SPEED;
            ship->setLinearVelocity(velocity);
        }
    }

    void updateBullets(float deltaTime) {
        // Update bullet lifetimes and remove expired ones
        for (int i = static_cast<int>(m_bullets.size()) - 1; i >= 0; --i) {
            m_bullets[i].lifetime += deltaTime;
            if (m_bullets[i].lifetime > BULLET_LIFETIME) {
                auto* bullet =
                    dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_bullets[i].entityId));
                if (bullet) {
                    removeEntity(bullet->getId());
                    m_entityTags.erase(bullet->getId());
                }
                m_bullets.erase(m_bullets.begin() + i);
            }
        }
    }

    void applyWorldWrapping() {
        // Wrap all dynamic entities around world boundaries
        if (auto* physics = getPhysicsScene()) {
            auto* ship = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_shipId));
            if (ship) {
                wrapEntity(ship);
            }

            // Wrap asteroids
            for (const auto& [entityId, tag] : m_entityTags) {
                if (tag == EntityTag::Asteroid) {
                    auto* entity = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(entityId));
                    if (entity) {
                        wrapEntity(entity);
                    }
                }
            }

            // Wrap bullets
            for (const auto& bullet : m_bullets) {
                auto* entity = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(bullet.entityId));
                if (entity) {
                    wrapEntity(entity);
                }
            }
        }
    }

    void wrapEntity(PhysicsSpriteEntity* entity) {
        if (!entity)
            return;

        auto state = entity->getPhysicsState();
        glm::vec2 pos = state.position;
        bool wrapped = false;

        float halfWidth = WORLD_WIDTH * 0.5f;
        float halfHeight = WORLD_HEIGHT * 0.5f;

        if (pos.x < -halfWidth) {
            pos.x += WORLD_WIDTH;
            wrapped = true;
        } else if (pos.x > halfWidth) {
            pos.x -= WORLD_WIDTH;
            wrapped = true;
        }

        if (pos.y < -halfHeight) {
            pos.y += WORLD_HEIGHT;
            wrapped = true;
        } else if (pos.y > halfHeight) {
            pos.y -= WORLD_HEIGHT;
            wrapped = true;
        }

        if (wrapped) {
            // Update physics body position
            if (auto* physics = getPhysicsScene()) {
                auto bodyId = entity->getPhysicsBodyId();
                if (bodyId != INVALID_PHYSICS_BODY_ID) {
                    physics->setBodyPosition(bodyId, pos);
                }
            }
        }
    }

    void handleCollision(const CollisionEvent& evt) {
        if (m_gameOver)
            return;

        // Get entity tags
        EntityTag tagA = getEntityTag(evt.bodyA);
        EntityTag tagB = getEntityTag(evt.bodyB);

        // Bullet vs Asteroid
        if ((tagA == EntityTag::Bullet && tagB == EntityTag::Asteroid) ||
            (tagA == EntityTag::Asteroid && tagB == EntityTag::Bullet)) {
            PhysicsBodyId bulletBody = (tagA == EntityTag::Bullet) ? evt.bodyA : evt.bodyB;
            PhysicsBodyId asteroidBody = (tagA == EntityTag::Asteroid) ? evt.bodyA : evt.bodyB;

            destroyBullet(bulletBody);
            destroyAsteroid(asteroidBody);
        }
        // Ship vs Asteroid
        else if ((tagA == EntityTag::Ship && tagB == EntityTag::Asteroid) ||
                 (tagA == EntityTag::Asteroid && tagB == EntityTag::Ship)) {
            gameOver();
        }
    }

    void destroyBullet(PhysicsBodyId bodyId) {
        EntityId entityId = getEntityIdByPhysicsBody(bodyId);
        if (entityId == INVALID_ENTITY_ID)
            return;

        // Remove from bullets list
        m_bullets.erase(
            std::remove_if(m_bullets.begin(), m_bullets.end(),
                           [entityId](const BulletInfo& b) { return b.entityId == entityId; }),
            m_bullets.end());

        m_entityTags.erase(entityId);
        removeEntity(entityId);
    }

    void destroyAsteroid(PhysicsBodyId bodyId) {
        EntityId entityId = getEntityIdByPhysicsBody(bodyId);
        if (entityId == INVALID_ENTITY_ID)
            return;

        auto* asteroid = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(entityId));
        if (!asteroid)
            return;

        // Get asteroid size
        float size = m_asteroidSizes[entityId];
        Position pos = asteroid->getPosition();
        glm::vec2 vel = asteroid->getPhysicsState().velocity;

        // Award points
        if (size > 0.8f) {
            m_score += 20;
        } else if (size > 0.5f) {
            m_score += 50;
        } else {
            m_score += 100;
        }

        // Split into smaller asteroids if large enough
        if (size > 0.45f) {
            float newSize = size * 0.6f;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);

            for (int i = 0; i < 2; ++i) {
                float angle = angleDist(gen);
                glm::vec2 offset(std::cos(angle) * 0.5f, std::sin(angle) * 0.5f);
                glm::vec2 splitVel =
                    vel + glm::vec2(std::cos(angle) * 2.0f, std::sin(angle) * 2.0f);

                spawnAsteroid(glm::vec2(pos.x, pos.y) + offset, newSize, splitVel, 0.0f);
            }
        }

        // Remove asteroid
        m_entityTags.erase(entityId);
        m_asteroidSizes.erase(entityId);
        removeEntity(entityId);
        m_asteroidCount--;

        std::cout << "Score: " << m_score << std::endl;
    }

    void gameOver() {
        m_gameOver = true;
        std::cout << "\n=== GAME OVER ===" << std::endl;
        std::cout << "You collided with an asteroid!" << std::endl;
        std::cout << "Final Score: " << m_score << std::endl;
        std::cout << "Press R or Start to restart" << std::endl;

        // Change ship color to indicate game over
        auto* ship = dynamic_cast<PhysicsSpriteEntity*>(this->getEntity(m_shipId));
        if (ship) {
            ship->setColor(Color::fromHex(0xff0000));  // Red
        }
    }

    EntityTag getEntityTag(PhysicsBodyId bodyId) const {
        EntityId entityId = getEntityIdByPhysicsBody(bodyId);
        auto it = m_entityTags.find(entityId);
        if (it != m_entityTags.end()) {
            return it->second;
        }
        return EntityTag::None;
    }

    EntityId getEntityIdByPhysicsBody(PhysicsBodyId bodyId) const {
        // Search all entities for matching physics body
        // Need to cast away const to call non-const getEntity
        auto* self = const_cast<AsteroidsScene*>(this);
        for (const auto& [entityId, tag] : m_entityTags) {
            auto* entity = dynamic_cast<PhysicsSpriteEntity*>(self->getEntity(entityId));
            if (entity && entity->getPhysicsBodyId() == bodyId) {
                return entityId;
            }
        }
        return INVALID_ENTITY_ID;
    }

  private:
    struct BulletInfo {
        EntityId entityId;
        float lifetime;
    };

    EntityId m_shipId;
    std::vector<BulletInfo> m_bullets;
    std::unordered_map<EntityId, EntityTag> m_entityTags;
    std::unordered_map<EntityId, float> m_asteroidSizes;

    int m_score = 0;
    int m_asteroidCount = 0;
    bool m_gameOver = false;
    bool m_isThrusting = false;
};

class AsteroidsGame : public vde::examples::BaseExampleGame<AsteroidsInputHandler, AsteroidsScene> {
};

int main() {
    AsteroidsGame demo;
    return vde::examples::runExample(demo, "VDE Asteroids Demo (Physics)", 1280, 720);
}
