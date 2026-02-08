/**
 * @file main.cpp
 * @brief 2D sidescroller game example using VDE.
 *
 * This example demonstrates:
 * - 2D sidescroller camera following the player
 * - Player movement and jumping with physics
 * - Platform collision detection
 * - Sprite animation using UV rectangles
 * - Background layers with simple parallax
 * - Enemy entities
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <iostream>

#include "../ExampleBase.h"

/**
 * @brief Simple 2D physics for platformer
 */
struct Physics2D {
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec2 acceleration{0.0f, 0.0f};
    float gravity = -15.0f;  // Negative Y is down
    bool onGround = false;

    void update(float deltaTime) {
        velocity += acceleration * deltaTime;
        velocity.y += gravity * deltaTime;

        // Terminal velocity
        velocity.y = std::max(velocity.y, -30.0f);

        acceleration = glm::vec2(0.0f, 0.0f);
    }

    void applyForce(const glm::vec2& force) { acceleration += force; }

    void jump(float power) {
        if (onGround) {
            velocity.y = power;
            onGround = false;
        }
    }
};

/**
 * @brief Animated sprite that cycles through frames
 */
class AnimatedSpriteEntity : public vde::SpriteEntity {
  public:
    void setAnimation(int frameCount, int framesPerRow, float frameTime) {
        m_frameCount = frameCount;
        m_framesPerRow = framesPerRow;
        m_frameTime = frameTime;
        m_frameWidth = 1.0f / framesPerRow;
        m_frameHeight = 1.0f / ((frameCount + framesPerRow - 1) / framesPerRow);
        setFrame(0);
    }

    void setFrame(int frame) {
        if (frame >= m_frameCount)
            frame = 0;
        m_currentFrame = frame;

        int col = frame % m_framesPerRow;
        int row = frame / m_framesPerRow;

        setUVRect(col * m_frameWidth, row * m_frameHeight, m_frameWidth, m_frameHeight);
    }

    void update(float deltaTime) override {
        if (!m_playing)
            return;

        m_animTime += deltaTime;
        if (m_animTime >= m_frameTime) {
            m_animTime -= m_frameTime;
            setFrame((m_currentFrame + 1) % m_frameCount);
        }
    }

    void play() { m_playing = true; }
    void pause() { m_playing = false; }
    void setFlipX(bool flip) { m_flipX = flip; }

  protected:
    int m_frameCount = 1;
    int m_framesPerRow = 1;
    int m_currentFrame = 0;
    float m_frameTime = 0.1f;
    float m_frameWidth = 1.0f;
    float m_frameHeight = 1.0f;
    float m_animTime = 0.0f;
    bool m_playing = false;
    bool m_flipX = false;  // Note: would need shader support
};

/**
 * @brief Player character entity
 */
class PlayerEntity : public AnimatedSpriteEntity {
  public:
    PlayerEntity() {
        setScale(1.0f, 1.0f, 1.0f);
        setAnchor(0.5f, 0.0f);                    // Bottom center
        setColor(vde::Color::fromHex(0x00d2d3));  // Cyan player

        // Simulate 4-frame walk animation (2x2 layout)
        setAnimation(4, 2, 0.15f);
        play();
    }

    void moveHorizontal(float direction, float speed) {
        m_physics.applyForce(glm::vec2(direction * speed, 0.0f));
        if (direction < 0)
            setFlipX(true);
        else if (direction > 0)
            setFlipX(false);
    }

    void jump(float power) { m_physics.jump(power); }

    void update(float deltaTime) override {
        AnimatedSpriteEntity::update(deltaTime);

        m_physics.update(deltaTime);

        // Apply velocity to position
        auto pos = getPosition();
        pos.x += m_physics.velocity.x * deltaTime;
        pos.y += m_physics.velocity.y * deltaTime;

        // Simple ground collision (Y = 0)
        if (pos.y <= 0.0f) {
            pos.y = 0.0f;
            m_physics.velocity.y = 0.0f;
            m_physics.onGround = true;
        }

        // Apply friction when on ground
        if (m_physics.onGround) {
            m_physics.velocity.x *= 0.85f;
        }

        setPosition(pos);
    }

    glm::vec2 getVelocity() const { return m_physics.velocity; }
    bool isOnGround() const { return m_physics.onGround; }

  private:
    Physics2D m_physics;
};

/**
 * @brief Platform entity
 */
class PlatformEntity : public vde::SpriteEntity {
  public:
    PlatformEntity(float x, float y, float width, float height) {
        setPosition(x, y + height / 2.0f, -0.1f);
        setScale(width, height, 1.0f);
        setColor(vde::Color::fromHex(0x6c5ce7));  // Purple platform
        setAnchor(0.5f, 0.5f);

        m_bounds = {x - width / 2.0f, x + width / 2.0f, y, y + height};
    }

