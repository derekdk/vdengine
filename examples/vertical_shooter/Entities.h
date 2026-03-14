#pragma once

/**
 * @file Entities.h
 * @brief Entity management helpers — spawning, AI updates, and collision for the vertical shooter.
 */

#include <vde/api/GameAPI.h>

#include <vector>

#include "Types.h"

namespace shooter {

// ============================================================================
// Collision helper
// ============================================================================

bool boxOverlap(glm::vec2 aPos, glm::vec2 aHalf, glm::vec2 bPos, glm::vec2 bHalf);

// ============================================================================
// Enemy AI tick (modifies entity transform, optionally requests a bullet spawn)
// ============================================================================

struct FireRequest {
    glm::vec2 origin{0.0f};
    glm::vec2 velocity{0.0f};
};

/**
 * @brief Advance one enemy's AI for a single frame.
 * @return true if the enemy wants to fire a bullet this frame.
 */
bool updateEnemy(EnemyData& data, vde::SpriteEntity* sprite, float dt, glm::vec2 playerPos,
                 float scrollY, FireRequest& outFire);

// ============================================================================
// Hit-flash helper
// ============================================================================

glm::vec2 enemyHalfExtents(EnemyType type);
int enemyScore(EnemyType type);

}  // namespace shooter
