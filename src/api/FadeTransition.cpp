/**
 * @file FadeTransition.cpp
 * @brief Implementation of FadeTransition.
 */

#include <vde/api/FadeTransition.h>

namespace vde {

std::string FadeTransition::getFragmentShaderPath() const {
    return "transition_fade.frag";
}

}  // namespace vde
