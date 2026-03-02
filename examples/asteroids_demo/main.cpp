/**
 * @file main.cpp
 * @brief Asteroids clone demonstrating VDE SpriteEntity functionality and game logic.
 *
 * This example demonstrates:
 * - Creating and controlling a spaceship with rotation and thrust
 * - Asteroid spawning and movement
 * - Bullet firing and collision detection
 * - Wrap-around world boundaries (toroidal)
 * - Score system and game over conditions
 * - Sprite-based 2D gameplay
 */

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

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

    // Gamepad input
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

    void onGamepadAxis(int gamepadId, int axis, float value) override {
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
 * @brief Game object base class with position, velocity, and wrap-around logic.
 */
class GameObject : public SpriteEntity {
  public:
    GameObject(float worldWidth, float worldHeight)
        : m_worldWidth(worldWidth), m_worldHeight(worldHeight) {}

    void update(float deltaTime) override {
        // Update position based on velocity
        auto pos = getPosition();
        pos.x += m_velocity.x * deltaTime;
        pos.y += m_velocity.y * deltaTime;

        // Wrap around world boundaries (toroidal)
        if (pos.x < -m_worldWidth * 0.5f)
            pos.x += m_worldWidth;
        if (pos.x > m_worldWidth * 0.5f)
            pos.x -= m_worldWidth;
        if (pos.y < -m_worldHeight * 0.5f)
            pos.y += m_worldHeight;
        if (pos.y > m_worldHeight * 0.5f)
            pos.y -= m_worldHeight;

        setPosition(pos);
    }

    void setVelocity(const glm::vec2& velocity) { m_velocity = velocity; }
    const glm::vec2& getVelocity() const { return m_velocity; }

    void setAngularVelocity(float angularVelocity) { m_angularVelocity = angularVelocity; }
    float getAngularVelocity() const { return m_angularVelocity; }

    void applyThrust(float thrust, float deltaTime) {
        // Calculate forward direction based on current rotation
        float angle = glm::radians(getRotation().roll);
        glm::vec2 forward(std::sin(angle), std::cos(angle));

        // Apply thrust to velocity
        m_velocity += forward * thrust * deltaTime;
    }

    void applyRotation(float rotationSpeed, float deltaTime) {
        auto rot = getRotation();
        rot.roll += rotationSpeed * deltaTime;
        setRotation(rot);
    }

  protected:
    glm::vec2 m_velocity = {0.0f, 0.0f};
    float m_angularVelocity = 0.0f;
    float m_worldWidth, m_worldHeight;
};

/**
 * @brief Spaceship controlled by the player.
 */
class Spaceship : public GameObject {
  public:
    Spaceship(float worldWidth, float worldHeight) : GameObject(worldWidth, worldHeight) {}

    void update(float deltaTime) override {
        GameObject::update(deltaTime);

        // Apply drag to slow down over time
        m_velocity *= 0.99f;

        // Clamp maximum speed
        float speed = glm::length(m_velocity);
        if (speed > m_maxSpeed) {
            m_velocity = glm::normalize(m_velocity) * m_maxSpeed;
        }
    }

    void setThrusting(bool thrusting) {
        m_isThrusting = thrusting;
        // Visual feedback for thrusting
        if (thrusting) {
            setColor(Color::fromHex(0xff6b6b));  // Red when thrusting
        } else {
            setColor(Color::fromHex(0x00b894));  // Green normally
        }
    }

    bool isThrusting() const { return m_isThrusting; }

  private:
    bool m_isThrusting = false;
    float m_maxSpeed = 8.0f;
};

/**
 * @brief Asteroid that moves in a straight line.
 */
class Asteroid : public GameObject {
  public:
    Asteroid(float worldWidth, float worldHeight, float size)
        : GameObject(worldWidth, worldHeight), m_size(size) {
        // Random initial velocity
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> speedDist(1.0f, 3.0f);

        float angle = angleDist(gen);
        float speed = speedDist(gen);
        m_velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed;

        // Random rotation
        std::uniform_real_distribution<float> rotDist(-2.0f, 2.0f);
        m_angularVelocity = rotDist(gen);

        // Set size and color based on asteroid size
        setScale(size, size, 1.0f);
        if (size > 0.8f) {
            setColor(Color::fromHex(0x636e72));  // Large - dark gray
        } else if (size > 0.5f) {
            setColor(Color::fromHex(0x95a5a6));  // Medium - light gray
        } else {
            setColor(Color::fromHex(0xbdc3c7));  // Small - very light gray
        }
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);

        // Apply rotation
        applyRotation(m_angularVelocity, deltaTime);
    }

    float getSize() const { return m_size; }

  private:
    float m_size;
};

/**
 * @brief Bullet fired by the spaceship.
 */
class Bullet : public GameObject {
  public:
    Bullet(float worldWidth, float worldHeight)
        : GameObject(worldWidth, worldHeight), m_lifetime(0.0f) {}

    void update(float deltaTime) override {
        GameObject::update(deltaTime);

        m_lifetime += deltaTime;
        if (m_lifetime > m_maxLifetime) {
            // Bullet expires
            setVisible(false);
        }
    }

