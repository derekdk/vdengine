#pragma once

#include <vde/api/GameAPI.h>

#include <memory>
#include <vector>

namespace rungame {

class ParallaxLayer {
  public:
    virtual ~ParallaxLayer() = default;
    virtual void update(float deltaTime, float runSpeed) = 0;
    virtual void reset();

  protected:
    explicit ParallaxLayer(vde::Scene& scene, float speedFactor);

    void advanceSprites(float deltaTime, float runSpeed);
    std::shared_ptr<vde::SpriteEntity> addSprite(float x, float y, float width, float height,
                                                 const vde::Color& color, float z,
                                                 float roll = 0.0f, float bobAmplitude = 0.0f,
                                                 float bobFrequency = 0.0f, float phase = 0.0f);
    std::shared_ptr<vde::SpriteEntity> addStaticSprite(float x, float y, float width, float height,
                                                       const vde::Color& color, float z);

    static constexpr float kSegmentWidth = 24.0f;

    vde::Scene& m_scene;
    float m_speedFactor;
    float m_offset = 0.0f;
    float m_time = 0.0f;

  private:
    struct SpriteMotion {
        std::shared_ptr<vde::SpriteEntity> sprite;
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

    std::vector<SpriteMotion> m_sprites;
};

}  // namespace rungame
