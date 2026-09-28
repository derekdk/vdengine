#pragma once

#include "ParallaxLayer.h"

namespace rungame {

class MountainLayer : public ParallaxLayer {
  public:
    explicit MountainLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
};

}  // namespace rungame
