#pragma once

#include "ParallaxLayer.h"

namespace rungame {

class CloudLayer : public ParallaxLayer {
  public:
    explicit CloudLayer(vde::Scene& scene);

    void update(float deltaTime, float runSpeed) override;
};

}  // namespace rungame
