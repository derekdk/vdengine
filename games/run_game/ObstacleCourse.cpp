#include "ObstacleCourse.h"

#include <array>

#include "RunnerCharacter.h"

namespace rungame {

ObstacleCourse::ObstacleCourse(vde::Scene& scene) {
    constexpr std::array<float, 3> starts{9.0f, 25.0f, 41.0f};
    constexpr std::array<float, 3> widths{0.78f, 1.05f, 0.82f};
    constexpr std::array<float, 3> heights{1.12f, 0.92f, 1.3f};
    const std::array<vde::Color, 3> colors{
        vde::Color::fromHex(0xd95642),
        vde::Color::fromHex(0xcf713a),
        vde::Color::fromHex(0xd95642),
    };

    for (std::size_t i = 0; i < m_obstacles.size(); ++i) {
        auto sprite = scene.addEntity<vde::SpriteEntity>();
        sprite->setScale(widths[i], heights[i], 1.0f);
        sprite->setColor(colors[i]);
        m_obstacles[i] = {sprite, starts[i], widths[i], heights[i], starts[i]};
    }
    reset();
}

void ObstacleCourse::update(float deltaTime, float runSpeed) {
    for (auto& obstacle : m_obstacles) {
        obstacle.x -= runSpeed * deltaTime;
        while (obstacle.x + obstacle.width * 0.5f < -12.5f) {
            obstacle.x += kWrapDistance;
        }
        obstacle.sprite->setPosition(obstacle.x, kGroundY + obstacle.height * 0.5f, 0.52f);
    }
}

void ObstacleCourse::reset() {
    for (auto& obstacle : m_obstacles) {
        obstacle.x = obstacle.initialX;
        obstacle.sprite->setPosition(obstacle.x, kGroundY + obstacle.height * 0.5f, 0.52f);
    }
}

bool ObstacleCourse::collides(const RunnerCharacter& runner) const {
    for (const auto& obstacle : m_obstacles) {
        const float halfWidth = obstacle.width * 0.5f;
        const float bottom = kGroundY;
        const float top = bottom + obstacle.height;
        if (runner.right() >= obstacle.x - halfWidth && runner.left() <= obstacle.x + halfWidth &&
            runner.top() >= bottom && runner.bottom() <= top) {
            return true;
        }
    }
    return false;
}

}  // namespace rungame
