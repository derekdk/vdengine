#pragma once

/**
 * @file Tween.h
 * @brief Typed interpolation helpers for animation values.
 *
 * Provides tweenValue<T>() for interpolating common VDE types between a
 * start and end value using a pre-evaluated eased progress in [0, 1].
 *
 * Supported types: float, glm::vec2, glm::vec3, glm::vec4, Color,
 *                  Position, Scale.
 *
 * Rotation is intentionally excluded in v1 because VDE uses Euler angles
 * and a rushed interpolation rule would be hard to undo.
 */

#include <vde/api/GameTypes.h>

#include <glm/glm.hpp>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Linearly interpolate between @p from and @p to using @p t.
 *
 * @param from           Start value (t = 0).
 * @param to             End value (t = 1).
 * @param easedProgress  Pre-evaluated easing result in [0, 1].
 * @return Interpolated value.
 */
template <typename T>
T tweenValue(const T& from, const T& to, float easedProgress) {
    // If you hit this assertion, T is not a supported tween type.
    // Add an explicit specialization for T in Tween.h.
    static_assert(sizeof(T) == 0, "tweenValue<T>: unsupported type. "
                                  "Add an explicit specialization in include/vde/Tween.h.");
    (void)from;
    (void)to;
    (void)easedProgress;
    return from;
}

// ---------------------------------------------------------------------------
// float
// ---------------------------------------------------------------------------

template <>
inline float tweenValue<float>(const float& from, const float& to, float t) {
    return from + (to - from) * t;
}

// ---------------------------------------------------------------------------
// glm::vec2
// ---------------------------------------------------------------------------

template <>
inline glm::vec2 tweenValue<glm::vec2>(const glm::vec2& from, const glm::vec2& to, float t) {
    return from + (to - from) * t;
}

// ---------------------------------------------------------------------------
// glm::vec3
// ---------------------------------------------------------------------------

template <>
inline glm::vec3 tweenValue<glm::vec3>(const glm::vec3& from, const glm::vec3& to, float t) {
    return from + (to - from) * t;
}

// ---------------------------------------------------------------------------
// glm::vec4
// ---------------------------------------------------------------------------

template <>
inline glm::vec4 tweenValue<glm::vec4>(const glm::vec4& from, const glm::vec4& to, float t) {
    return from + (to - from) * t;
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

template <>
inline Color tweenValue<Color>(const Color& from, const Color& to, float t) {
    return Color{from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
                 from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t};
}

// ---------------------------------------------------------------------------
// Position
// ---------------------------------------------------------------------------

template <>
inline Position tweenValue<Position>(const Position& from, const Position& to, float t) {
    return Position{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                    from.z + (to.z - from.z) * t};
}

// ---------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------

template <>
inline Scale tweenValue<Scale>(const Scale& from, const Scale& to, float t) {
    return Scale{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                 from.z + (to.z - from.z) * t};
}

}  // namespace vde
