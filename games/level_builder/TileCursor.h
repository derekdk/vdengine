#pragma once

#include <glm/vec2.hpp>

#include <memory>

namespace vde {
class Scene;
class SpriteEntity;
}  // namespace vde

namespace levelbuilder {

class TileCursor {
  public:
    void initialize(vde::Scene& scene);
    void show(const glm::vec2& center, float tileWidth, float tileHeight);
    void hide();

    [[nodiscard]] bool isVisible() const;

  private:
    std::shared_ptr<vde::SpriteEntity> m_outline;
};

}  // namespace levelbuilder