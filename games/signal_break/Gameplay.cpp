#include "Gameplay.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace signalbreak {

Gameplay::Gameplay() {
    reset();
}

void Gameplay::reset(float checkpoint) {
    vde::PhysicsConfig config;
    config.gravity = {0.0f, GRAVITY};
    m_physics = std::make_unique<vde::PhysicsScene>(config);
    for (const auto& platform : platforms()) {
        auto definition = vde::PhysicsBodyDef::staticBox(platform.center, platform.halfSize);
        definition.friction = 0.0f;
        definition.restitution = 0.0f;
        m_physics->createBody(definition);
    }
    auto playerDefinition =
        vde::PhysicsBodyDef::dynamicBox({checkpoint, 0.75f}, PLAYER_HALF_SIZE, 1.0f, 0.0f, 0.0f);
    playerDefinition.linearDamping = 0.0f;
    m_player = m_physics->createBody(playerDefinition);
    m_enemies.clear();
    for (const auto& spawn : enemySpawns()) {
        auto definition = vde::PhysicsBodyDef::staticBox(spawn.position, {0.4f, 0.7f});
        definition.isSensor = true;
        auto body = m_physics->createBody(definition);
        m_enemies.push_back({body, spawn.position, spawn.position, spawn.commander ? 8 : 2,
                             0.7f + static_cast<float>(m_enemies.size() % 3) * 0.2f, -1.0f,
                             spawn.commander});
    }
    m_projectiles.clear();
    m_impacts.clear();
    m_health = MAX_HEALTH;
    m_kills = m_shots = m_enemyShots = m_jumps = 0;
    m_checkpoint = checkpoint;
    m_phase = RunPhase::Playing;
    m_paused = m_grounded = false;
    m_time = m_fireTimer = m_coyoteTimer = m_jumpBuffer = m_aim = 0.0f;
    m_facing = 1.0f;
    m_invulnerability = 1.0f;
}

glm::vec2 Gameplay::playerPosition() const {
    return m_physics->getBodyState(m_player).position;
}

glm::vec2 Gameplay::playerVelocity() const {
    return m_physics->getBodyState(m_player).velocity;
}

bool Gameplay::isGrounded() const {
    if (playerVelocity().y > 0.8f) {
        return false;
    }
    const glm::vec2 feet = playerPosition() - glm::vec2(0.0f, PLAYER_HALF_SIZE.y);
    auto bodies =
        m_physics->queryAABB(feet + glm::vec2(-0.29f, -0.12f), feet + glm::vec2(0.29f, 0.025f));
    for (auto body : bodies) {
        const auto definition = m_physics->getBodyDef(body);
        if (definition.type == vde::PhysicsBodyType::Static && !definition.isSensor) {
            const float top = m_physics->getBodyState(body).position.y + definition.extents.y;
            if (std::abs(feet.y - top) < 0.14f) {
                return true;
            }
        }
    }
    return false;
}

void Gameplay::movePlayer(float deltaTime, const InputFrame& input) {
    m_grounded = isGrounded();
    m_coyoteTimer = m_grounded ? 0.10f : std::max(0.0f, m_coyoteTimer - deltaTime);
    m_jumpBuffer = input.jump ? 0.12f : std::max(0.0f, m_jumpBuffer - deltaTime);
    glm::vec2 velocity = playerVelocity();
    velocity.x = input.move * PLAYER_SPEED;
    if (m_jumpBuffer > 0.0f && m_coyoteTimer > 0.0f) {
        velocity.y = JUMP_SPEED;
        m_jumpBuffer = m_coyoteTimer = 0.0f;
        m_grounded = false;
        ++m_jumps;
    }
    m_physics->setLinearVelocity(m_player, velocity);
    if (std::abs(input.move) > 0.1f) {
        m_facing = input.move > 0.0f ? 1.0f : -1.0f;
    }
    m_aim = std::abs(input.aim) > 0.45f ? (input.aim > 0.0f ? 0.85f : -0.85f) : 0.0f;
    m_fireTimer = std::max(0.0f, m_fireTimer - deltaTime);
    if (input.fire && m_fireTimer <= 0.0f) {
        fire(playerPosition() + glm::vec2(m_facing * 0.7f, 0.15f), {m_facing, m_aim}, false);
        m_fireTimer = 0.14f;
        ++m_shots;
    }
}

