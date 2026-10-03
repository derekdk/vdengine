#pragma once

#include <glm/vec2.hpp>

#include <vector>

namespace signalbreak {

struct Platform {
    glm::vec2 center;
    glm::vec2 halfSize;
};

struct EnemySpawn {
    glm::vec2 position;
    bool commander = false;
};

constexpr float LEVEL_LENGTH = 120.0f;
constexpr float PLAYER_SPEED = 7.5f;
constexpr float JUMP_SPEED = 12.0f;
constexpr float GRAVITY = -26.0f;
constexpr int MAX_HEALTH = 6;
constexpr glm::vec2 PLAYER_HALF_SIZE{0.35f, 0.7f};

const std::vector<Platform>& platforms();
const std::vector<EnemySpawn>& enemySpawns();

}  // namespace signalbreak