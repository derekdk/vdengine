/**
 * @file BlockFallTransition.cpp
 * @brief Implementation of BlockFallTransition.
 */

#include <vde/api/BlockFallTransition.h>

#include <algorithm>

namespace vde {

std::string BlockFallTransition::getFragmentShaderPath() const {
    return "transition_block_fall.frag";
}

void BlockFallTransition::update(const TransitionUpdateContext& ctx,
                                 TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = m_randomSeed;

    const float safeBlockSize = std::max(1.0f, m_blockSizePixels);
    const float width = static_cast<float>(ctx.frameWidth);
    const float height = static_cast<float>(ctx.frameHeight);

    outUniforms.param0 = (width > 0.0f) ? (safeBlockSize / width) : 1.0f;
    outUniforms.param1 = (height > 0.0f) ? (safeBlockSize / height) : 1.0f;
}

}  // namespace vde