    void fire(const glm::vec3& position, const glm::vec2& direction, float speed) {
        setPosition(position);
        setVelocity(direction * speed);
        m_lifetime = 0.0f;
        setVisible(true);
    }

    bool isExpired() const { return m_lifetime >= m_maxLifetime; }

  private:
    float m_lifetime;
    float m_maxLifetime = 2.0f;
};

/**
 * @brief Main game scene for Asteroids.
 */
class AsteroidsScene : public vde::examples::BaseExampleScene {
  public:
    AsteroidsScene() : BaseExampleScene(60.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Set up 2D camera
        m_worldWidth = 16.0f;
        m_worldHeight = 12.0f;
        auto* camera = new Camera2D(m_worldWidth, m_worldHeight);
        camera->setPosition(0.0f, 0.0f);
        camera->setZoom(1.0f);
        setCamera(camera);

        setBackgroundColor(Color::fromHex(0x2c3e50));

        // Initialize game
        initializeGame();

        std::cout << "Destroy all asteroids to win! Avoid collisions!" << std::endl;
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

        // Update game objects
        updateSpaceship(deltaTime);
        updateAsteroids(deltaTime);
        updateBullets(deltaTime);

        // Check collisions
        checkCollisions();

        // Spawn pending asteroids
        for (auto& spawn : m_pendingSpawns) {
            spawnAsteroidAt(spawn.position, spawn.size);
        }
        m_pendingSpawns.clear();

        // Check win condition
        if (m_asteroids.empty()) {
            std::cout << "All asteroids destroyed! You win!" << std::endl;
            handleTestSuccess();
        }
    }

  protected:
    std::string getExampleName() const override { return "Asteroids Clone"; }

    std::vector<std::string> getFeatures() const override {
        return {"Spaceship control with rotation and thrust", "Asteroid spawning and movement",
                "Bullet firing and collision detection", "Wrap-around world boundaries",
                "Score system and game over conditions"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Green spaceship that can rotate and thrust", "Gray asteroids of different sizes",
                "White bullets fired from spaceship", "Score display in console"};
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
        m_asteroids.clear();
        m_bullets.clear();

        // Create spaceship
        m_spaceship = addEntity<Spaceship>(m_worldWidth, m_worldHeight);
        m_spaceship->setName("Spaceship");
        m_spaceship->setScale(0.6f, 0.8f, 1.0f);
        m_spaceship->setAnchor(0.5f, 0.5f);
        m_spaceship->setPosition(0.0f, 0.0f, 0.0f);
        m_spaceship->setColor(Color::fromHex(0x00b894));

        // Create asteroids
        spawnAsteroids(6);

        std::cout << "Score: " << m_score << std::endl;
    }

    void clearEntities() {
        if (m_spaceship) {
            removeEntity(m_spaceship->getId());
            m_spaceship.reset();
        }

        for (auto& asteroid : m_asteroids) {
            removeEntity(asteroid->getId());
        }
        m_asteroids.clear();

        for (auto& bullet : m_bullets) {
            removeEntity(bullet->getId());
        }
        m_bullets.clear();
    }

    void handleInput(AsteroidsInputHandler* input, float deltaTime) {
        if (!m_spaceship)
            return;

        // Rotation
        float rotationSpeed = 180.0f;  // degrees per second
        if (input->isLeft() || input->getLeftStickX() < -0.1f) {
            m_spaceship->applyRotation(-rotationSpeed, deltaTime);
        }
        if (input->isRight() || input->getLeftStickX() > 0.1f) {
            m_spaceship->applyRotation(rotationSpeed, deltaTime);
        }

        // Thrust
        bool thrusting = input->isThrust() || input->getLeftStickY() > 0.1f;
        m_spaceship->setThrusting(thrusting);
        if (thrusting) {
            m_spaceship->applyThrust(15.0f, deltaTime);
        }

        // Fire bullets
        if (input->isFirePressed()) {
            fireBullet();
        }
    }

    void updateSpaceship(float deltaTime) {
        if (m_spaceship) {
            m_spaceship->update(deltaTime);
        }
    }

    void updateAsteroids(float deltaTime) {
        for (auto& asteroid : m_asteroids) {
            asteroid->update(deltaTime);
        }
    }

    void updateBullets(float deltaTime) {
        // Update bullets and remove expired ones
        for (int i = static_cast<int>(m_bullets.size()) - 1; i >= 0; --i) {
            auto& bullet = m_bullets[i];
            bullet->update(deltaTime);
            if (bullet->isExpired()) {
                removeEntity(bullet->getId());
                m_bullets.erase(m_bullets.begin() + i);
            }
        }
    }

