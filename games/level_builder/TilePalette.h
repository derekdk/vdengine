#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace vde {
class Scene;
class SpriteEntity;
class SpriteSheet;
class TextEntity;
struct Rect2D;
}  // namespace vde

namespace levelbuilder {

class TilePalette {
  public:
    void initialize(vde::Scene& scene);
    void setTileSet(std::shared_ptr<vde::SpriteSheet> tileSet, float tileWidth, float tileHeight);
    void setCurrentTile(std::optional<int> tileId);

    void show();
    void hide();
    void updatePosition(const vde::Rect2D& visibleRect);

    [[nodiscard]] bool isVisible() const { return m_visible; }

  private:
    using Frame = std::array<std::shared_ptr<vde::SpriteEntity>, 4>;

    void createEntries();
    void updateVisibility();
    void updateSelectionFrame();
    void updateCurrentText();
    [[nodiscard]] float paletteWidth() const;
    [[nodiscard]] float paletteHeight() const;

    vde::Scene* m_scene = nullptr;
    std::shared_ptr<vde::SpriteSheet> m_tileSet;
    std::shared_ptr<vde::SpriteEntity> m_panel;
    std::shared_ptr<vde::TextEntity> m_titleText;
    std::shared_ptr<vde::TextEntity> m_currentText;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_tileSprites;
    std::vector<Frame> m_selectionFrames;
    std::optional<int> m_currentTile;
    float m_tileWidth = 1.0f;
    float m_tileHeight = 1.0f;
    bool m_initialized = false;
    bool m_visible = false;
};

}  // namespace levelbuilder