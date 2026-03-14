/**
 * @file Entities.cpp
 * @brief Enemy AI, collision helpers, and entity sizing.
 */

#include "Entities.h"

#include <cmath>

namespace shooter {

// ============================================================================
// Collision
// ============================================================================

bool boxOverlap(glm::vec2 aPos, glm::vec2 aHalf, glm::vec2 bPos, glm::vec2 bHalf) {
    return std::abs(aPos.x - bPos.x) < (aHalf.x + bHalf.x) &&
           std::abs(aPos.y - bPos.y) < (aHalf.y + bHalf.y);
}

// ============================================================================
// Enemy sizing and scoring
// ============================================================================

glm::vec2 enemyHalfExtents(EnemyType type) {
    switch (type) {
    case EnemyType::Turret:
        return {0.35f, 0.35f};
    case EnemyType::Drone:
        return {0.3f, 0.3f};
    case EnemyType::Chaser:
        return {0.3f, 0.3f};
    case EnemyType::Tank:
        return {0.5f, 0.45f};
    }
    return {0.3f, 0.3f};
}

int enemyScore(EnemyType type) {
    switch (type) {
    case EnemyType::Turret:
        return SCORE_TURRET;
    case EnemyType::Drone:
        return SCORE_DRONE;
    case EnemyType::Chaser:
        return SCORE_CHASER;
    case EnemyType::Tank:
        return SCORE_TANK;
    }
    return 10;
}

// ============================================================================
// Enemy AI
// ============================================================================

bool updateEnemy(EnemyData& data, vde::SpriteEntity* sprite, float dt, glm::vec2 playerPos,
                 float /*scrollY*/, FireRequest& outFire) {
    if (!data.alive || !sprite)
        return false;

    data.timer += dt;
    auto pos = sprite->getPosition();

    switch (data.type) {
    case EnemyType::Turret: {
        // Stationary; fires periodically toward the player
        if (data.timer >= TURRET_FIRE_INTERVAL) {
            data.timer -= TURRET_FIRE_INTERVAL;
            glm::vec2 dir = playerPos - glm::vec2(pos.x, pos.y);
            float len = glm::length(dir);
            if (len > 0.01f) {
                dir /= len;
                outFire.origin = {pos.x, pos.y};
                outFire.velocity = dir * ENEMY_BULLET_SPEED;
                return true;
            }
        }
        break;
    }

    case EnemyType::Drone: {
        // Sine-wave horizontal drift
        float newX = data.baseX + std::sin(data.timer * 2.0f) * DRONE_AMPLITUDE;
        float newY = data.baseY - DRONE_SPEED * data.timer * 0.3f;
        sprite->setPosition(newX, newY, 0.0f);
        break;
    }

    case EnemyType::Chaser: {
        // Accelerate toward the player
        glm::vec2 toPlayer = playerPos - glm::vec2(pos.x, pos.y);
        float len = glm::length(toPlayer);
        if (len > 0.1f)
            toPlayer /= len;
        glm::vec2 move = toPlayer * CHASER_SPEED * dt;
        sprite->setPosition(pos.x + move.x, pos.y + move.y, 0.0f);
        break;
    }

    case EnemyType::Tank: {
        // Slow downward drift, fires periodically
        sprite->setPosition(pos.x, pos.y - TANK_SPEED * dt, 0.0f);
        if (data.timer >= TURRET_FIRE_INTERVAL * 0.8f) {
            data.timer -= TURRET_FIRE_INTERVAL * 0.8f;
            outFire.origin = {pos.x, pos.y};
            outFire.velocity = {0.0f, -ENEMY_BULLET_SPEED};
            return true;
        }
        break;
    }
    }

    return false;
}

}  // namespace shooter
