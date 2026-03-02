#pragma once

/**
 * @file CircleRevealTransition.h
 * @brief Built-in center-out circle reveal scene transition.
 *
 * @example
 * @code
 * transitionToScene("game", std::make_unique<vde::CircleRevealTransition>(), 1.5f);
 * @endcode
 */

#include <vde/api/Transition.h>

namespace vde {

/**
 * @brief Expanding circle from the center revealing the destination.
 */
class CircleRevealTransition : public Transition {
  public:
    CircleRevealTransition() { m_direction = TransitionDirection::Center; }

    const char* getName() const override { return "CircleReveal"; }
    std::string getFragmentShaderPath() const override;

    /**
     * @brief Maps linear progress to a radius and encodes it in uniforms.
     */
    void update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) override;
};

}  // namespace vde
