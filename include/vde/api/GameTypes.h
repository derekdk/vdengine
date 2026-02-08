#pragma once

/**
 * @file GameTypes.h
 * @brief Common types for the VDE Game API
 *
 * Contains fundamental data structures used by games including
 * colors, positions, directions, and other common types.
 */

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace vde {

/**
 * @brief Represents an RGBA color.
 */
struct Color {
    float r, g, b, a;

    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    /**
     * @brief Create color from 8-bit components (0-255).
     */
    static Color fromRGB8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    /**
     * @brief Create color from hex value (0xRRGGBB or 0xRRGGBBAA).
     */
    static Color fromHex(uint32_t hex) {
        if (hex > 0xFFFFFF) {
            // Has alpha
            return Color(((hex >> 24) & 0xFF) / 255.0f, ((hex >> 16) & 0xFF) / 255.0f,
                         ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f);
        }
        return Color(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f,
                     (hex & 0xFF) / 255.0f, 1.0f);
    }

    glm::vec3 toVec3() const { return glm::vec3(r, g, b); }
    glm::vec4 toVec4() const { return glm::vec4(r, g, b, a); }

    // Predefined colors
    static Color white() { return Color(1.0f, 1.0f, 1.0f); }
    static Color black() { return Color(0.0f, 0.0f, 0.0f); }
    static Color red() { return Color(1.0f, 0.0f, 0.0f); }
    static Color green() { return Color(0.0f, 1.0f, 0.0f); }
    static Color blue() { return Color(0.0f, 0.0f, 1.0f); }
    static Color yellow() { return Color(1.0f, 1.0f, 0.0f); }
    static Color cyan() { return Color(0.0f, 1.0f, 1.0f); }
    static Color magenta() { return Color(1.0f, 0.0f, 1.0f); }
};

/**
 * @brief Represents a 3D position in world space.
 */
struct Position {
    float x, y, z;

    Position() : x(0.0f), y(0.0f), z(0.0f) {}
    Position(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Position(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

    glm::vec3 toVec3() const { return glm::vec3(x, y, z); }

    Position operator+(const Position& other) const {
        return Position(x + other.x, y + other.y, z + other.z);
    }

    Position operator-(const Position& other) const {
        return Position(x - other.x, y - other.y, z - other.z);
    }

    Position operator*(float scalar) const { return Position(x * scalar, y * scalar, z * scalar); }
};

/**
 * @brief Represents a 3D direction vector.
 */
struct Direction {
    float x, y, z;

    Direction() : x(0.0f), y(0.0f), z(-1.0f) {}
    Direction(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Direction(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

    glm::vec3 toVec3() const { return glm::vec3(x, y, z); }

    /**
     * @brief Returns a normalized version of this direction.
     */
    Direction normalized() const {
        glm::vec3 n = glm::normalize(toVec3());
        return Direction(n.x, n.y, n.z);
    }

    // Common directions
    static Direction forward() { return Direction(0.0f, 0.0f, -1.0f); }
    static Direction back() { return Direction(0.0f, 0.0f, 1.0f); }
    static Direction up() { return Direction(0.0f, 1.0f, 0.0f); }
    static Direction down() { return Direction(0.0f, -1.0f, 0.0f); }
    static Direction left() { return Direction(-1.0f, 0.0f, 0.0f); }
    static Direction right() { return Direction(1.0f, 0.0f, 0.0f); }
};

/**
 * @brief Represents 3D rotation in Euler angles (degrees).
 */
struct Rotation {
    float pitch;  ///< Rotation around X axis
    float yaw;    ///< Rotation around Y axis
    float roll;   ///< Rotation around Z axis

    Rotation() : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
    Rotation(float pitch, float yaw, float roll) : pitch(pitch), yaw(yaw), roll(roll) {}

    glm::vec3 toVec3() const { return glm::vec3(pitch, yaw, roll); }
};

/**
 * @brief Represents a 3D scale factor.
 */
struct Scale {
    float x, y, z;

    Scale() : x(1.0f), y(1.0f), z(1.0f) {}
    Scale(float uniform) : x(uniform), y(uniform), z(uniform) {}
    Scale(float x, float y, float z) : x(x), y(y), z(z) {}

    glm::vec3 toVec3() const { return glm::vec3(x, y, z); }

    static Scale uniform(float s) { return Scale(s, s, s); }
};

/**
 * @brief Represents a transform with position, rotation, and scale.
 */
struct Transform {
    Position position;
    Rotation rotation;
    Scale scale;

    Transform() = default;
    Transform(const Position& pos) : position(pos) {}
    Transform(const Position& pos, const Rotation& rot) : position(pos), rotation(rot) {}
    Transform(const Position& pos, const Rotation& rot, const Scale& scl)
        : position(pos), rotation(rot), scale(scl) {}

    /**
     * @brief Get the model matrix for this transform.
     */
    glm::mat4 getMatrix() const;
};

/**
 * @brief Unique identifier for resources.
 */
using ResourceId = uint64_t;

/**
 * @brief Invalid resource ID constant.
 */
constexpr ResourceId INVALID_RESOURCE_ID = 0;

/**
 * @brief Unique identifier for entities.
 */
using EntityId = uint64_t;

/**
 * @brief Invalid entity ID constant.
 */
constexpr EntityId INVALID_ENTITY_ID = 0;

}  // namespace vde
