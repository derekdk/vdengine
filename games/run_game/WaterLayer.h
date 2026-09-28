#pragma once

#include "ParallaxLayer.h"

namespace rungame {

class WaterLayer : public ParallaxLayer {
  public:
    explicit WaterLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
};

}  // namespace rungame
