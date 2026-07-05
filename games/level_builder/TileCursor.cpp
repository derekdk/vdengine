#include "TileCursor.h"

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr uint32_t kOutlineTextureSize = 24;
constexpr uint32_t kOutlineThickness = 2;
constexpr float kOutlineDepth = 1.35f;

void putPixel(std::vector<uint8_t>& pixels, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b,
              uint8_t a) {
    const size_t index = (static_cast<size_t>(y) * kOutlineTextureSize + x) * 4;
    pixels.at(index + 0) = r;
    pixels.at(index + 1) = g;
    pixels.at(index + 2) = b;
    pixels.at(index + 3) = a;
}

std::shared_ptr<vde::Texture> createOutlineTexture() {
    std::vector<uint8_t> pixels(static_cast<size_t>(kOutlineTextureSize) * kOutlineTextureSize * 4,
                                0);

    for (uint32_t y = 0; y < kOutlineTextureSize; ++y) {
        for (uint32_t x = 0; x < kOutlineTextureSize; ++x) {
            const bool isBorder = x < kOutlineThickness || y < kOutlineThickness ||
                                  x >= (kOutlineTextureSize - kOutlineThickness) ||
                                  y >= (kOutlineTextureSize - kOutlineThickness);
            if (isBorder) {
                putPixel(pixels, x, y, 255, 255, 255, 255);
            }
        }
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), kOutlineTextureSize, kOutlineTextureSize);
    return texture;
}

}  // namespace

namespace levelbuilder {

void TileCursor::initialize(vde::Scene& scene) {
    if (m_outline != nullptr) {
        return;
    }

    m_outline = scene.addEntity<vde::SpriteEntity>();
    m_outline->setTexture(createOutlineTexture());
    m_outline->setAnchor(0.5f, 0.5f);
    m_outline->setVisible(false);
}

void TileCursor::show(const glm::vec2& center, float tileWidth, float tileHeight) {
    if (m_outline == nullptr) {
        return;
    }

    m_outline->setVisible(true);
    m_outline->setPosition(center.x, center.y, kOutlineDepth);
    m_outline->setScale(tileWidth, tileHeight, 1.0f);
}

void TileCursor::hide() {
    if (m_outline == nullptr) {
        return;
    }

    m_outline->setVisible(false);
}

bool TileCursor::isVisible() const {
    return m_outline != nullptr && m_outline->isVisible();
}

}  // namespace levelbuilder