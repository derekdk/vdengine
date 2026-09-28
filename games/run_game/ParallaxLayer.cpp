#include "ParallaxLayer.h"

#include <cmath>

namespace rungame {

namespace {

constexpr int kFirstSegment = -2;
constexpr int kLastSegment = 2;

}  // namespace

ParallaxLayer::ParallaxLayer(vde::Scene& scene, float speedFactor)
    : m_scene(scene), m_speedFactor(speedFactor) {}

void ParallaxLayer::reset() {
    m_offset = 0.0f;
    m_time = 0.0f;
    advanceSprites(0.0f, 0.0f);
}

void ParallaxLayer::advanceSprites(float deltaTime, float runSpeed) {
    m_time += deltaTime;
    m_offset = std::fmod(m_offset + runSpeed * m_speedFactor * deltaTime, kSegmentWidth);
    for (auto& motion : m_sprites) {
        const float x = motion.x + static_cast<float>(motion.segment) * kSegmentWidth - m_offset;
        const float y = motion.y +
                        motion.bobAmplitude * std::sin(m_time * motion.bobFrequency + motion.phase);
        motion.sprite->setPosition(x, y, motion.z);
        motion.sprite->setScale(motion.width, motion.height, 1.0f);
        motion.sprite->setRotation(0.0f, 0.0f, motion.roll);
    }
}

std::shared_ptr<vde::SpriteEntity> ParallaxLayer::addSprite(
    float x, float y, float width, float height, const vde::Color& color,
    float z, float roll, float bobAmplitude, float bobFrequency, float phase) {
    std::shared_ptr<vde::SpriteEntity> first;
    m_sprites.reserve(m_sprites.size() + kLastSegment - kFirstSegment + 1);
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        auto sprite = m_scene.addEntity<vde::SpriteEntity>();
        sprite->setColor(color);
        const float segmentX = x + static_cast<float>(segment) * kSegmentWidth;
        sprite->setPosition(segmentX, y, z);
        sprite->setScale(width, height, 1.0f);
        sprite->setRotation(0.0f, 0.0f, roll);
        if (!first) {
            first = sprite;
        }
        m_sprites.push_back({std::move(sprite), x, y, z, width, height, roll,
                             bobAmplitude, bobFrequency, phase, segment});
    }
    return first;
}

std::shared_ptr<vde::SpriteEntity> ParallaxLayer::addStaticSprite(
    float x, float y, float width, float height, const vde::Color& color, float z) {
    auto sprite = m_scene.addEntity<vde::SpriteEntity>();
    sprite->setPosition(x, y, z);
    sprite->setScale(width, height, 1.0f);
    sprite->setColor(color);
    return sprite;
}

}  // namespace rungame