void Gameplay::fire(glm::vec2 origin, glm::vec2 direction, bool hostile) {
    if (m_projectiles.size() >= 96) {
        return;
    }
    m_projectiles.push_back(
        {origin, glm::normalize(direction) * (hostile ? 8.0f : 28.0f), 2.0f, hostile});
}

void Gameplay::updateEnemies(float deltaTime) {
    const auto player = playerPosition();
    for (auto& enemy : m_enemies) {
        if (enemy.health <= 0) {
            continue;
        }
        enemy.position.x = enemy.home.x + std::sin(m_time * 1.3f + enemy.home.x) * 0.65f;
        m_physics->setBodyPosition(enemy.body, enemy.position);
        enemy.facing = player.x < enemy.position.x ? -1.0f : 1.0f;
        if (glm::distance(player, enemy.position) > 14.0f) {
            continue;
        }
        enemy.fireTimer -= deltaTime;
        if (enemy.fireTimer <= 0.0f) {
            const glm::vec2 muzzle = enemy.position + glm::vec2(enemy.facing * 0.7f, 0.15f);
            const glm::vec2 direction = player - muzzle;
            if (glm::length(direction) > 0.1f) {
                fire(muzzle, direction, true);
                ++m_enemyShots;
            }
            enemy.fireTimer = enemy.commander ? 0.55f : 1.65f;
        }
        if (std::abs(player.x - enemy.position.x) < 0.72f &&
            std::abs(player.y - enemy.position.y) < 1.25f) {
            hurtPlayer();
        }
    }
}

void Gameplay::hurtPlayer() {
    if (m_invulnerability > 0.0f || m_phase != RunPhase::Playing) {
        return;
    }
    --m_health;
    m_invulnerability = 1.0f;
    if (m_health <= 0) {
        m_phase = RunPhase::Down;
    }
}

void Gameplay::updateProjectiles(float deltaTime) {
    for (auto& projectile : m_projectiles) {
        const auto travel = projectile.velocity * deltaTime;
        vde::RaycastHit hit;
        if (m_physics->raycast(projectile.position, travel, glm::length(travel), hit)) {
            projectile.life = 0.0f;
            m_impacts.push_back({hit.point, 0.24f, projectile.hostile});
            if (hit.bodyId == m_player && projectile.hostile) {
                hurtPlayer();
            }
            if (!projectile.hostile) {
                for (auto& enemy : m_enemies) {
                    if (enemy.health > 0 && enemy.body == hit.bodyId) {
                        --enemy.health;
                        if (enemy.health == 0) {
                            m_physics->destroyBody(enemy.body);
                            ++m_kills;
                        }
                        break;
                    }
                }
            }
        } else {
            projectile.position += travel;
            projectile.life -= deltaTime;
        }
    }
    std::erase_if(m_projectiles, [](const auto& projectile) { return projectile.life <= 0.0f; });
}

void Gameplay::update(float deltaTime, const InputFrame& input) {
    if (input.restart) {
        reset();
        return;
    }
    if (m_phase != RunPhase::Playing) {
        if (input.jump) {
            reset(m_phase == RunPhase::Down ? m_checkpoint : 3.0f);
        }
        return;
    }
    if (input.pause) {
        m_paused = !m_paused;
    }
    if (m_paused || deltaTime <= 0.0f) {
        return;
    }
    const float step = std::min(deltaTime, 0.05f);
    m_time += step;
    m_invulnerability = std::max(0.0f, m_invulnerability - step);
    for (auto& impact : m_impacts) {
        impact.life -= step;
    }
    std::erase_if(m_impacts, [](const auto& impact) { return impact.life <= 0.0f; });
    movePlayer(step, input);
    m_physics->step(step);
    m_grounded = isGrounded();
    updateEnemies(step);
    updateProjectiles(step);
    const auto position = playerPosition();
    if (position.y < -6.0f) {
        m_health = 0;
        m_phase = RunPhase::Down;
    }
    if (m_phase != RunPhase::Playing) {
        return;
    }
    const float nextCheckpoint = position.x >= 84.0f ? 84.0f : (position.x >= 44.0f ? 44.0f : 3.0f);
    if (nextCheckpoint > m_checkpoint && m_grounded) {
        m_checkpoint = nextCheckpoint;
        m_health = MAX_HEALTH;
        m_invulnerability = 1.0f;
    }
    if (position.x > 117.0f && m_enemies.back().health == 0) {
        m_phase = RunPhase::Complete;
    }
}

}  // namespace signalbreak