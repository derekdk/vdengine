#include "RunnerCharacter.h"

namespace rungame {

RunnerCharacter::RunnerCharacter(vde::Scene& scene)
    : m_sprite(scene.addEntity<vde::SpriteEntity>()) {
    m_sprite->setScale(kWidth, kHeight, 1.0f);
    m_sprite->setColor(vde::Color::fromHex(0xff8b4a));
    reset();
}

void RunnerCharacter::update(float deltaTime) {
    if (m_velocityY == 0.0f) {
        return;
    }

    m_velocityY -= kGravity * deltaTime;
    m_y += m_velocityY * deltaTime;
    const float groundCenter = kGroundY + kHeight * 0.5f;
    if (m_y <= groundCenter) {
        m_y = groundCenter;
        m_velocityY = 0.0f;
    }
    m_sprite->setPosition(kX, m_y, 0.55f);
}

void RunnerCharacter::jump() {
    if (m_velocityY == 0.0f && m_y <= kGroundY + kHeight * 0.5f) {
        m_velocityY = kJumpVelocity;
    }
}

void RunnerCharacter::reset() {
    m_y = kGroundY + kHeight * 0.5f;
    m_velocityY = 0.0f;
    m_sprite->setPosition(kX, m_y, 0.55f);
}

bool RunnerCharacter::isJumping() const {
    return m_y > kGroundY + kHeight * 0.5f;
}

float RunnerCharacter::left() const {
    return kX - kWidth * 0.5f;
}

float RunnerCharacter::right() const {
    return kX + kWidth * 0.5f;
}

float RunnerCharacter::bottom() const {
    return m_y - kHeight * 0.5f;
}

float RunnerCharacter::top() const {
    return m_y + kHeight * 0.5f;
}

}  // namespace rungame
