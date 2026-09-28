#include "FoliageLayer.h"

#include <iterator>

namespace rungame {

FoliageLayer::FoliageLayer(vde::Scene& scene) : ParallaxLayer(scene, 0.72f) {
    const auto leaf = vde::Color::fromHex(0x3e733e);
    addSprite(0.0f, -3.25f, kSegmentWidth + 0.2f, 1.05f,
              vde::Color::fromHex(0x578046), 0.08f);

    constexpr float treePositions[] = {-10.0f, -4.8f, 0.4f, 5.6f, 10.8f};
    for (std::size_t i = 0; i < std::size(treePositions); ++i) {
        const float x = treePositions[i];
        const float trunkHeight = 1.1f + static_cast<float>(i % 3) * 0.18f;
        const float phase = static_cast<float>(i);
        addSprite(x, -3.0f + trunkHeight * 0.5f, 0.18f, trunkHeight,
                  vde::Color::fromHex(0x775335), 0.10f, 0.0f, 0.015f, 1.0f, phase);
        addSprite(x - 0.28f, -2.05f + trunkHeight * 0.12f, 0.95f, 0.74f,
                  leaf, 0.12f, -8.0f, 0.035f, 1.0f, phase);
        addSprite(x + 0.25f, -1.96f + trunkHeight * 0.12f, 0.98f, 0.8f,
                  vde::Color::fromHex(0x4b8645), 0.13f, 7.0f, 0.04f, 1.0f, phase + 0.5f);
    }
}

void FoliageLayer::update(float deltaTime, float runSpeed) {
    advanceSprites(deltaTime, runSpeed);
}

}  // namespace rungame
