#pragma once

/**
 * @file BlockFallTransition.h
 * @brief Built-in random block-fall scene transition.
 *
 * Splits the source scene into fixed-size pixel blocks. Each block begins
 * falling at a random time, revealing the destination scene beneath.
 *
 * @example
 * @code
 * transitionToScene("game", std::make_unique<vde::BlockFallTransition>(), 1.25f);
 * @endcode
 */

#include <vde/api/Transition.h>

namespace vde {

/**
 * @brief Random 32x32 block fall transition.
 */
class BlockFallTransition : public Transition {
  public:
    /**
     * @brief Construct a block-fall transition.
     * @param blockSizePixels Block size in pixels (default: 32)
     * @param randomSeed Seed used by the shader hash (default: 0)
     */
    explicit BlockFallTransition(float blockSizePixels = 32.0f, float randomSeed = 0.0f)
        : m_blockSizePixels(blockSizePixels), m_randomSeed(randomSeed) {}

    const char* getName() const override { return "BlockFall"; }
    std::string getFragmentShaderPath() const override;

    /**
     * @brief Encodes normalized block size and seed into uniforms.
     */
    void update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) override;

    float getBlockSizePixels() const { return m_blockSizePixels; }
    float getRandomSeed() const { return m_randomSeed; }

  private:
    float m_blockSizePixels;
    float m_randomSeed;
};

}  // namespace vde
