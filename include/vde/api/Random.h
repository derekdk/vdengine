#pragma once

/**
 * @file Random.h
 * @brief Seedable randomness for gameplay, tests, and scripted runs
 *
 * Provides a deterministic random stream backed by std::mt19937.
 * Use explicit seeds for reproducible tests and replays.
 */

#include <vde/api/WorldBounds.h>

#include <glm/glm.hpp>

#include <cmath>
#include <random>

namespace vde {

/**
 * @brief Seedable random number stream.
 *
 * Backed by std::mt19937 for deterministic, portable behavior.
 * Construct with a fixed seed for reproducible results, or use
 * fromEntropy() for non-deterministic seeding.
 */
class RandomStream {
  public:
    /**
     * @brief Construct with a fixed seed (deterministic).
     */
    explicit RandomStream(uint32_t seed = 0u) : m_seed(seed), m_engine(seed) {}

    /**
     * @brief Construct with entropy-based seeding (non-deterministic).
     */
    static RandomStream fromEntropy() {
        std::random_device rd;
        return RandomStream(rd());
    }

    /**
     * @brief Re-seed the generator.
     */
    void reseed(uint32_t seed) {
        m_seed = seed;
        m_engine.seed(seed);
    }

    /**
     * @brief Get the current seed.
     */
    uint32_t seed() const { return m_seed; }

    /**
     * @brief Random float in [0, 1).
     */
    float unit() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(m_engine);
    }

    /**
     * @brief Random float in [minValue, maxValue].
     */
    float range(float minValue, float maxValue) {
        std::uniform_real_distribution<float> dist(minValue, maxValue);
        return dist(m_engine);
    }

    /**
     * @brief Random integer in [minInclusive, maxInclusive].
     */
    int rangeInt(int minInclusive, int maxInclusive) {
        std::uniform_int_distribution<int> dist(minInclusive, maxInclusive);
        return dist(m_engine);
    }

    /**
     * @brief Return true with the given probability [0, 1].
     */
    bool chance(float probability) { return unit() < probability; }

    /**
     * @brief Random unit direction on the 2D unit circle.
     */
    glm::vec2 unitDirection2D() {
        float angle = range(0.0f, 6.28318530718f);
        return glm::vec2(std::cos(angle), std::sin(angle));
    }

    /**
     * @brief Random point inside a 2D bounds.
     */
    glm::vec2 inside(const WorldBounds2D& bounds) {
        return glm::vec2(range(bounds.minX.value, bounds.maxX.value),
                         range(bounds.minY.value, bounds.maxY.value));
    }

  private:
    uint32_t m_seed;
    std::mt19937 m_engine;
};

}  // namespace vde
