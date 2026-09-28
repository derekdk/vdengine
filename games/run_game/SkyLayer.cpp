#include "SkyLayer.h"

#include <array>
#include <cmath>
#include <utility>

namespace rungame {

SkyLayer::SkyLayer(vde::Scene& scene) : ParallaxLayer(scene, 0.0f) {
    m_sky = addStaticSprite(0.0f, 0.0f, 30.0f, 16.0f,
                            vde::Color::fromHex(0x83c9ef), -0.99f);
    m_sun = addStaticSprite(-8.5f, 4.6f, 2.0f, 2.0f,
                            vde::Color::fromHex(0xffdf91), -0.96f);
    m_moon = addStaticSprite(8.5f, 4.6f, 1.35f, 1.35f,
                             vde::Color::fromHex(0xe5edff), -0.95f);

    constexpr std::array<std::pair<float, float>, 12> positions{{
        {-10.5f, 2.4f}, {-8.0f, 4.7f}, {-5.4f, 3.5f}, {-2.1f, 5.4f},
        {0.4f, 2.7f},   {2.8f, 4.3f},  {5.5f, 3.1f},  {8.1f, 5.5f},
        {10.2f, 2.5f},  {-6.7f, 5.8f}, {4.1f, 5.9f},  {0.1f, 5.8f},
    }};
    m_stars.reserve(positions.size());
    for (const auto& [x, y] : positions) {
        m_stars.push_back(addStaticSprite(x, y, 0.07f, 0.07f,
                                          vde::Color(1.0f, 1.0f, 0.95f, 0.3f), -0.94f));
    }
}

void SkyLayer::update(float deltaTime, float runSpeed) {
    static_cast<void>(runSpeed);
    m_time += deltaTime;
    const float angle = m_time * 0.035f;
    const float sunX = 9.0f * std::sin(angle);
    const float sunY = 1.8f + 3.0f * std::cos(angle);
    const float daylight = 0.5f + 0.5f * std::cos(angle);

    m_sun->setPosition(sunX, sunY, -0.96f);
    m_sun->setColor(vde::Color(1.0f, 0.88f, 0.55f, daylight));
    m_moon->setPosition(-sunX, -sunY, -0.95f);
    m_moon->setColor(vde::Color(0.86f, 0.91f, 1.0f, 1.0f - daylight));
    m_sky->setColor(vde::Color(
        0.03f + 0.48f * daylight, 0.05f + 0.72f * daylight, 0.16f + 0.77f * daylight, 1.0f));

    for (std::size_t i = 0; i < m_stars.size(); ++i) {
        const float twinkle = 0.45f + 0.55f * std::sin(m_time * (1.1f + 0.12f * i) + i);
        m_stars[i]->setColor(vde::Color(1.0f, 1.0f, 0.95f,
                                        (1.0f - daylight) * (0.25f + twinkle * 0.7f)));
    }
}

void SkyLayer::reset() {
    ParallaxLayer::reset();
    update(0.0f, 0.0f);
}

}  // namespace rungame
