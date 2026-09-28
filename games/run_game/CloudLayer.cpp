#include "CloudLayer.h"

#include <iterator>

namespace rungame {

CloudLayer::CloudLayer(vde::Scene& scene) : ParallaxLayer(scene, 0.12f) {
    constexpr float centers[] = {-9.0f, -1.5f, 7.0f, 13.5f};
    const auto cloud = vde::Color(0.96f, 0.99f, 1.0f, 0.74f);
    for (std::size_t i = 0; i < std::size(centers); ++i) {
        const float x = centers[i];
        const float y = 3.2f + static_cast<float>(i % 2) * 0.65f;
        const float size = 0.75f + static_cast<float>(i % 3) * 0.12f;
        addSprite(x - size, y, 1.55f * size, 0.46f * size,
                  vde::Color(0.89f, 0.96f, 1.0f, 0.68f), -0.86f, 0.0f,
                  0.06f, 0.7f);
        addSprite(x, y + 0.16f, 1.9f * size, 0.62f * size, cloud,
                  -0.85f, 0.0f, 0.08f, 0.7f);
        addSprite(x + size, y - 0.02f, 1.35f * size, 0.44f * size, cloud,
                  -0.86f, 0.0f, 0.05f, 0.7f);
    }
}

void CloudLayer::update(float deltaTime, float runSpeed) {
    advanceSprites(deltaTime, runSpeed);
}

}  // namespace rungame
