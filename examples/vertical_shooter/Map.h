#pragma once

/**
 * @file Map.h
 * @brief Procedural map generation for the vertical shooter.
 */

#include <cstdint>
#include <vector>

#include "Types.h"

namespace shooter {

struct MapLayout {
    std::vector<EnemySpawn> enemies;
};

/**
 * @brief Procedurally generate enemy placements for the entire map.
 * @param seed Random seed for deterministic generation.
 */
MapLayout generateMap(uint32_t seed);

}  // namespace shooter
