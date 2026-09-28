#pragma once

#include <memory>
#include <vector>

#include "ParallaxLayer.h"

namespace rungame {

class SkyLayer : public ParallaxLayer {
  public:
    explicit SkyLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
    void reset() override;

  private:
    std::shared_ptr<vde::SpriteEntity> m_sky;
    std::shared_ptr<vde::SpriteEntity> m_sun;
    std::shared_ptr<vde::SpriteEntity> m_moon;
    std::vector<std::shared_ptr<vde::SpriteEntity>> m_stars;
};

}  // namespace rungame
