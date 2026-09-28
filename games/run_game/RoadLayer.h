#pragma once

#include "ParallaxLayer.h"

namespace rungame {

class RoadLayer : public ParallaxLayer {
  public:
    explicit RoadLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
};

}  // namespace rungame
