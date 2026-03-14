#pragma once

/**
 * @file Types.h
 * @brief Shared constants, enums, and data types for the vertical shooter.
 */

#include <glm/glm.hpp>

namespace shooter {

// ============================================================================
// World dimensions
// ============================================================================

static constexpr float VIEW_WIDTH = 10.0f;
static constexpr float VIEW_HEIGHT = 14.0f;
static constexpr float HALF_VIEW_W = VIEW_WIDTH * 0.5f;
static constexpr float HALF_VIEW_H = VIEW_HEIGHT * 0.5f;
static constexpr int MAP_SCREENS = 16;
static constexpr float MAP_HEIGHT = VIEW_HEIGHT * MAP_SCREENS;
static constexpr float SCROLL_SPEED = 2.0f;

// ============================================================================
// Player
// ============================================================================

static constexpr float PLAYER_SPEED = 8.0f;
static constexpr float PLAYER_HALF_W = 0.35f;
static constexpr float PLAYER_HALF_H = 0.45f;
static constexpr int PLAYER_MAX_HEALTH = 3;
static constexpr float INVULN_TIME = 1.5f;

// ============================================================================
// Projectiles
// ============================================================================

static constexpr float BULLET_SPEED = 16.0f;
static constexpr float SPREAD_SPEED = 14.0f;
static constexpr float SPREAD_ANGLE = 0.25f;  // radians off-center
static constexpr float RAPID_SPEED = 18.0f;
static constexpr float ENEMY_BULLET_SPEED = 6.0f;
static constexpr float BASIC_COOLDOWN = 0.22f;
static constexpr float SPREAD_COOLDOWN = 0.35f;
static constexpr float RAPID_COOLDOWN = 0.08f;

// ============================================================================
// Enemies
// ============================================================================

static constexpr float TURRET_FIRE_INTERVAL = 1.8f;
static constexpr float DRONE_SPEED = 2.5f;
static constexpr float DRONE_AMPLITUDE = 3.0f;
static constexpr float CHASER_SPEED = 3.5f;
static constexpr float TANK_SPEED = 1.0f;
static constexpr int TURRET_HP = 2;
static constexpr int DRONE_HP = 1;
static constexpr int CHASER_HP = 1;
static constexpr int TANK_HP = 5;

// ============================================================================
// Scoring
// ============================================================================

static constexpr int SCORE_TURRET = 20;
static constexpr int SCORE_DRONE = 10;
static constexpr int SCORE_CHASER = 15;
static constexpr int SCORE_TANK = 50;

// ============================================================================
// Enums
// ============================================================================

enum class WeaponType { Basic, Spread, Rapid, Count };
enum class EnemyType { Turret, Drone, Chaser, Tank };

// ============================================================================
// Data structures
// ============================================================================

struct ProjectileData {
    glm::vec2 velocity{0.0f};
    bool alive = true;
};

struct EnemyData {
    EnemyType type = EnemyType::Drone;
    int health = 1;
    float timer = 0.0f;
    float baseX = 0.0f;  // for sine-wave drones
    float baseY = 0.0f;
    bool alive = true;
};

struct StarData {
    float brightness = 1.0f;
};

struct EnemySpawn {
    float x = 0.0f;
    float y = 0.0f;
    EnemyType type = EnemyType::Drone;
};

}  // namespace shooter
