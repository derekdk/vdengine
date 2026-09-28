#pragma once

#include <array>
#include <memory>

#include <vde/api/GameAPI.h>

namespace rungame {

class RunnerCharacter;

class ObstacleCourse {
  public:
    explicit ObstacleCourse(vde::Scene& scene);

    void update(float deltaTime, float runSpeed);
    void reset();
    [[nodiscard]] bool collides(const RunnerCharacter& runner) const;

  private:
    struct Obstacle {
        std::shared_ptr<vde::SpriteEntity> sprite;
        float x;
        float width;
        float height;
        float initialX;
    };

    static constexpr float kGroundY = -4.04f;
    static constexpr float kWrapDistance = 48.0f;
    std::array<Obstacle, 3> m_obstacles;
};

}  // namespace rungame
