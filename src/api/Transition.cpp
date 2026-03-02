/**
 * @file Transition.cpp
 * @brief Implementation of Transition base class and built-in transitions.
 */

#include <vde/api/Transition.h>

namespace vde {

// =========================================================================
// Transition base class
// =========================================================================

std::string Transition::getVertexShaderPath() const {
    return "transition_fullscreen.vert";
}

void Transition::update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = static_cast<float>(m_direction);
}

// =========================================================================
// FadeTransition
// =========================================================================

std::string FadeTransition::getFragmentShaderPath() const {
    return "transition_fade.frag";
}

// =========================================================================
// WipeTransition
// =========================================================================

std::string WipeTransition::getFragmentShaderPath() const {
    return "transition_wipe.frag";
}

void WipeTransition::update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = static_cast<float>(m_direction);
}

// =========================================================================
// CircleRevealTransition
// =========================================================================

std::string CircleRevealTransition::getFragmentShaderPath() const {
    return "transition_circle_reveal.frag";
}

void CircleRevealTransition::update(const TransitionUpdateContext& ctx,
                                    TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = static_cast<float>(m_direction);
    // param0 = aspect ratio correction so the circle is round on non-square viewports
    if (ctx.frameHeight > 0) {
        outUniforms.param0 =
            static_cast<float>(ctx.frameWidth) / static_cast<float>(ctx.frameHeight);
    } else {
        outUniforms.param0 = 1.0f;
    }
}

}  // namespace vde
