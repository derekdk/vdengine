#include "PlayerController.h"

#include <algorithm>
#include <cmath>

#include "TileMapSession.h"

namespace {

constexpr float kPlayerWidth = 0.78f;
constexpr float kPlayerHeight = 1.42f;
constexpr float kPlayerHalfWidth = kPlayerWidth * 0.5f;
constexpr float kPlayerHalfHeight = kPlayerHeight * 0.5f;
constexpr float kMoveSpeed = 7.5f;
constexpr float kJumpVelocity = 11.5f;
constexpr float kGravity = -28.0f;
constexpr float kMaxFallVelocity = -22.0f;
constexpr float kCollisionEpsilon = 0.001f;

struct AABB {
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
};

AABB makePlayerBounds(const glm::vec2& center) {
    return AABB{.left = center.x - kPlayerHalfWidth,
                .right = center.x + kPlayerHalfWidth,
                .bottom = center.y - kPlayerHalfHeight,
                .top = center.y + kPlayerHalfHeight};
}

AABB makeRectBounds(const vde::TileCollisionRect& rect) {
    return AABB{.left = rect.center.x - rect.halfExtents.x,
                .right = rect.center.x + rect.halfExtents.x,
                .bottom = rect.center.y - rect.halfExtents.y,
                .top = rect.center.y + rect.halfExtents.y};
}

bool intersects(const AABB& a, const AABB& b) {
    return a.left < b.right && a.right > b.left && a.bottom < b.top && a.top > b.bottom;
}

bool overlapsHorizontally(const AABB& a, const AABB& b) {
    return a.left < b.right && a.right > b.left;
}

}  // namespace

namespace levelbuilder {

void PlayerController::createEntities(vde::Scene& scene) {
    // Entities are owned by the scene and only need to be created once per scene instance.
    if (m_player != nullptr) {
        return;
    }

    m_player = scene.addEntity<vde::SpriteEntity>();
    m_player->setScale(kPlayerWidth, kPlayerHeight, 1.0f);
    m_player->setAnchor(0.5f, 0.5f);
    m_player->setColor(vde::Color::fromHex(0xfff1a8));

    m_playerShadow = scene.addEntity<vde::SpriteEntity>();
    m_playerShadow->setScale(0.95f, 0.22f, 1.0f);
    m_playerShadow->setColor(vde::Color(0.05f, 0.08f, 0.12f, 0.45f));
}

void PlayerController::reset(const TileMapSession& session, vde::Camera2D* camera) {
    m_playerPosition = session.spawnPoint();
    m_playerVelocity = glm::vec2(0.0f);
    m_onGround = false;

    if (camera != nullptr) {
        camera->setPosition(session.spawnPoint().x + 3.0f, session.spawnPoint().y + 1.5f);
    }

    syncVisuals();
}

void PlayerController::update(float deltaTime, float moveAxis, bool jumpRequested,
                              const TileMapSession& session) {
    if (m_player == nullptr || session.tileMap() == nullptr) {
        return;
    }

    if (jumpRequested && m_onGround) {
        m_playerVelocity.y = kJumpVelocity;
        m_onGround = false;
    }

    m_playerVelocity.x = moveAxis * kMoveSpeed;
    m_playerVelocity.y = std::max(m_playerVelocity.y + (kGravity * deltaTime), kMaxFallVelocity);

    m_playerPosition.x += m_playerVelocity.x * deltaTime;
    resolveHorizontalCollisions(session);

    const float previousBottom = m_playerPosition.y - kPlayerHalfHeight;
    m_playerPosition.y += m_playerVelocity.y * deltaTime;
    m_onGround = false;
    resolveVerticalCollisions(session, previousBottom);

    const float maxX = static_cast<float>(session.tileMap()->getColumnCount()) - kPlayerHalfWidth;
    m_playerPosition.x = std::clamp(m_playerPosition.x, kPlayerHalfWidth, maxX);

    if (moveAxis < 0.0f) {
        m_player->setFlipX(true);
    } else if (moveAxis > 0.0f) {
        m_player->setFlipX(false);
    }

    syncVisuals();
}

void PlayerController::resolveHorizontalCollisions(const TileMapSession& session) {
    if (std::abs(m_playerVelocity.x) < 0.0001f) {
        return;
    }

    AABB playerBounds = makePlayerBounds(m_playerPosition);
    for (const auto& rect : session.solidRects()) {
        const AABB solidBounds = makeRectBounds(rect);
        if (!intersects(playerBounds, solidBounds)) {
            continue;
        }

        if (m_playerVelocity.x > 0.0f) {
            m_playerPosition.x = solidBounds.left - kPlayerHalfWidth - kCollisionEpsilon;
        } else {
            m_playerPosition.x = solidBounds.right + kPlayerHalfWidth + kCollisionEpsilon;
        }

        m_playerVelocity.x = 0.0f;
        playerBounds = makePlayerBounds(m_playerPosition);
    }
}

void PlayerController::resolveVerticalCollisions(const TileMapSession& session,
                                                 float previousBottom) {
    AABB playerBounds = makePlayerBounds(m_playerPosition);

    for (const auto& rect : session.solidRects()) {
        const AABB solidBounds = makeRectBounds(rect);
        if (!intersects(playerBounds, solidBounds)) {
            continue;
        }

        if (m_playerVelocity.y > 0.0f) {
            m_playerPosition.y = solidBounds.bottom - kPlayerHalfHeight - kCollisionEpsilon;
        } else {
            m_playerPosition.y = solidBounds.top + kPlayerHalfHeight + kCollisionEpsilon;
            m_onGround = true;
        }

        m_playerVelocity.y = 0.0f;
        playerBounds = makePlayerBounds(m_playerPosition);
    }

    if (m_playerVelocity.y > 0.0f) {
        return;
    }

    playerBounds = makePlayerBounds(m_playerPosition);
    for (const auto& rect : session.oneWayRects()) {
        const AABB platformBounds = makeRectBounds(rect);
        if (!overlapsHorizontally(playerBounds, platformBounds)) {
            continue;
        }

        if (previousBottom >= (platformBounds.top - 0.02f) &&
            playerBounds.bottom <= platformBounds.top && playerBounds.top > platformBounds.top) {
            m_playerPosition.y = platformBounds.top + kPlayerHalfHeight + kCollisionEpsilon;
            m_playerVelocity.y = 0.0f;
            m_onGround = true;
            break;
        }
    }
}

void PlayerController::syncVisuals() {
    if (m_player == nullptr || m_playerShadow == nullptr) {
        return;
    }

    const float speedRatio =
        vde::math2d::saturate(std::abs(m_playerVelocity.x) / (kMoveSpeed + 0.001f));
    m_player->setPosition(m_playerPosition.x, m_playerPosition.y, 0.75f);
    m_player->setScale(kPlayerWidth + (speedRatio * 0.08f), kPlayerHeight - (speedRatio * 0.06f),
                       1.0f);
    m_player->setColor(m_onGround ? vde::Color::fromHex(0xfff1a8) : vde::Color::fromHex(0xffc36b));

    const float shadowY = std::max(0.12f, m_playerPosition.y - kPlayerHalfHeight);
    m_playerShadow->setPosition(m_playerPosition.x, shadowY, 0.68f);
    m_playerShadow->setScale(0.84f - (speedRatio * 0.08f), 0.18f, 1.0f);
}

}  // namespace levelbuilder