#include "RoadLayer.h"

namespace rungame {

RoadLayer::RoadLayer(vde::Scene& scene) : ParallaxLayer(scene, 1.0f) {
    addSprite(0.0f, -5.45f, kSegmentWidth + 0.2f, 2.7f, vde::Color::fromHex(0x87603e), 0.32f);
    addSprite(0.0f, -4.15f, kSegmentWidth + 0.2f, 0.22f, vde::Color::fromHex(0xe0bd7a), 0.34f);
    for (int i = 0; i < 8; ++i) {
        const float x = -10.0f + static_cast<float>(i) * 3.0f;
        addSprite(x, -5.3f, 1.3f, 0.11f, vde::Color::fromHex(0xc29761), 0.35f);
    }
}

void RoadLayer::update(float deltaTime, float runSpeed) {
    advanceSprites(deltaTime, runSpeed);
}

}  // namespace rungame
