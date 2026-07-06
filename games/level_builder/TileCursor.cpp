#include "TileCursor.h"

#include <vde/api/GameAPI.h>

#include <algorithm>

namespace {

constexpr float kOutlineDepth = 1.35f;
constexpr float kOutlinePadding = 0.05f;
constexpr float kOutlineThickness = 0.08f;

}  // namespace

namespace levelbuilder {

void TileCursor::initialize(vde::Scene& scene) {
    if (m_outlineSegments[0] != nullptr) {
        return;
    }

    for (auto& segment : m_outlineSegments) {
        segment = scene.addEntity<vde::SpriteEntity>();
        segment->setAnchor(0.5f, 0.5f);
        segment->setColor(vde::Color::white());
        segment->setVisible(false);
    }
}

void TileCursor::show(const glm::vec2& center, float tileWidth, float tileHeight) {
    if (m_outlineSegments[0] == nullptr) {
        return;
    }

    const float halfWidth = (tileWidth * 0.5f) + kOutlinePadding;
    const float halfHeight = (tileHeight * 0.5f) + kOutlinePadding;
    const float horizontalLength = (halfWidth * 2.0f) + kOutlineThickness;
    const float verticalLength = (halfHeight * 2.0f) + kOutlineThickness;
    const float lineThickness =
        std::min({kOutlineThickness, tileWidth * 0.25f, tileHeight * 0.25f});

    auto& top = m_outlineSegments[0];
    top->setVisible(true);
    top->setPosition(center.x, center.y + halfHeight, kOutlineDepth);
    top->setScale(horizontalLength, lineThickness, 1.0f);

    auto& bottom = m_outlineSegments[1];
    bottom->setVisible(true);
    bottom->setPosition(center.x, center.y - halfHeight, kOutlineDepth);
    bottom->setScale(horizontalLength, lineThickness, 1.0f);

    auto& left = m_outlineSegments[2];
    left->setVisible(true);
    left->setPosition(center.x - halfWidth, center.y, kOutlineDepth);
    left->setScale(lineThickness, verticalLength, 1.0f);

    auto& right = m_outlineSegments[3];
    right->setVisible(true);
    right->setPosition(center.x + halfWidth, center.y, kOutlineDepth);
    right->setScale(lineThickness, verticalLength, 1.0f);
}

void TileCursor::hide() {
    for (auto& segment : m_outlineSegments) {
        if (segment != nullptr) {
            segment->setVisible(false);
        }
    }
}

bool TileCursor::isVisible() const {
    return m_outlineSegments[0] != nullptr && m_outlineSegments[0]->isVisible();
}

}  // namespace levelbuilder