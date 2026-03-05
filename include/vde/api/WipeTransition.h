#pragma once

/**
 * @file WipeTransition.h
 * @brief Built-in directional wipe scene transition.
 *
 * @example
 * @code
 * transitionToScene("credits",
 *                   std::make_unique<vde::WipeTransition>(vde::TransitionDirection::Left),
 *                   0.8f);
 * @endcode
 */

#include <vde/api/Transition.h>

namespace vde {

/**
 * @brief Linear wipe in the configured direction.
 */
class WipeTransition : public Transition {
  public:
    /**
     * @brief Construct a wipe transition with the given direction.
     * @param dir Wipe direction (default: Left)
     */
    explicit WipeTransition(TransitionDirection dir = TransitionDirection::Left) {
        m_direction = dir;
    }

    const char* getName() const override { return "Wipe"; }
    std::string getFragmentShaderPath() const override;

    /**
     * @brief Encodes direction into uniforms so the shader can branch.
     */
    void update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) override;
};

}  // namespace vde
