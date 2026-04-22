/**
 * @file Map.cpp
 * @brief Procedural map generation — places enemies at fixed Y positions along the map.
 */

#include "Map.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace shooter {

MapLayout generateMap(uint32_t seed) {
    std::mt19937 rng(seed);
    MapLayout layout;

    // Leave the first 2 screens relatively empty (warm-up zone).
    float startY = VIEW_HEIGHT * 2.0f;
    float endY = MAP_HEIGHT - VIEW_HEIGHT;

    // Parameters table: how many enemies per screen by type
    // Difficulty ramps up as Y increases.
    auto randFloat = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    auto randInt = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

    float y = startY;
    int screen = 2;

    while (y < endY) {
        float sectionHeight = VIEW_HEIGHT;
        float sectionTop = y + sectionHeight;

        // Difficulty tier based on screen index
        int tier = std::min(screen / 4, 3);

        // Drones — always present
        int numDrones = 2 + tier + randInt(0, 2);
        for (int i = 0; i < numDrones; ++i) {
            float ex = randFloat(-HALF_VIEW_W + 1.0f, HALF_VIEW_W - 1.0f);
            float ey = randFloat(y + 1.0f, sectionTop - 1.0f);
            layout.enemies.push_back({ex, ey, EnemyType::Drone});
        }

        // Turrets — from screen 3 onward
        if (screen >= 3) {
            int numTurrets = 1 + tier / 2 + randInt(0, 1);
            for (int i = 0; i < numTurrets; ++i) {
                float ex = randFloat(-HALF_VIEW_W + 1.5f, HALF_VIEW_W - 1.5f);
                float ey = randFloat(y + 2.0f, sectionTop - 2.0f);
                layout.enemies.push_back({ex, ey, EnemyType::Turret});
            }
        }

        // Chasers — from screen 5 onward
        if (screen >= 5) {
            int numChasers = tier + randInt(0, 1);
            for (int i = 0; i < numChasers; ++i) {
                float ex = randFloat(-HALF_VIEW_W + 1.0f, HALF_VIEW_W - 1.0f);
                float ey = randFloat(y + 1.0f, sectionTop - 1.0f);
                layout.enemies.push_back({ex, ey, EnemyType::Chaser});
            }
        }

        // Tanks — from screen 8 onward, rare
        if (screen >= 8) {
            if (randInt(0, 2) == 0) {
                float ex = randFloat(-HALF_VIEW_W + 2.0f, HALF_VIEW_W - 2.0f);
                float ey = randFloat(y + 3.0f, sectionTop - 3.0f);
                layout.enemies.push_back({ex, ey, EnemyType::Tank});
            }
        }

        y += sectionHeight;
        ++screen;
    }

    // Sort enemies by Y so we can spawn them in order as the camera scrolls
    std::sort(layout.enemies.begin(), layout.enemies.end(),
              [](const EnemySpawn& a, const EnemySpawn& b) { return a.y < b.y; });

    return layout;
}

}  // namespace shooter
