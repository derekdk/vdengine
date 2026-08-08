#include "TilePalette.h"

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <string>

namespace {

constexpr int kPaletteColumns = 8;
constexpr float kTileDisplayScale = 0.72f;
constexpr float kTileGap = 0.14f;
constexpr float kPanelPadding = 0.22f;
constexpr float kHeaderHeight = 0.22f;
constexpr float kFooterHeight = 0.20f;
constexpr float kSectionGap = 0.12f;
constexpr float kScreenMargin = 0.35f;
constexpr float kPanelDepth = 1.05f;
constexpr float kTileDepth = 1.55f;
constexpr float kTextDepth = 1.75f;
constexpr float kSelectionDepth = 1.85f;
constexpr float kSelectionPadding = 0.07f;
constexpr float kSelectionThickness = 0.07f;

vde::Color paletteColor() {
    return {0.025f, 0.055f, 0.09f, 0.94f};
}

}  // namespace

namespace levelbuilder {

void TilePalette::initialize(vde::Scene& scene) {
    if (m_initialized) {
        return;
    }

    m_scene = &scene;

    m_panel = scene.addEntity<vde::SpriteEntity>();
    m_panel->setAnchor(0.5f, 0.5f);
    m_panel->setColor(paletteColor());

    m_titleText = scene.addEntity<vde::TextEntity>();
    m_titleText->setFont(vde::BitmapFont::small());
    m_titleText->setStyle({.color = vde::Color(0.84f, 0.92f, 0.98f, 1.0f), .pixelScale = 1});
    m_titleText->setAnchor(0.0f, 0.5f);
    m_titleText->setText("TILE PALETTE");
    m_titleText->setWorldHeight(kHeaderHeight);

    m_currentText = scene.addEntity<vde::TextEntity>();
    m_currentText->setFont(vde::BitmapFont::small());
    m_currentText->setStyle({.color = vde::Color::fromHex(0xffd36b), .pixelScale = 1});
    m_currentText->setAnchor(0.0f, 0.5f);
    m_currentText->setWorldHeight(kFooterHeight);

    m_initialized = true;
    createEntries();
    updateCurrentText();
    updateVisibility();
}

void TilePalette::setTileSet(std::shared_ptr<vde::SpriteSheet> tileSet, float tileWidth,
                             float tileHeight) {
    m_tileSet = std::move(tileSet);
    m_tileWidth = std::max(tileWidth, 0.01f);
    m_tileHeight = std::max(tileHeight, 0.01f);

    if (!m_initialized) {
        return;
    }

    for (const auto& tile : m_tileSprites) {
        if (tile != nullptr && m_scene != nullptr) {
            m_scene->removeEntity(tile->getId());
        }
    }
    for (const auto& frame : m_selectionFrames) {
        for (const auto& segment : frame) {
            if (segment != nullptr && m_scene != nullptr) {
                m_scene->removeEntity(segment->getId());
            }
        }
    }
    m_tileSprites.clear();
    m_selectionFrames.clear();

    createEntries();
    updateCurrentText();
    updateVisibility();
}

void TilePalette::setCurrentTile(std::optional<int> tileId) {
    m_currentTile = tileId;
    updateCurrentText();
    updateSelectionFrame();
}

void TilePalette::show() {
    m_visible = true;
    updateVisibility();
}

void TilePalette::hide() {
    m_visible = false;
    updateVisibility();
}

void TilePalette::updatePosition(const vde::Rect2D& visibleRect) {
    if (!m_initialized || m_tileSprites.empty()) {
        return;
    }

    const float width = paletteWidth();
    const float height = paletteHeight();
    const glm::vec2 center(visibleRect.right - (width * 0.5f) - kScreenMargin,
                           visibleRect.top - (height * 0.5f) - kScreenMargin);
    const float left = center.x - (width * 0.5f);
    const float top = center.y + (height * 0.5f);
    const float tileWidth = m_tileWidth * kTileDisplayScale;
    const float tileHeight = m_tileHeight * kTileDisplayScale;
    m_panel->setScale(width, height, 1.0f);
    const int columnCount = std::min(kPaletteColumns, static_cast<int>(m_tileSprites.size()));

    m_panel->setPosition(center.x, center.y, kPanelDepth);
    m_titleText->setPosition(left + kPanelPadding, top - kPanelPadding - (kHeaderHeight * 0.5f),
                             kTextDepth);

    const float tileTop = top - kPanelPadding - kHeaderHeight - kSectionGap;
    for (size_t index = 0; index < m_tileSprites.size(); ++index) {
        const int column = static_cast<int>(index) % columnCount;
        const int row = static_cast<int>(index) / columnCount;
        const float tileCenterX = left + kPanelPadding + (tileWidth * 0.5f) +
                                  (static_cast<float>(column) * (tileWidth + kTileGap));
        const float tileCenterY =
            tileTop - (tileHeight * 0.5f) - (static_cast<float>(row) * (tileHeight + kTileGap));

        m_tileSprites.at(index)->setPosition(tileCenterX, tileCenterY, kTileDepth);

        auto& frame = m_selectionFrames.at(index);
        const float horizontalLength = tileWidth + (kSelectionPadding * 2.0f);
        const float verticalLength = tileHeight + (kSelectionPadding * 2.0f);
        const float frameX = tileCenterX;
        const float frameY = tileCenterY;
        frame.at(0)->setPosition(frameX, frameY + (verticalLength * 0.5f), kSelectionDepth);
        frame.at(0)->setScale(horizontalLength, kSelectionThickness, 1.0f);
        frame.at(1)->setPosition(frameX, frameY - (verticalLength * 0.5f), kSelectionDepth);
        frame.at(1)->setScale(horizontalLength, kSelectionThickness, 1.0f);
        frame.at(2)->setPosition(frameX - (horizontalLength * 0.5f), frameY, kSelectionDepth);
        frame.at(2)->setScale(kSelectionThickness, verticalLength, 1.0f);
        frame.at(3)->setPosition(frameX + (horizontalLength * 0.5f), frameY, kSelectionDepth);
        frame.at(3)->setScale(kSelectionThickness, verticalLength, 1.0f);
    }

    const int rowCount =
        (static_cast<int>(m_tileSprites.size()) + kPaletteColumns - 1) / kPaletteColumns;
    const float tileGridHeight = (static_cast<float>(rowCount) * tileHeight) +
                                 (static_cast<float>(std::max(0, rowCount - 1)) * kTileGap);
    const float footerY = top - kPanelPadding - kHeaderHeight - kSectionGap - tileGridHeight -
                          kSectionGap - (kFooterHeight * 0.5f);
    m_currentText->setPosition(left + kPanelPadding, footerY, kTextDepth);
}

void TilePalette::createEntries() {
    if (!m_initialized || m_scene == nullptr || m_tileSet == nullptr) {
        return;
    }

    const auto texture = m_tileSet->getTexture();
    const int tileCount = std::max(0, m_tileSet->getSpriteCount());
    m_tileSprites.reserve(static_cast<size_t>(tileCount));
    m_selectionFrames.reserve(static_cast<size_t>(tileCount));

    for (int tileId = 0; tileId < tileCount; ++tileId) {
        const auto uv = m_tileSet->getUVRect(tileId);
        auto tile = m_scene->addEntity<vde::SpriteEntity>();
        tile->setTexture(texture);
        tile->setUVRect(uv.u, uv.v, uv.width, uv.height);
        tile->setAnchor(0.5f, 0.5f);
        tile->setScale(m_tileWidth * kTileDisplayScale, m_tileHeight * kTileDisplayScale, 1.0f);
        m_tileSprites.push_back(tile);

        Frame frame;
        for (auto& segment : frame) {
            segment = m_scene->addEntity<vde::SpriteEntity>();
            segment->setAnchor(0.5f, 0.5f);
            segment->setColor(vde::Color::fromHex(0xffd36b));
            segment->setVisible(false);
        }
        m_selectionFrames.push_back(std::move(frame));
    }
}

void TilePalette::updateVisibility() {
    if (!m_initialized) {
        return;
    }

    m_panel->setVisible(m_visible);
    m_titleText->setVisible(m_visible);
    m_currentText->setVisible(m_visible);
    for (const auto& tile : m_tileSprites) {
        tile->setVisible(m_visible);
    }
    updateSelectionFrame();
}

void TilePalette::updateSelectionFrame() {
    for (auto& frame : m_selectionFrames) {
        for (auto& segment : frame) {
            segment->setVisible(false);
        }
    }

    if (!m_visible || !m_currentTile.has_value()) {
        return;
    }

    const int tileId = m_currentTile.value();
    if (tileId < 0) {
        return;
    }
    const auto tileIndex = static_cast<size_t>(tileId);
    if (tileIndex >= m_selectionFrames.size()) {
        return;
    }

    for (auto& segment : m_selectionFrames.at(tileIndex)) {
        segment->setVisible(true);
    }
}

void TilePalette::updateCurrentText() {
    if (m_currentText == nullptr) {
        return;
    }

    if (!m_currentTile.has_value()) {
        m_currentText->setText("CUT: NONE");
    } else if (m_currentTile.value() < 0) {
        m_currentText->setText("CUT: EMPTY");
    } else {
        m_currentText->setText("CUT: " + std::to_string(m_currentTile.value()));
    }
}

float TilePalette::paletteWidth() const {
    const int tileCount = static_cast<int>(m_tileSprites.size());
    const int columnCount = std::min(kPaletteColumns, tileCount);
    if (columnCount <= 0) {
        return 0.0f;
    }

    const float tileWidth = m_tileWidth * kTileDisplayScale;
    return (kPanelPadding * 2.0f) + (static_cast<float>(columnCount) * tileWidth) +
           (static_cast<float>(columnCount - 1) * kTileGap);
}

float TilePalette::paletteHeight() const {
    const int tileCount = static_cast<int>(m_tileSprites.size());
    if (tileCount <= 0) {
        return 0.0f;
    }

    const int rowCount = (tileCount + kPaletteColumns - 1) / kPaletteColumns;
    const float tileHeight = m_tileHeight * kTileDisplayScale;
    const float tileGridHeight = (static_cast<float>(rowCount) * tileHeight) +
                                 (static_cast<float>(std::max(0, rowCount - 1)) * kTileGap);
    return (kPanelPadding * 2.0f) + kHeaderHeight + kSectionGap + tileGridHeight + kSectionGap +
           kFooterHeight;
}

}  // namespace levelbuilder