    bool checkCollision(const vde::Position& pos, float size) const {
        return pos.x + size / 2.0f > m_bounds.minX && pos.x - size / 2.0f < m_bounds.maxX &&
               pos.y + size > m_bounds.minY && pos.y < m_bounds.maxY;
    }

    struct Bounds {
        float minX, maxX, minY, maxY;
    };

    Bounds getBounds() const { return m_bounds; }

  private:
    Bounds m_bounds;
};

/**
 * @brief Tiled background layer (manual tiling)
 */
class TiledBackground : public vde::Entity {
  public:
    TiledBackground(float tilesWide, float tilesHigh, float tileSize, vde::Color color,
                    float depth) {
        m_tilesWide = static_cast<int>(tilesWide);
        m_tilesHigh = static_cast<int>(tilesHigh);
        m_tileSize = tileSize;
        m_color = color;
        m_depth = depth;
    }

    void onAttach(vde::Scene* scene) override {
        vde::Entity::onAttach(scene);

        // Create individual sprite tiles
        // Note: This is inefficient - a proper tilemap system would be better
        for (int y = 0; y < m_tilesHigh; ++y) {
            for (int x = 0; x < m_tilesWide; ++x) {
                auto tile = std::make_shared<vde::SpriteEntity>();
                tile->setPosition(x * m_tileSize, y * m_tileSize, m_depth);
                tile->setScale(m_tileSize, m_tileSize, 1.0f);
                tile->setColor(m_color);
                tile->setAnchor(0.0f, 0.0f);
                m_tiles.push_back(tile);
                // Note: Would need to add to scene, but API doesn't support dynamic adding easily
            }
        }
    }

  private:
    int m_tilesWide = 0;
    int m_tilesHigh = 0;
    float m_tileSize = 1.0f;
    float m_depth = 0.0f;
    vde::Color m_color;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_tiles;
};

/**
 * @brief Enemy entity that patrols back and forth
 */
class EnemyEntity : public AnimatedSpriteEntity {
  public:
    EnemyEntity(float startX, float startY, float patrolDistance)
        : m_startX(startX), m_patrolDistance(patrolDistance) {
        setPosition(startX, startY, 0.0f);
        setScale(0.8f, 0.8f, 1.0f);
        setAnchor(0.5f, 0.0f);
        setColor(vde::Color::fromHex(0xff6348));  // Red enemy

        setAnimation(2, 2, 0.3f);
        play();
    }

    void update(float deltaTime) override {
        AnimatedSpriteEntity::update(deltaTime);

        // Simple patrol AI
        auto pos = getPosition();
        pos.x += m_direction * m_speed * deltaTime;

        if (std::abs(pos.x - m_startX) > m_patrolDistance) {
            m_direction *= -1.0f;
            setFlipX(m_direction < 0);
        }

        setPosition(pos);
    }

  private:
    float m_startX;
    float m_patrolDistance;
    float m_direction = 1.0f;
    float m_speed = 2.0f;
};

/**
 * @brief Input handler for the sidescroller
 */
class SidescrollerInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_A || key == vde::KEY_LEFT)
            m_moveLeft = true;
        if (key == vde::KEY_D || key == vde::KEY_RIGHT)
            m_moveRight = true;
        if (key == vde::KEY_SPACE || key == vde::KEY_W || key == vde::KEY_UP)
            m_jump = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_A || key == vde::KEY_LEFT)
            m_moveLeft = false;
        if (key == vde::KEY_D || key == vde::KEY_RIGHT)
            m_moveRight = false;
    }

    bool isMoveLeft() const { return m_moveLeft; }
    bool isMoveRight() const { return m_moveRight; }
    bool isJump() {
        bool val = m_jump;
        m_jump = false;
        return val;
    }

  private:
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_jump = false;
};

/**
 * @brief Main sidescroller game scene
 */
class SidescrollerScene : public vde::examples::BaseExampleScene {
  public:
    SidescrollerScene() : BaseExampleScene(60.0f) {}  // Run for 60 seconds

