#pragma once

#include <array>
#include <memory>
#include <vector>

#include <vde/api/GameAPI.h>

namespace rungame {

class ParallaxBackground {
  public:
    explicit ParallaxBackground(vde::Scene& scene);

    void update(float deltaTime, float runSpeed);
    void reset();

  private:
    enum class Layer : std::size_t {
        Clouds,
        Mountains,
        Water,
        Foliage,
        Road,
        Count,
    };

    struct LayerState {
        float speedFactor;
        float offset = 0.0f;
    };

    struct Piece {
        std::shared_ptr<vde::SpriteEntity> sprite;
        Layer layer;
        float x;
        float y;
        float z;
        float width;
        float height;
        float roll;
        float bobAmplitude;
        float bobFrequency;
        float phase;
        int segment;
    };

    void createWorld();
    void createSky();
    void createClouds();
    void createMountains();
    void createWater();
    void createFoliage();
    void createRoad();
    void addPiece(Layer layer, float x, float y, float width, float height,
                  const vde::Color& color, float z, float roll = 0.0f,
                  float bobAmplitude = 0.0f, float bobFrequency = 0.0f);
    void addStaticPiece(float x, float y, float width, float height,
                        const vde::Color& color, float z);

    static constexpr float kSegmentWidth = 24.0f;

    vde::Scene& m_scene;
    std::array<LayerState, static_cast<std::size_t>(Layer::Count)> m_layers{{
        {0.12f, 0.0f},
        {0.24f, 0.0f},
        {0.42f, 0.0f},
        {0.72f, 0.0f},
        {1.0f, 0.0f},
    }};
    std::vector<Piece> m_pieces;
    float m_time = 0.0f;
};

}  // namespace rungame
