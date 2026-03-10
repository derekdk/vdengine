/**
 * @file main.cpp
 * @brief Vertical Shooter demo showcasing the VDE gameplay utility library.
 *
 * This example demonstrates how short and clear game code can be with
 * the Math2D, Timing, Random, and WorldBounds2D utility types.
 *
 * Screens:
 * - Title screen  (press SPACE to start)
 * - Gameplay      (survive and shoot enemies)
 * - Game Over     (press R to restart)
 */

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Constants
// ============================================================================

static constexpr float WORLD_W = 10.0f;
static constexpr float WORLD_H = 14.0f;
static constexpr float HALF_W = WORLD_W * 0.5f;
static constexpr float HALF_H = WORLD_H * 0.5f;

static constexpr float PLAYER_SPEED = 8.0f;
static constexpr float BULLET_SPEED = 14.0f;
static constexpr float ENEMY_BASE_SPEED = 3.0f;
static constexpr float FIRE_COOLDOWN = 0.18f;
static constexpr float ENEMY_SPAWN_INTERVAL = 0.8f;

// ============================================================================
// Input handler
// ============================================================================

class ShooterInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = true;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = true;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_up = true;
        if (key == vde::KEY_DOWN || key == vde::KEY_S)
            m_down = true;
        if (key == vde::KEY_SPACE) {
            m_fireHeld = true;
            m_firePressed = true;
        }
        if (key == vde::KEY_R)
            m_restart = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = false;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = false;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_up = false;
        if (key == vde::KEY_DOWN || key == vde::KEY_S)
            m_down = false;
        if (key == vde::KEY_SPACE)
            m_fireHeld = false;
    }

    bool isLeft() const { return m_left; }
    bool isRight() const { return m_right; }
    bool isUp() const { return m_up; }
    bool isDown() const { return m_down; }
    bool isFireHeld() const { return m_fireHeld; }
    bool consumeFire() {
        bool v = m_firePressed;
        m_firePressed = false;
        return v;
    }
    bool consumeRestart() {
        bool v = m_restart;
        m_restart = false;
        return v;
    }

  private:
    bool m_left = false, m_right = false, m_up = false, m_down = false;
    bool m_fireHeld = false, m_firePressed = false, m_restart = false;
};

// ============================================================================
// Gameplay scene
// ============================================================================

class ShooterScene : public vde::examples::BaseExampleScene {
  public:
    ShooterScene() : BaseExampleScene(30.0f) {}

    void onEnter() override {
        printExampleHeader();

        // Camera
        auto* cam = new Camera2D(WORLD_W, WORLD_H);
        cam->setPosition(0.0f, 0.0f);
        cam->setZoom(1.0f);
        setCamera(cam);
        setBackgroundColor(Color::fromHex(0x0a0a2e));

        // Play area
        m_playArea = WorldBounds2D::fromCenter(0_m, 0_m, Meters(WORLD_W), Meters(WORLD_H));

        enterTitleScreen();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<ShooterInputHandler*>(getInputHandler());
        if (!input)
            return;

        switch (m_state) {
        case State::Title:
            updateTitle(input, deltaTime);
            break;
        case State::Playing:
            updatePlaying(input, deltaTime);
            break;
        case State::GameOver:
            updateGameOver(input, deltaTime);
            break;
        }
    }

  protected:
    std::string getExampleName() const override { return "Vertical Shooter"; }

    std::vector<std::string> getFeatures() const override {
        return {"Title screen -> Gameplay -> Game Over flow",
                "Uses Math2D, Timing, Random, WorldBounds2D utilities",
                "Deterministic enemy spawning with RandomStream",
                "Cooldown-driven fire rate and spawn intervals",
                "AABB collision via WorldBounds2D::intersects()"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Cyan player ship at the bottom", "White bullets moving upward",
                "Colored enemies scrolling downward", "Score printed in console"};
    }

    std::vector<std::string> getControls() const override {
        return {"Arrow keys / WASD - Move", "SPACE - Fire (hold for auto-fire)",
                "R - Restart when game over", "F - Report failure, ESC - Exit"};
    }

  private:
    // -----------------------------------------------------------------
    // State machine
    // -----------------------------------------------------------------
    enum class State { Title, Playing, GameOver };
    State m_state = State::Title;

    // -----------------------------------------------------------------
    // Title screen
    // -----------------------------------------------------------------
    void enterTitleScreen() {
        clearGame();
        m_state = State::Title;

        // Title banner sprite
        m_titleBanner = addEntity<SpriteEntity>();
        m_titleBanner->setScale(4.0f, 1.0f, 1.0f);
        m_titleBanner->setPosition(0.0f, 2.0f, 0.0f);
        m_titleBanner->setAnchor(0.5f, 0.5f);
        m_titleBanner->setColor(Color::fromHex(0x00e5ff));

        // "Press SPACE" prompt
        m_promptSprite = addEntity<SpriteEntity>();
        m_promptSprite->setScale(2.5f, 0.4f, 1.0f);
        m_promptSprite->setPosition(0.0f, -1.5f, 0.0f);
        m_promptSprite->setAnchor(0.5f, 0.5f);
        m_promptSprite->setColor(Color::fromHex(0xffffff));

        std::cout << "=== VERTICAL SHOOTER ===" << std::endl;
        std::cout << "Press SPACE to start!" << std::endl;
    }

