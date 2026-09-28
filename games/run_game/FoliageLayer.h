#pragma once

#include "ParallaxLayer.h"

namespace rungame {

class FoliageLayer : public ParallaxLayer {
  public:
    explicit FoliageLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
};

}  // namespace rungame
