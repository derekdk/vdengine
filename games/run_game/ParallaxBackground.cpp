#include "ParallaxBackground.h"

#include "CloudLayer.h"
#include "FoliageLayer.h"
#include "MountainLayer.h"
#include "RoadLayer.h"
#include "SkyLayer.h"
#include "WaterLayer.h"

namespace rungame {

ParallaxBackground::ParallaxBackground(vde::Scene& scene) {
    m_layers.reserve(6);
    m_layers.push_back(std::make_unique<SkyLayer>(scene));
    m_layers.push_back(std::make_unique<CloudLayer>(scene));
    m_layers.push_back(std::make_unique<MountainLayer>(scene));
    m_layers.push_back(std::make_unique<WaterLayer>(scene));
    m_layers.push_back(std::make_unique<FoliageLayer>(scene));
    m_layers.push_back(std::make_unique<RoadLayer>(scene));
}

void ParallaxBackground::update(float deltaTime, float runSpeed) {
    for (auto& layer : m_layers) {
        layer->update(deltaTime, runSpeed);
    }
}

void ParallaxBackground::reset() {
    for (auto& layer : m_layers) {
        layer->reset();
    }
}

}  // namespace rungame
