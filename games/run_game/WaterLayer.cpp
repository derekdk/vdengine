#include "WaterLayer.h"

namespace rungame {

WaterLayer::WaterLayer(vde::Scene& scene) : ParallaxLayer(scene, 0.42f) {
    addSprite(0.0f, -2.25f, kSegmentWidth + 0.2f, 1.5f,
              vde::Color::fromHex(0x28799a), -0.42f);
    addSprite(0.0f, -1.56f, kSegmentWidth + 0.2f, 0.18f,
              vde::Color(0.77f, 0.92f, 0.91f, 0.48f), -0.40f);
    for (int i = 0; i < 6; ++i) {
        const float x = -9.5f + static_cast<float>(i) * 4.0f;
        const float y = -1.9f - static_cast<float>(i % 3) * 0.36f;
        addSprite(x, y, 1.2f + static_cast<float>(i % 2) * 0.5f, 0.08f,
                  vde::Color(0.60f, 0.88f, 0.94f, 0.7f), -0.38f, 0.0f,
                  0.04f, 1.2f, static_cast<float>(i) * 0.5f);
    }
}

void WaterLayer::update(float deltaTime, float runSpeed) {
    advanceSprites(deltaTime, runSpeed);
}

}  // namespace rungame
