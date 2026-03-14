#pragma once

/**
 * @file GameScene.h
 * @brief Main game scene that ties together scrolling, player, enemies, and projectiles.
 */

#include <vde/api/GameAPI.h>

#include <memory>
#include <vector>

#include "../ExampleBase.h"
#include "Audio.h"
#include "Entities.h"
#include "Input.h"
#include "Map.h"
#include "Sprites.h"
#include "Types.h"

namespace shooter {

class GameScene : public vde::examples::BaseExampleScene {
  public:
    GameScene();

    void onEnter() override;
    void update(float deltaTime) override;

  protected:
    std::string getExampleName() const override { return "Vertical Shooter"; }

    std::vector<std::string> getFeatures() const override {
        return {"Gamepad / keyboard controls (left stick + face buttons)",
                "Three weapon types: Basic, Spread, Rapid (Q/E or bumpers to switch)",
                "16-screen procedurally generated scrolling map",
                "Four enemy types: Turret, Drone, Chaser, Tank",
                "Procedurally generated sprites and sound effects"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Cyan wedge spaceship at the bottom", "Starfield scrolling downward",
                "Colored enemies appearing from the top", "Projectiles of varying shapes"};
    }

    std::vector<std::string> getControls() const override {
        return {"Arrow/WASD or Left Stick - Move", "SPACE / A / RT - Fire",
                "Q/E or LB/RB - Switch weapon", "ENTER or START - Start game",
                "R or BACK - Restart after game over"};
    }

  private:
    // State machine
    enum class State { Title, Playing, GameOver, Victory };
    State m_state = State::Title;

    void enterTitle();
    void enterPlaying();
    void enterGameOver();
    void enterVictory();

    void updateTitle(float dt);
    void updatePlaying(float dt);
    void updateGameOver(float dt);

    // Scrolling
    float m_scrollY = 0.0f;

    // Player
    std::shared_ptr<vde::SpriteEntity> m_player;
    WeaponType m_weapon = WeaponType::Basic;
    int m_health = PLAYER_MAX_HEALTH;
    float m_invulnTimer = 0.0f;
    vde::Cooldown m_fireCooldown{BASIC_COOLDOWN};

    void movePlayer(float dt);
    void fireWeapon();
    void cycleWeapon(int dir);

    // Projectiles
    struct LiveProjectile {
        std::shared_ptr<vde::SpriteEntity> sprite;
        ProjectileData data;
        bool isEnemy = false;
    };
    std::vector<LiveProjectile> m_projectiles;
    void updateProjectiles(float dt);
    void spawnPlayerBullet(glm::vec2 pos, glm::vec2 vel);
    void spawnEnemyBullet(glm::vec2 pos, glm::vec2 vel);

    // Enemies
    struct LiveEnemy {
        std::shared_ptr<vde::SpriteEntity> sprite;
        EnemyData data;
    };
    std::vector<LiveEnemy> m_enemies;
    MapLayout m_map;
    size_t m_nextSpawnIdx = 0;
    void spawnVisibleEnemies();
    void updateEnemies(float dt);

    // Stars
    struct LiveStar {
        std::shared_ptr<vde::SpriteEntity> sprite;
    };
    std::vector<LiveStar> m_stars;
    void initStars();
    void recycleStars();

    // Collision
    void checkCollisions();

    // Title sprites
    std::shared_ptr<vde::SpriteEntity> m_titleBanner;
    std::shared_ptr<vde::SpriteEntity> m_promptSprite;
    vde::RepeatingTimer m_blinkTimer{0.5f};
    bool m_blinkOn = true;

    // Score / HUD
    int m_score = 0;

    // Textures
    std::shared_ptr<vde::Texture> m_playerTex;
    std::shared_ptr<vde::Texture> m_bulletTex[3];
    std::shared_ptr<vde::Texture> m_enemyTex[4];
    std::shared_ptr<vde::Texture> m_enemyBulletTex;
    std::shared_ptr<vde::Texture> m_starTex;
    std::shared_ptr<vde::Texture> m_titleTex;
    std::shared_ptr<vde::Texture> m_promptTex;

    // Audio
    SoundBank m_sounds;

    // Helpers
    void clearAll();
    ShooterInput* input();
};

}  // namespace shooter
