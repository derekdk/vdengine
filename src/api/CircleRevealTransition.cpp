/**
 * @file CircleRevealTransition.cpp
 * @brief Implementation of CircleRevealTransition.
 */

#include <vde/api/CircleRevealTransition.h>

namespace vde {

std::string CircleRevealTransition::getFragmentShaderPath() const {
    return "transition_circle_reveal.frag";
}

void CircleRevealTransition::update(const TransitionUpdateContext& ctx,
                                    TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = static_cast<float>(m_direction);
    if (ctx.frameHeight > 0) {
        outUniforms.param0 =
            static_cast<float>(ctx.frameWidth) / static_cast<float>(ctx.frameHeight);
    } else {
        outUniforms.param0 = 1.0f;
    }
}

}  // namespace vde
