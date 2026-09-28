#pragma once

#include <vde/api/GameAPI.h>

#include <memory>

namespace rungame {

class RunnerCharacter {
  public:
    explicit RunnerCharacter(vde::Scene& scene);

    void update(float deltaTime);
    void jump();
    void reset();

    [[nodiscard]] bool isJumping() const;
    [[nodiscard]] float left() const;
    [[nodiscard]] float right() const;
    [[nodiscard]] float bottom() const;
    [[nodiscard]] float top() const;

  private:
    static constexpr float kX = -7.0f;
    static constexpr float kGroundY = -4.04f;
    static constexpr float kWidth = 0.76f;
    static constexpr float kHeight = 1.08f;
    static constexpr float kJumpVelocity = 9.2f;
    static constexpr float kGravity = 22.0f;

    std::shared_ptr<vde::SpriteEntity> m_sprite;
    float m_y = kGroundY + kHeight * 0.5f;
    float m_velocityY = 0.0f;
};

}  // namespace rungame
