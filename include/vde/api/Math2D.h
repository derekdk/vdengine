#pragma once

/**
 * @file Math2D.h
 * @brief Common 2D gameplay math helpers
 *
 * Header-only free functions for 2D math operations that appear
 * repeatedly in gameplay code. Operates on glm::vec2 and bridges
 * to Position where needed.
 */

#include <vde/api/GameTypes.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace vde::math2d {

// ============================================================================
// Scalar helpers
// ============================================================================

/**
 * @brief Clamp a value to [minValue, maxValue].
 */
inline float clamp(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

/**
 * @brief Clamp a value to [0, 1].
 */
inline float saturate(float value) {
    return clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Linear interpolation between two values.
 */
inline float lerp(float from, float to, float t) {
    return from + (to - from) * t;
}

/**
 * @brief Inverse linear interpolation: returns where value falls in [from, to].
 * @return 0 when value == from, 1 when value == to.
 */
inline float inverseLerp(float from, float to, float value) {
    float range = to - from;
    if (std::abs(range) < 0.0001f)
        return 0.0f;
    return (value - from) / range;
}

/**
 * @brief Check if a float is near zero.
 */
inline bool nearlyZero(float value, float epsilon = 0.0001f) {
    return std::abs(value) < epsilon;
}

/**
 * @brief Check if two floats are approximately equal.
 */
inline bool nearlyEqual(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// Vector helpers
// ============================================================================

/**
 * @brief Squared length of a 2D vector (avoids sqrt).
 */
inline float lengthSquared(const glm::vec2& value) {
    return glm::dot(value, value);
}

/**
 * @brief Squared distance between two points (avoids sqrt).
 */
inline float distanceSquared(const glm::vec2& a, const glm::vec2& b) {
    glm::vec2 d = b - a;
    return glm::dot(d, d);
}

/**
 * @brief Normalize a vector, returning zero if nearly zero length.
 */
inline glm::vec2 normalizeOrZero(const glm::vec2& value, float epsilon = 0.0001f) {
    float len2 = lengthSquared(value);
    if (len2 < epsilon * epsilon)
        return glm::vec2(0.0f);
    return value / std::sqrt(len2);
}

/**
 * @brief Linear interpolation between two 2D vectors.
 */
inline glm::vec2 lerp(const glm::vec2& from, const glm::vec2& to, float t) {
    return from + (to - from) * t;
}

/**
 * @brief Move current toward target by at most maxDelta.
 */
inline glm::vec2 moveToward(const glm::vec2& current, const glm::vec2& target, float maxDelta) {
    glm::vec2 diff = target - current;
    float dist2 = lengthSquared(diff);
    if (dist2 <= maxDelta * maxDelta || dist2 < 0.0001f * 0.0001f)
        return target;
    return current + diff / std::sqrt(dist2) * maxDelta;
}

/**
 * @brief Perpendicular vector rotated 90 degrees left (counter-clockwise).
 */
inline glm::vec2 perpendicularLeft(const glm::vec2& value) {
    return glm::vec2(-value.y, value.x);
}

/**
 * @brief Perpendicular vector rotated 90 degrees right (clockwise).
 */
inline glm::vec2 perpendicularRight(const glm::vec2& value) {
    return glm::vec2(value.y, -value.x);
}

/**
 * @brief Convert an angle in degrees to a unit direction vector.
 *
 * 0 degrees points up (+Y), 90 degrees points right (+X).
 */
inline glm::vec2 directionFromAngleDegrees(float angleDegrees) {
    float rad = glm::radians(angleDegrees);
    return glm::vec2(std::sin(rad), std::cos(rad));
}

/**
 * @brief Get the angle in degrees from the up direction (+Y) to a direction vector.
 *
 * Returns [0, 360) in clockwise order: 0=up, 90=right, 180=down, 270=left.
 */
inline float angleDegreesFromUp(const glm::vec2& direction) {
    float rad = std::atan2(direction.x, direction.y);
    float deg = glm::degrees(rad);
    if (deg < 0.0f)
        deg += 360.0f;
    return deg;
}

/**
 * @brief Get the angle in degrees from the right direction (+X).
 *
 * Returns [-180, 180]. Standard math convention: 0=right, 90=up.
 */
inline float angleDegreesFromRight(const glm::vec2& direction) {
    return glm::degrees(std::atan2(direction.y, direction.x));
}

// ============================================================================
// Type conversion helpers
// ============================================================================

/**
 * @brief Convert a 2D vector to a 3D Position (XY plane, z = given value).
 */
inline Position toPosition(const glm::vec2& value, float z = 0.0f) {
    return Position(value.x, value.y, z);
}

/**
 * @brief Extract the XY components of a Position as a vec2.
 */
inline glm::vec2 toVec2(const Position& value) {
    return glm::vec2(value.x, value.y);
}

}  // namespace vde::math2d