    void updateTitle(ShooterInputHandler* input, float deltaTime) {
        // Blink the prompt using RepeatingTimer
        int ticks = m_blinkTimer.advance(deltaTime);
        if (ticks > 0) {
            m_blinkVisible = !m_blinkVisible;
            if (m_promptSprite)
                m_promptSprite->setVisible(m_blinkVisible);
        }

        if (input->consumeFire() || input->consumeRestart()) {
            enterPlaying();
        }
    }

    // -----------------------------------------------------------------
    // Gameplay
    // -----------------------------------------------------------------
    void enterPlaying() {
        clearGame();
        m_state = State::Playing;
        m_score = 0;
        m_rng.reseed(42);
        m_fireCooldown = Cooldown(FIRE_COOLDOWN);
        m_fireCooldown.finish();
        m_spawnTimer = RepeatingTimer(ENEMY_SPAWN_INTERVAL);
        m_difficultyTimer = 0.0f;

        // Create player ship
        m_player = addEntity<SpriteEntity>();
        m_player->setScale(0.6f, 0.8f, 1.0f);
        m_player->setPosition(0.0f, -HALF_H + 2.0f, 0.0f);
        m_player->setAnchor(0.5f, 0.5f);
        m_player->setColor(Color::fromHex(0x00e5ff));

        std::cout << "Score: 0" << std::endl;
    }

    void updatePlaying(ShooterInputHandler* input, float deltaTime) {
        // Advance timers
        m_fireCooldown.advance(deltaTime);
        m_difficultyTimer += deltaTime;

        // Move player
        glm::vec2 moveDir(0.0f);
        if (input->isLeft())
            moveDir.x -= 1.0f;
        if (input->isRight())
            moveDir.x += 1.0f;
        if (input->isUp())
            moveDir.y += 1.0f;
        if (input->isDown())
            moveDir.y -= 1.0f;
        moveDir = math2d::normalizeOrZero(moveDir);

        if (m_player) {
            auto pos = m_player->getPosition();
            glm::vec2 pos2d(pos.x, pos.y);
            pos2d += moveDir * PLAYER_SPEED * deltaTime;
            pos2d = m_playArea.clampPoint(pos2d);
            m_player->setPosition(pos2d.x, pos2d.y, 0.0f);
        }

        // Fire bullets
        if (input->isFireHeld() && m_fireCooldown.tryConsume()) {
            m_fireCooldown.start();
            fireBullet();
        }

        // Spawn enemies
        int spawns = m_spawnTimer.advance(deltaTime);
        for (int i = 0; i < spawns; ++i) {
            spawnEnemy();
        }

        // Update bullets
        for (int i = static_cast<int>(m_bullets.size()) - 1; i >= 0; --i) {
            auto& b = m_bullets[i];
            auto pos = b.entity->getPosition();
            pos.y += BULLET_SPEED * deltaTime;
            b.entity->setPosition(pos);
            if (pos.y > HALF_H + 1.0f) {
                removeEntity(b.entity->getId());
                m_bullets.erase(m_bullets.begin() + i);
            }
        }

        // Update enemies
        float speedMult = 1.0f + m_difficultyTimer * 0.02f;
        for (int i = static_cast<int>(m_enemies.size()) - 1; i >= 0; --i) {
            auto& e = m_enemies[i];
            auto pos = e.entity->getPosition();
            pos.y -= e.speed * speedMult * deltaTime;
            e.entity->setPosition(pos);
            if (pos.y < -HALF_H - 1.0f) {
                removeEntity(e.entity->getId());
                m_enemies.erase(m_enemies.begin() + i);
            }
        }

        // Collision: bullets vs enemies
        for (int b = static_cast<int>(m_bullets.size()) - 1; b >= 0; --b) {
            bool bulletHit = false;
            auto bPos = m_bullets[b].entity->getPosition();
            auto bBounds =
                WorldBounds2D::fromCenterSize(glm::vec2(bPos.x, bPos.y), glm::vec2(0.2f, 0.4f));

            for (int e = static_cast<int>(m_enemies.size()) - 1; e >= 0; --e) {
                auto ePos = m_enemies[e].entity->getPosition();
                auto eBounds =
                    WorldBounds2D::fromCenterSize(glm::vec2(ePos.x, ePos.y), glm::vec2(0.7f, 0.7f));

                if (bBounds.intersects(eBounds)) {
                    removeEntity(m_enemies[e].entity->getId());
                    m_enemies.erase(m_enemies.begin() + e);
                    bulletHit = true;
                    m_score += 10;
                    std::cout << "Score: " << m_score << std::endl;
                    break;
                }
            }
            if (bulletHit) {
                removeEntity(m_bullets[b].entity->getId());
                m_bullets.erase(m_bullets.begin() + b);
            }
        }

        // Collision: enemies vs player
        if (m_player) {
            auto pPos = m_player->getPosition();
            auto pBounds =
                WorldBounds2D::fromCenterSize(glm::vec2(pPos.x, pPos.y), glm::vec2(0.5f, 0.7f));
            for (auto& e : m_enemies) {
                auto ePos = e.entity->getPosition();
                auto eBounds =
                    WorldBounds2D::fromCenterSize(glm::vec2(ePos.x, ePos.y), glm::vec2(0.7f, 0.7f));
                if (pBounds.intersects(eBounds)) {
                    enterGameOver();
                    return;
                }
            }
        }
    }

