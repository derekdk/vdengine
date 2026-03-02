/**
 * @file Transition.cpp
 * @brief Implementation of Transition base class.
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

}  // namespace vde
