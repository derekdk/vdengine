#pragma once

/**
 * @file Easing.h
 * @brief Easing curves for animations.
 *
 * Provides the AnimationEasing enum and evaluateEasing() for converting a
 * linear progress value in [0, 1] to a curved value using standard easing
 * algorithms.
 *
 * All built-in evaluations are allocation-free and deterministic.
 * The Custom entry delegates to a stored callable in AnimationOptions.
 */

#include <cmath>
#include <cstdint>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Standard easing algorithms for animations.
 *
 * @note This enum is the canonical easing vocabulary for VDE.
 *       Overlay sheets, transitions, and other subsystems should
 *       reference AnimationEasing rather than defining a separate enum.
 */
enum class AnimationEasing : uint8_t {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseOutBack,
    EaseOutBounce,
    EaseOutElastic,
    Custom  ///< Delegates to the custom easing function in AnimationOptions
};

/**
 * @brief Evaluate an easing curve at the given progress.
 *
 * @param easing The easing algorithm to apply.
 * @param t      Linear progress in [0, 1].  Values outside this range are clamped.
 * @return Eased progress value (output range depends on the curve; most stay in [0, 1]).
 *
 * @note For AnimationEasing::Custom this function returns @p t unchanged.
 *       The caller is responsible for applying the custom easing callable.
 */
inline float evaluateEasing(AnimationEasing easing, float t) noexcept {
    // Clamp input to [0, 1] before evaluation.
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;

    const float pi = 3.14159265358979323846f;

    switch (easing) {
    case AnimationEasing::Linear:
        return t;

    case AnimationEasing::EaseInQuad:
        return t * t;

    case AnimationEasing::EaseOutQuad:
        return t * (2.0f - t);

    case AnimationEasing::EaseInOutQuad:
        return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);

    case AnimationEasing::EaseInCubic:
        return t * t * t;

    case AnimationEasing::EaseOutCubic: {
        float u = t - 1.0f;
        return 1.0f + u * u * u;
    }

    case AnimationEasing::EaseInOutCubic:
        if (t < 0.5f) {
            return 4.0f * t * t * t;
        } else {
            float u = 2.0f * t - 2.0f;
            return 1.0f + 0.5f * u * u * u;
        }

    case AnimationEasing::EaseInSine:
        return 1.0f - std::cos(t * pi * 0.5f);

    case AnimationEasing::EaseOutSine:
        return std::sin(t * pi * 0.5f);

    case AnimationEasing::EaseInOutSine:
        return 0.5f * (1.0f - std::cos(t * pi));

    case AnimationEasing::EaseOutBack: {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        float u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }

    case AnimationEasing::EaseOutBounce: {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            float u = t - 1.5f / d1;
            return n1 * u * u + 0.75f;
        } else if (t < 2.5f / d1) {
            float u = t - 2.25f / d1;
            return n1 * u * u + 0.9375f;
        } else {
            float u = t - 2.625f / d1;
            return n1 * u * u + 0.984375f;
        }
    }

    case AnimationEasing::EaseOutElastic: {
        const float c4 = 2.0f * pi / 3.0f;
        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }

    case AnimationEasing::Custom:
        return t;  // Caller applies the custom callable.
    }

    return t;
}

}  // namespace vde