    void onEnter() override {
        printExampleHeader();

        // Create 2D camera
        m_camera = new vde::Camera2D(20.0f, 15.0f);  // 20x15 world units
        m_camera->setPosition(0.0f, 5.0f);
        setCamera(m_camera);

        // Set background color (sky blue)
        setBackgroundColor(vde::Color::fromHex(0x74b9ff));

        // Create background layers (simple parallax effect would be nice here)
        createBackground();

        // Create platforms
        createPlatforms();

        // Create player
        m_player = addEntity<PlayerEntity>();
        m_player->setName("Player");
        m_player->setPosition(0.0f, 5.0f, 0.0f);

        // Create enemies
        auto enemy1 = addEntity<EnemyEntity>(8.0f, 0.0f, 3.0f);
        enemy1->setName("Enemy1");

        auto enemy2 = addEntity<EnemyEntity>(15.0f, 3.0f, 2.0f);
        enemy2->setName("Enemy2");

        std::cout << "\n=== SIDESCROLLER GAME ===" << std::endl;
        std::cout << "A simple platformer example" << std::endl;
    }

    void createBackground() {
        // Far background layer
        for (int i = -2; i < 10; i++) {
            auto bg = addEntity<vde::SpriteEntity>();
            bg->setPosition(i * 5.0f, 5.0f, -0.9f);
            bg->setScale(5.0f, 10.0f, 1.0f);
            bg->setColor(vde::Color(0.6f, 0.7f, 0.8f, 0.3f));  // Light blue, transparent
            bg->setAnchor(0.0f, 0.0f);
        }

        // Ground tiles
        for (int i = -5; i < 20; i++) {
            auto ground = addEntity<vde::SpriteEntity>();
            ground->setPosition(i * 2.0f, -1.0f, -0.2f);
            ground->setScale(2.0f, 1.0f, 1.0f);
            ground->setColor(vde::Color::fromHex(0x6c5ce7));  // Purple
            ground->setAnchor(0.0f, 0.0f);
        }
    }

    void createPlatforms() {
        m_platforms.push_back(addEntity<PlatformEntity>(5.0f, 0.0f, 4.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(10.0f, 2.0f, 3.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(14.0f, 4.0f, 4.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(18.0f, 1.0f, 3.0f, 0.5f));
        m_platforms.push_back(addEntity<PlatformEntity>(22.0f, 3.0f, 4.0f, 0.5f));
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<SidescrollerInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Player movement
        float moveSpeed = 50.0f;
        if (input->isMoveLeft()) {
            m_player->moveHorizontal(-1.0f, moveSpeed);
        }
        if (input->isMoveRight()) {
            m_player->moveHorizontal(1.0f, moveSpeed);
        }
        if (input->isJump()) {
            m_player->jump(12.0f);
        }

        // Camera follows player with smoothing
        auto playerPos = m_player->getPosition();
        auto camPos = m_camera->getPosition();

        // Smooth camera following
        float cameraSpeed = 3.0f;
        camPos.x += (playerPos.x - camPos.x) * cameraSpeed * deltaTime;

        // Keep camera centered vertically around the action
        float targetY = std::max(5.0f, playerPos.y);
        camPos.y += (targetY - camPos.y) * cameraSpeed * deltaTime;

        m_camera->setPosition(camPos.x, camPos.y);

        // Simple platform collision (very basic)
        // A proper implementation would use better collision detection
        for (auto& platform : m_platforms) {
            auto pBounds = platform->getBounds();
            auto pPos = m_player->getPosition();
            auto pVel = m_player->getVelocity();

            // Very simple top collision only
            if (pVel.y < 0 && pPos.x > pBounds.minX && pPos.x < pBounds.maxX &&
                pPos.y > pBounds.minY && pPos.y < pBounds.maxY + 1.0f) {
                // Land on platform
                auto pos = m_player->getPosition();
                pos.y = pBounds.maxY;
                m_player->setPosition(pos);
            }
        }
    }

  protected:
    std::string getExampleName() const override { return "2D Sidescroller"; }

    std::vector<std::string> getFeatures() const override {
        return {"2D platformer mechanics",  "Player movement and jumping",
                "Simple physics (gravity)", "Platform collision",
                "Enemy AI (patrol)",        "Camera following"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Cyan player character", "Purple platforms at various heights",
                "Red patrolling enemies", "Blue sky background", "Camera follows player"};
    }

    std::vector<std::string> getControls() const override {
        return {"A/D or Arrow Keys - Move left/right", "Space/W/Up Arrow - Jump"};
    }

  private:
    std::shared_ptr<PlayerEntity> m_player;
    std::vector<std::shared_ptr<PlatformEntity>> m_platforms;
    vde::Camera2D* m_camera = nullptr;
};

/**
 * @brief Game class for the sidescroller
 */
class SidescrollerGame
    : public vde::examples::BaseExampleGame<SidescrollerInputHandler, SidescrollerScene> {};

/**
 * @brief Main entry point
 */
int main() {
    SidescrollerGame game;
    return vde::examples::runExample(game, "VDE 2D Sidescroller", 1280, 720);
}
