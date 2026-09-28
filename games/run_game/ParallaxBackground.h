#pragma once

#include <memory>
#include <vector>

#include "ParallaxLayer.h"

namespace rungame {

class ParallaxBackground {
  public:
    explicit ParallaxBackground(vde::Scene& scene);

    void update(float deltaTime, float runSpeed);
    void reset();

  private:
    std::vector<std::unique_ptr<ParallaxLayer>> m_layers;
};

}  // namespace rungame