    void checkCollisions() {
        if (!m_spaceship)
            return;

        // Bullet vs Asteroid collisions
        for (int b = static_cast<int>(m_bullets.size()) - 1; b >= 0; --b) {
            auto& bullet = m_bullets[b];
            if (!bullet->isVisible())
                continue;

            for (int a = static_cast<int>(m_asteroids.size()) - 1; a >= 0; --a) {
                auto& asteroid = m_asteroids[a];

                if (aabbIntersect(bullet->getPosition(), bullet->getScale().x, bullet->getScale().y,
                                  asteroid->getPosition(), asteroid->getScale().x,
                                  asteroid->getScale().y)) {
                    // Hit! Remove bullet and asteroid
                    removeEntity(bullet->getId());
                    m_bullets.erase(m_bullets.begin() + b);

                    destroyAsteroid(a);
                    break;  // Bullet can only hit one asteroid
                }
            }
        }

        // Spaceship vs Asteroid collisions
        for (auto& asteroid : m_asteroids) {
            if (aabbIntersect(m_spaceship->getPosition(), m_spaceship->getScale().x,
                              m_spaceship->getScale().y, asteroid->getPosition(),
                              asteroid->getScale().x, asteroid->getScale().y)) {
                // Game over!
                gameOver();
                return;
            }
        }
    }

    void fireBullet() {
        // Find inactive bullet or create new one
        Bullet* bullet = nullptr;
        for (auto& b : m_bullets) {
            if (!b->isVisible()) {
                bullet = b.get();
                break;
            }
        }

        if (!bullet) {
            // Create new bullet
            auto newBullet = addEntity<Bullet>(m_worldWidth, m_worldHeight);
            newBullet->setName("Bullet");
            newBullet->setScale(0.1f, 0.1f, 1.0f);
            newBullet->setAnchor(0.5f, 0.5f);
            newBullet->setColor(Color::white());
            m_bullets.push_back(newBullet);
            bullet = newBullet.get();
        }

        // Fire bullet from spaceship position in its facing direction
        float angle = glm::radians(m_spaceship->getRotation().roll);
        glm::vec2 direction(std::sin(angle), std::cos(angle));
        bullet->fire(m_spaceship->getPosition().toVec3(), direction, 12.0f);
    }

    void destroyAsteroid(int index) {
        auto& asteroid = m_asteroids[index];
        float size = asteroid->getSize();

        // Add score based on size
        if (size > 0.8f) {
            m_score += 20;
        } else if (size > 0.5f) {
            m_score += 50;
        } else {
            m_score += 100;
        }

        // Create smaller asteroids if large enough
        if (size > 0.5f) {
            float newSize = size * 0.6f;
            m_pendingSpawns.push_back({asteroid->getPosition().toVec3(), newSize});
            m_pendingSpawns.push_back({asteroid->getPosition().toVec3(), newSize});
        }

        // Remove asteroid
        removeEntity(asteroid->getId());
        m_asteroids.erase(m_asteroids.begin() + index);

        std::cout << "Score: " << m_score << std::endl;
    }

    void spawnAsteroids(int count) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(-m_worldWidth * 0.4f, m_worldWidth * 0.4f);
        std::uniform_real_distribution<float> sizeDist(0.8f, 1.2f);

        for (int i = 0; i < count; ++i) {
            float x, y;
            // Avoid spawning too close to the center (where spaceship starts)
            do {
                x = posDist(gen);
                y = posDist(gen);
            } while (std::abs(x) < 3.0f && std::abs(y) < 3.0f);

            float size = sizeDist(gen);
            spawnAsteroidAt({x, y, 0.0f}, size);
        }
    }

    void spawnAsteroidAt(const glm::vec3& position, float size) {
        auto asteroid = addEntity<Asteroid>(m_worldWidth, m_worldHeight, size);
        asteroid->setName("Asteroid");
        asteroid->setAnchor(0.5f, 0.5f);
        asteroid->setPosition(position);
        m_asteroids.push_back(asteroid);
    }

    void gameOver() {
        m_gameOver = true;
        std::cout << "Game Over! Final Score: " << m_score << std::endl;
        std::cout << "Press R or Start to restart" << std::endl;
    }

    static bool aabbIntersect(const vde::Position& aPos, float aW, float aH,
                              const vde::Position& bPos, float bW, float bH) {
        float aHalfW = aW * 0.5f;
        float aHalfH = aH * 0.5f;
        float bHalfW = bW * 0.5f;
        float bHalfH = bH * 0.5f;

        return !(aPos.x + aHalfW < bPos.x - bHalfW || aPos.x - aHalfW > bPos.x + bHalfW ||
                 aPos.y + aHalfH < bPos.y - bHalfH || aPos.y - aHalfH > bPos.y + bHalfH);
    }

  private:
    struct PendingSpawn {
        glm::vec3 position;
        float size;
    };

    std::shared_ptr<Spaceship> m_spaceship;
    std::vector<std::shared_ptr<Asteroid>> m_asteroids;
    std::vector<std::shared_ptr<Bullet>> m_bullets;
    std::vector<PendingSpawn> m_pendingSpawns;

    float m_worldWidth, m_worldHeight;
    int m_score = 0;
    bool m_gameOver = false;
};

class AsteroidsGame : public vde::examples::BaseExampleGame<AsteroidsInputHandler, AsteroidsScene> {
};

int main(int argc, char** argv) {
    AsteroidsGame demo;
    return vde::examples::runExample(demo, "VDE Asteroids Demo", 1280, 720, argc, argv);
}