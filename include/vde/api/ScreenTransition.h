#pragma once

/**
 * @file ScreenTransition.h
 * @brief Screen transition effects for scene changes
 *
 * Provides transition types and state management for animated
 * scene transitions such as fade-to-black, crossfade, and slides.
 */

#include <functional>
#include <string>

namespace vde {

/**
 * @brief Types of transition effects available for scene changes.
 *
 * Used with Game::setActiveScene() to animate the transition
 * between scenes instead of an instant switch.
 */
enum class TransitionType {
    NONE,         ///< Instant switch (current behavior)
    FADE_BLACK,   ///< Fade to black, then fade in new scene
    CROSSFADE,    ///< Blend old and new scenes (reserved for future use)
    SLIDE_LEFT,   ///< Slide new scene in from the right (reserved for future use)
    SLIDE_RIGHT,  ///< Slide new scene in from the left (reserved for future use)
};

/**
 * @brief Phase of a running transition.
 */
enum class TransitionPhase {
    NONE,        ///< No transition active
    FADING_OUT,  ///< Old scene is fading out
    FADING_IN,   ///< New scene is fading in
};

/**
 * @brief Tracks the state of an active screen transition.
 *
 * Manages the timing and phase progression for animated scene
 * transitions.  The Game class owns a TransitionState and updates
 * it each frame when a transition is in progress.
 *
 * The overlay alpha value (0.0–1.0) can be used by renderers to
 * draw a full-screen overlay or otherwise modulate the scene output:
 * - 0.0 = fully transparent (no overlay)
 * - 1.0 = fully opaque (e.g., solid black for FADE_BLACK)
 */
struct TransitionState {
    /// The type of transition being performed.
    TransitionType type = TransitionType::NONE;

    /// Current phase of the transition.
    TransitionPhase phase = TransitionPhase::NONE;

    /// Name of the scene being transitioned to.
    std::string targetScene;

    /// Duration of each half of the transition in seconds.
    /// Total transition time is halfDuration * 2.
    float halfDuration = 0.25f;

    /// Elapsed time in the current phase (seconds).
    float elapsed = 0.0f;

    /// Current overlay alpha (0.0 = transparent, 1.0 = opaque).
    /// Computed from elapsed / halfDuration.
    float overlayAlpha = 0.0f;

    /// Optional callback invoked when the transition completes.
    std::function<void()> onComplete;

    /**
     * @brief Check whether a transition is currently active.
     * @return true if a transition is in progress
     */
    bool isActive() const { return phase != TransitionPhase::NONE; }

    /**
     * @brief Reset the transition state to idle.
     */
    void reset();

    /**
     * @brief Start a new transition.
     * @param transitionType The transition effect to use
     * @param target Name of the destination scene
     * @param duration Total duration of the transition in seconds
     */
    void start(TransitionType transitionType, const std::string& target, float duration);

    /**
     * @brief Advance the transition by one frame.
     * @param deltaTime Time since last frame in seconds
     * @return true if the midpoint was reached this frame (scene should switch)
     */
    bool update(float deltaTime);
};

}  // namespace vde
