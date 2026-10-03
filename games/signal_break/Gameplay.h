#pragma once

#include <vde/api/PhysicsScene.h>

#include <memory>
#include <vector>

#include "Input.h"
#include "Level.h"

namespace signalbreak {

enum class RunPhase { Playing, Down, Complete };

struct Enemy {
    vde::PhysicsBodyId body;
    glm::vec2 home;
    glm::vec2 position;
    int health;
    float fireTimer;
    float facing = -1.0f;
    bool commander;
};

struct Projectile {
    glm::vec2 position;
    glm::vec2 velocity;
    float life = 2.0f;
    bool hostile = false;
};

struct Impact {
    glm::vec2 position;
    float life = 0.24f;
    bool hostile = false;
};

class Gameplay {
  public:
    Gameplay();
    void reset(float checkpoint = 3.0f);
    void update(float deltaTime, const InputFrame& input);

    glm::vec2 playerPosition() const;
    glm::vec2 playerVelocity() const;
    const std::vector<Enemy>& enemies() const { return m_enemies; }
    const std::vector<Projectile>& projectiles() const { return m_projectiles; }
    const std::vector<Impact>& impacts() const { return m_impacts; }
    int health() const { return m_health; }
    int kills() const { return m_kills; }
    int shots() const { return m_shots; }
    int enemyShots() const { return m_enemyShots; }
    int jumps() const { return m_jumps; }
    bool grounded() const { return m_grounded; }
    bool paused() const { return m_paused; }
    float facing() const { return m_facing; }
    float aim() const { return m_aim; }
    float time() const { return m_time; }
    float checkpoint() const { return m_checkpoint; }
    float invulnerability() const { return m_invulnerability; }
    RunPhase phase() const { return m_phase; }

  private:
    void movePlayer(float deltaTime, const InputFrame& input);
    void updateEnemies(float deltaTime);
    void updateProjectiles(float deltaTime);
    void fire(glm::vec2 origin, glm::vec2 direction, bool hostile);
    void hurtPlayer();
    bool isGrounded() const;

    std::unique_ptr<vde::PhysicsScene> m_physics;
    vde::PhysicsBodyId m_player = vde::INVALID_PHYSICS_BODY_ID;
    std::vector<Enemy> m_enemies;
    std::vector<Projectile> m_projectiles;
    std::vector<Impact> m_impacts;
    RunPhase m_phase = RunPhase::Playing;
    int m_health = MAX_HEALTH;
    int m_kills = 0;
    int m_shots = 0;
    int m_enemyShots = 0;
    int m_jumps = 0;
    float m_time = 0.0f;
    float m_facing = 1.0f;
    float m_aim = 0.0f;
    float m_checkpoint = 3.0f;
    float m_fireTimer = 0.0f;
    float m_coyoteTimer = 0.0f;
    float m_jumpBuffer = 0.0f;
    float m_invulnerability = 0.0f;
    bool m_grounded = false;
    bool m_paused = false;
};

}  // namespace signalbreak