    void fireBullet() {
        if (!m_player)
            return;
        auto pos = m_player->getPosition();

        auto bullet = addEntity<SpriteEntity>();
        bullet->setScale(0.15f, 0.4f, 1.0f);
        bullet->setPosition(pos.x, pos.y + 0.5f, 0.0f);
        bullet->setAnchor(0.5f, 0.5f);
        bullet->setColor(Color::white());

        m_bullets.push_back({bullet});
    }

    void spawnEnemy() {
        float x = m_rng.range(-HALF_W + 0.5f, HALF_W - 0.5f);
        float speed = ENEMY_BASE_SPEED + m_rng.range(0.0f, 2.0f);

        auto enemy = addEntity<SpriteEntity>();
        enemy->setScale(0.7f, 0.7f, 1.0f);
        enemy->setPosition(x, HALF_H + 0.5f, 0.0f);
        enemy->setAnchor(0.5f, 0.5f);

        // Random color per enemy
        auto color =
            Color(m_rng.range(0.5f, 1.0f), m_rng.range(0.2f, 0.6f), m_rng.range(0.2f, 0.6f));
        enemy->setColor(color);

        m_enemies.push_back({enemy, speed});
    }

    // -----------------------------------------------------------------
    // Game Over
    // -----------------------------------------------------------------
    void enterGameOver() {
        m_state = State::GameOver;
        if (m_player) {
            m_player->setColor(Color::red());
        }
        std::cout << "GAME OVER! Final Score: " << m_score << std::endl;
        std::cout << "Press R to restart." << std::endl;
    }

    void updateGameOver(ShooterInputHandler* input, float /*deltaTime*/) {
        if (input->consumeRestart() || input->consumeFire()) {
            enterPlaying();
        }
    }

    // -----------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------
    void clearGame() {
        if (m_player) {
            removeEntity(m_player->getId());
            m_player.reset();
        }
        for (auto& b : m_bullets)
            removeEntity(b.entity->getId());
        m_bullets.clear();
        for (auto& e : m_enemies)
            removeEntity(e.entity->getId());
        m_enemies.clear();
        if (m_titleBanner) {
            removeEntity(m_titleBanner->getId());
            m_titleBanner.reset();
        }
        if (m_promptSprite) {
            removeEntity(m_promptSprite->getId());
            m_promptSprite.reset();
        }
    }

    // -----------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------
    WorldBounds2D m_playArea;
    RandomStream m_rng{42};
    Cooldown m_fireCooldown{FIRE_COOLDOWN};
    RepeatingTimer m_spawnTimer{ENEMY_SPAWN_INTERVAL};
    RepeatingTimer m_blinkTimer{0.5f};
    bool m_blinkVisible = true;
    float m_difficultyTimer = 0.0f;
    int m_score = 0;

    std::shared_ptr<SpriteEntity> m_player;
    std::shared_ptr<SpriteEntity> m_titleBanner;
    std::shared_ptr<SpriteEntity> m_promptSprite;

    struct BulletData {
        std::shared_ptr<SpriteEntity> entity;
    };
    struct EnemyData {
        std::shared_ptr<SpriteEntity> entity;
        float speed = ENEMY_BASE_SPEED;
    };
    std::vector<BulletData> m_bullets;
    std::vector<EnemyData> m_enemies;
};

// ============================================================================
// Game class
// ============================================================================

class ShooterGame : public vde::examples::BaseExampleGame<ShooterInputHandler, ShooterScene> {
  public:
    ShooterGame() = default;
};

// ============================================================================
// Entry point
// ============================================================================

int main(int argc, char** argv) {
    ShooterGame game;
    return vde::examples::runExample(game, "VDE Vertical Shooter", 600, 840, argc, argv);
}
