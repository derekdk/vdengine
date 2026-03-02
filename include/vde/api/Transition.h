#pragma once

/**
 * @file Transition.h
 * @brief Base transition interface and shared transition data types.
 *
 * Provides the `Transition` base class that custom transitions extend.
 * Built-in effects are declared in dedicated headers for easy discovery:
 * - vde/api/FadeTransition.h
 * - vde/api/WipeTransition.h
 * - vde/api/CircleRevealTransition.h
 * - vde/api/BlockFallTransition.h
 *
 * Transitions are primarily shader-driven: a fullscreen-triangle fragment
 * shader samples source and destination textures with a `progress` uniform.
 */

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace vde {

/// Direction hint for transitions that are not symmetric.
enum class TransitionDirection : uint8_t {
    Left,
    Right,
    Up,
    Down,
    Center  ///< e.g. circle expand from center
};

/// Parameters passed to a transition each frame.
struct TransitionUpdateContext {
    float progress;        ///< 0.0 → 1.0 over the transition duration
    float deltaTime;       ///< Frame delta in seconds
    float elapsed;         ///< Wall-clock time since transition start
    float duration;        ///< Total transition duration (seconds)
    uint32_t frameWidth;   ///< Render target width in pixels
    uint32_t frameHeight;  ///< Render target height in pixels
};

/// GPU-side uniform block written by the transition each frame.
/// Passed as push constants to the transition fragment shader.
struct TransitionUniforms {
    float progress = 0.0f;   ///< Animation progress [0, 1]
    float direction = 0.0f;  ///< Encoded TransitionDirection (float cast)
    float param0 = 0.0f;     ///< Effect-specific parameter
    float param1 = 0.0f;     ///< Effect-specific parameter
};

/**
 * @brief Base class for all screen transitions.
 *
 * Subclass and override `update()` to animate uniforms.
 * Override `getFragmentShaderPath()` to supply a custom
 * fragment shader that samples `sourceTexture` (binding 0)
 * and `destTexture` (binding 1) with a `progress` push constant.
 *
 * For 3D-geometry-based transitions, override
 * `usesCustomGeometry()` and `renderCustomGeometry()`.
 */
class Transition {
  public:
    virtual ~Transition() = default;

    // ---- Identity ----

    /**
     * @brief Human-readable name (for debug UI / logging).
     */
    virtual const char* getName() const = 0;

    // ---- Shader paths ----

    /**
     * @brief Path to the GLSL fragment shader for this transition.
     *
     * The shader receives:
     *   layout(binding = 0) uniform sampler2D sourceTexture;
     *   layout(binding = 1) uniform sampler2D destTexture;
     *   layout(push_constant) TransitionUniforms uniforms;
     */
    virtual std::string getFragmentShaderPath() const = 0;

    /**
     * @brief Vertex shader — the default fullscreen triangle is usually sufficient.
     * Override only for custom geometry.
     */
    virtual std::string getVertexShaderPath() const;

    // ---- Per-frame callback ----

    /**
     * @brief Called every frame while the transition is active.
     *
     * Update `outUniforms` to drive the shader.
     * The default implementation sets `outUniforms.progress = ctx.progress`.
     *
     * @param ctx         Frame context with progress, timing, and resolution
     * @param outUniforms Uniforms to populate for the GPU
     */
    virtual void update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms);

    // ---- Source scene behaviour ----

    /**
     * @brief Return true if the source scene should be frozen (not updated)
     * during this transition.
     *
     * Transitions that break the source image into discrete pieces (e.g. block
     * fall) need a static snapshot so that each piece carries consistent
     * content throughout the animation.  The default is false, which lets the
     * source scene continue animating (appropriate for cross-fades, wipes,
     * etc.).
     */
    virtual bool freezesSourceScene() const { return false; }

    // ---- Lifecycle hooks ----

    /** @brief Called once when the transition begins. */
    virtual void onStart() {}

    /** @brief Called once when the transition completes (progress == 1). */
    virtual void onComplete() {}

    // ---- Geometry override (advanced) ----

    /**
     * @brief Return true if this transition renders its own geometry
     * instead of using the default fullscreen quad.
     */
    virtual bool usesCustomGeometry() const { return false; }

    /**
     * @brief Record custom draw commands.
     *
     * `sourceTexture` and `destTexture` descriptor sets are already bound.
     *
     * @param cmd The active command buffer
     * @param ctx Frame context
     */
    virtual void renderCustomGeometry(VkCommandBuffer cmd, const TransitionUpdateContext& ctx) {
        (void)cmd;
        (void)ctx;
    }

    // ---- Configuration helpers ----

    void setDirection(TransitionDirection dir) { m_direction = dir; }
    TransitionDirection getDirection() const { return m_direction; }

  protected:
    TransitionDirection m_direction = TransitionDirection::Center;
};

}  // namespace vde

// Built-in transitions (kept included here for backward compatibility).
#include <vde/api/BlockFallTransition.h>
#include <vde/api/CircleRevealTransition.h>
#include <vde/api/FadeTransition.h>
#include <vde/api/WipeTransition.h>
