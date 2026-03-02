/**
 * @file WipeTransition.cpp
 * @brief Implementation of WipeTransition.
 */

#include <vde/api/WipeTransition.h>

namespace vde {

std::string WipeTransition::getFragmentShaderPath() const {
    return "transition_wipe.frag";
}

void WipeTransition::update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) {
    outUniforms.progress = ctx.progress;
    outUniforms.direction = static_cast<float>(m_direction);
}

}  // namespace vde
