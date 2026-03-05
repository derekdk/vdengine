#pragma once

/**
 * @file FadeTransition.h
 * @brief Built-in cross-fade scene transition.
 *
 * @example
 * @code
 * transitionToScene("menu", std::make_unique<vde::FadeTransition>(), 1.0f);
 * @endcode
 */

#include <vde/api/Transition.h>

namespace vde {

/**
 * @brief Cross-fade (alpha blend) between source and destination.
 */
class FadeTransition : public Transition {
  public:
    const char* getName() const override { return "Fade"; }
    std::string getFragmentShaderPath() const override;
};

}  // namespace vde
