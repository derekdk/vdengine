#include "ParallaxBackground.h"

#include <cmath>
#include <iterator>

namespace rungame {

namespace {

constexpr int kFirstSegment = -2;
constexpr int kLastSegment = 2;

}  // namespace

ParallaxBackground::ParallaxBackground(vde::Scene& scene) : m_scene(scene) {
    m_pieces.reserve(180);
    createWorld();
}

void ParallaxBackground::update(float deltaTime, float runSpeed) {
    m_time += deltaTime;
    for (auto& layer : m_layers) {
        layer.offset = std::fmod(layer.offset + runSpeed * layer.speedFactor * deltaTime,
                                 kSegmentWidth);
    }

    for (auto& piece : m_pieces) {
        const auto& layer = m_layers[static_cast<std::size_t>(piece.layer)];
        const float y = piece.y + piece.bobAmplitude *
                                      std::sin(m_time * piece.bobFrequency + piece.phase);
        const float x = piece.x + static_cast<float>(piece.segment) * kSegmentWidth - layer.offset;
        piece.sprite->setPosition(x, y, piece.z);
        piece.sprite->setScale(piece.width, piece.height, 1.0f);
        piece.sprite->setRotation(0.0f, 0.0f, piece.roll);
    }
}

void ParallaxBackground::reset() {
    m_time = 0.0f;
    for (auto& layer : m_layers) {
        layer.offset = 0.0f;
    }
    update(0.0f, 0.0f);
}

void ParallaxBackground::createWorld() {
    createSky();
    createClouds();
    createMountains();
    createWater();
    createFoliage();
    createRoad();
}

void ParallaxBackground::createSky() {
    addStaticPiece(0.0f, 0.0f, 30.0f, 16.0f, vde::Color::fromHex(0x83c9ef), -0.99f);
    addStaticPiece(-8.5f, 4.6f, 2.2f, 2.2f, vde::Color::fromHex(0xffdf91), -0.96f);

    for (int i = 0; i < 12; ++i) {
        const float x = -10.5f + static_cast<float>((i * 7) % 22);
        const float y = 2.1f + static_cast<float>((i * 5) % 5) * 0.65f;
        const float size = 0.035f + static_cast<float>(i % 3) * 0.02f;
        addStaticPiece(x, y, size, size, vde::Color(1.0f, 1.0f, 0.95f, 0.65f), -0.95f);
    }
}

void ParallaxBackground::createClouds() {
    const auto cloud = vde::Color(0.96f, 0.99f, 1.0f, 0.74f);
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        const float shift = static_cast<float>(segment) * kSegmentWidth;
        const float centers[] = {-9.0f, -1.5f, 7.0f, 13.5f};
        for (std::size_t i = 0; i < std::size(centers); ++i) {
            const float x = centers[i] + shift;
            const float y = 3.2f + static_cast<float>(i % 2) * 0.65f;
            const float size = 0.75f + static_cast<float>(i % 3) * 0.12f;
            addPiece(Layer::Clouds, x - size, y, 1.55f * size, 0.46f * size,
                     vde::Color(0.89f, 0.96f, 1.0f, 0.68f), -0.86f, 0.0f, 0.06f, 0.7f);
            addPiece(Layer::Clouds, x, y + 0.16f, 1.9f * size, 0.62f * size, cloud,
                     -0.85f, 0.0f, 0.08f, 0.7f);
            addPiece(Layer::Clouds, x + size, y - 0.02f, 1.35f * size, 0.44f * size,
                     cloud, -0.86f, 0.0f, 0.05f, 0.7f);
        }
    }
}

void ParallaxBackground::createMountains() {
    const auto farBlue = vde::Color::fromHex(0x7998b1);
    const auto nearBlue = vde::Color::fromHex(0x547d98);
    const auto snow = vde::Color::fromHex(0xe4f2f3);
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        const float shift = static_cast<float>(segment) * kSegmentWidth;
        addPiece(Layer::Mountains, shift, -1.05f, kSegmentWidth + 0.2f, 1.55f,
                 vde::Color::fromHex(0x7196a2), -0.72f);

        for (int i = 0; i < 4; ++i) {
            const float x = -9.0f + static_cast<float>(i) * 6.3f + shift;
            const float peakHeight = 3.1f + static_cast<float>((i + segment + 4) % 3) * 0.55f;
            addPiece(Layer::Mountains, x, -0.05f + peakHeight * 0.27f, 5.7f,
                     peakHeight * 0.56f, farBlue, -0.68f, 35.0f);
            addPiece(Layer::Mountains, x + 1.8f, -0.02f + peakHeight * 0.25f, 5.4f,
                     peakHeight * 0.52f, nearBlue, -0.67f, -33.0f);
            addPiece(Layer::Mountains, x, peakHeight * 0.55f, 0.48f, 0.42f, snow, -0.65f);
        }
    }
}

