#include "MountainLayer.h"

#include <iterator>

namespace rungame {

MountainLayer::MountainLayer(vde::Scene& scene) : ParallaxLayer(scene, 0.24f) {
    const auto farBlue = vde::Color::fromHex(0x7998b1);
    const auto nearBlue = vde::Color::fromHex(0x547d98);
    const auto snow = vde::Color::fromHex(0xe4f2f3);
    addSprite(0.0f, -1.05f, kSegmentWidth + 0.2f, 1.55f,
              vde::Color::fromHex(0x7196a2), -0.72f);

    constexpr float peaks[] = {-9.0f, -2.7f, 3.6f, 9.9f};
    for (std::size_t i = 0; i < std::size(peaks); ++i) {
        const float height = 3.1f + static_cast<float>(i % 3) * 0.55f;
        const float x = peaks[i];
        addSprite(x, -0.05f + height * 0.27f, 5.7f, height * 0.56f,
                  farBlue, -0.68f, 35.0f);
        addSprite(x + 1.8f, -0.02f + height * 0.25f, 5.4f, height * 0.52f,
                  nearBlue, -0.67f, -33.0f);
        addSprite(x, height * 0.55f, 0.48f, 0.42f, snow, -0.65f);
    }
}

void MountainLayer::update(float deltaTime, float runSpeed) {
    advanceSprites(deltaTime, runSpeed);
}

}  // namespace rungame