void ParallaxBackground::createWater() {
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        const float shift = static_cast<float>(segment) * kSegmentWidth;
        addPiece(Layer::Water, shift, -2.25f, kSegmentWidth + 0.2f, 1.5f,
                 vde::Color::fromHex(0x28799a), -0.42f);
        addPiece(Layer::Water, shift, -1.56f, kSegmentWidth + 0.2f, 0.18f,
                 vde::Color(0.77f, 0.92f, 0.91f, 0.48f), -0.40f);
        for (int i = 0; i < 6; ++i) {
            const float x = -9.5f + static_cast<float>(i) * 4.0f + shift;
            const float y = -1.9f - static_cast<float>(i % 3) * 0.36f;
            addPiece(Layer::Water, x, y, 1.2f + static_cast<float>(i % 2) * 0.5f,
                     0.08f, vde::Color(0.60f, 0.88f, 0.94f, 0.7f), -0.38f, 0.0f,
                     0.04f, 1.2f, static_cast<float>(i) * 0.5f);
        }
    }
}

void ParallaxBackground::createFoliage() {
    const auto leaf = vde::Color::fromHex(0x3e733e);
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        const float shift = static_cast<float>(segment) * kSegmentWidth;
        addPiece(Layer::Foliage, shift, -3.25f, kSegmentWidth + 0.2f, 1.05f,
                 vde::Color::fromHex(0x578046), 0.08f);
        for (int i = 0; i < 5; ++i) {
            const float x = -10.0f + static_cast<float>(i) * 5.2f + shift;
            const float trunkHeight = 1.1f + static_cast<float>(i % 3) * 0.18f;
            addPiece(Layer::Foliage, x, -3.0f + trunkHeight * 0.5f, 0.18f,
                     trunkHeight, vde::Color::fromHex(0x775335), 0.10f, 0.0f,
                     0.015f, 1.0f, static_cast<float>(i));
            addPiece(Layer::Foliage, x - 0.28f, -2.05f + trunkHeight * 0.12f, 0.95f,
                     0.74f, leaf, 0.12f, -8.0f, 0.035f, 1.0f, static_cast<float>(i));
            addPiece(Layer::Foliage, x + 0.25f, -1.96f + trunkHeight * 0.12f, 0.98f,
                     0.8f, vde::Color::fromHex(0x4b8645), 0.13f, 7.0f,
                     0.04f, 1.0f, static_cast<float>(i) + 0.5f);
        }
    }
}

void ParallaxBackground::createRoad() {
    for (int segment = kFirstSegment; segment <= kLastSegment; ++segment) {
        const float shift = static_cast<float>(segment) * kSegmentWidth;
        addPiece(Layer::Road, shift, -5.45f, kSegmentWidth + 0.2f, 2.7f,
                 vde::Color::fromHex(0x87603e), 0.32f);
        addPiece(Layer::Road, shift, -4.15f, kSegmentWidth + 0.2f, 0.22f,
                 vde::Color::fromHex(0xe0bd7a), 0.34f);
        for (int i = 0; i < 8; ++i) {
            const float x = -10.0f + static_cast<float>(i) * 3.0f + shift;
            addPiece(Layer::Road, x, -5.3f, 1.3f, 0.11f,
                     vde::Color::fromHex(0xc29761), 0.35f);
        }
    }
}

void ParallaxBackground::addPiece(Layer layer, float x, float y, float width,
                                  float height, const vde::Color& color, float z, float roll,
                                  float bobAmplitude, float bobFrequency, float phase) {
    const int segment =
        static_cast<int>(std::floor((x + kSegmentWidth * 0.5f) / kSegmentWidth));
    const float localX = x - static_cast<float>(segment) * kSegmentWidth;
    auto sprite = m_scene.addEntity<vde::SpriteEntity>();
    sprite->setColor(color);
    sprite->setScale(width, height, 1.0f);
    sprite->setRotation(0.0f, 0.0f, roll);
    sprite->setPosition(x, y, z);
    m_pieces.push_back({sprite, layer, localX, y, z, width, height, roll,
                        bobAmplitude, bobFrequency, phase, segment});
}

void ParallaxBackground::addStaticPiece(float x, float y, float width, float height,
                                        const vde::Color& color, float z) {
    auto sprite = m_scene.addEntity<vde::SpriteEntity>();
    sprite->setPosition(x, y, z);
    sprite->setScale(width, height, 1.0f);
    sprite->setColor(color);
}

}  // namespace rungame
