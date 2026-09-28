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

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Represents an RGBA color.
 */
struct Color {
    float r, g, b, a;

    constexpr Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    /**
     * @brief Create color from 8-bit components (0-255).
     */
    static constexpr Color fromRGB8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    /**
     * @brief Create color from hex value (0xRRGGBB or 0xRRGGBBAA).
     */
    static constexpr Color fromHex(uint32_t hex) {
        if (hex > 0xFFFFFF) {
            // Has alpha
            return {((hex >> 24) & 0xFF) / 255.0f, ((hex >> 16) & 0xFF) / 255.0f,
                    ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f};
        }
        return {((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f,
                1.0f};
    }

    bool operator==(const Color&) const = default;

    [[nodiscard]] glm::vec3 toVec3() const { return {r, g, b}; }
    [[nodiscard]] glm::vec4 toVec4() const { return {r, g, b, a}; }

    // Predefined colors
    static constexpr Color white() { return {1.0f, 1.0f, 1.0f}; }
    static constexpr Color black() { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Color red() { return {1.0f, 0.0f, 0.0f}; }
    static constexpr Color green() { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Color blue() { return {0.0f, 0.0f, 1.0f}; }
    static constexpr Color yellow() { return {1.0f, 1.0f, 0.0f}; }
    static constexpr Color cyan() { return {0.0f, 1.0f, 1.0f}; }
    static constexpr Color magenta() { return {1.0f, 0.0f, 1.0f}; }
};

/**
 * @brief Represents a 3D position in world space.
 */
struct Position {
    float x, y, z;

    Position() : x(0.0f), y(0.0f), z(0.0f) {}
    Position(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Position(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

    [[nodiscard]] glm::vec3 toVec3() const { return {x, y, z}; }

    Position operator+(const Position& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    Position operator-(const Position& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    Position operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
};

/**
 * @brief Represents a 3D direction vector.
 */
struct Direction {
    float x, y, z;

    Direction() : x(0.0f), y(0.0f), z(-1.0f) {}
    Direction(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Direction(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

    [[nodiscard]] glm::vec3 toVec3() const { return {x, y, z}; }

    /**
     * @brief Returns a normalized version of this direction.
     */
    [[nodiscard]] Direction normalized() const {
        glm::vec3 n = glm::normalize(toVec3());
        return {n.x, n.y, n.z};
    }

    // Common directions
    static Direction forward() { return {0.0f, 0.0f, -1.0f}; }
    static Direction back() { return {0.0f, 0.0f, 1.0f}; }
    static Direction up() { return {0.0f, 1.0f, 0.0f}; }
    static Direction down() { return {0.0f, -1.0f, 0.0f}; }
    static Direction left() { return {-1.0f, 0.0f, 0.0f}; }
    static Direction right() { return {1.0f, 0.0f, 0.0f}; }
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

    [[nodiscard]] glm::vec3 toVec3() const { return {pitch, yaw, roll}; }
};

/**
 * @brief Represents a 3D scale factor.
 */
struct Scale {
    float x, y, z;

    Scale() : x(1.0f), y(1.0f), z(1.0f) {}
    Scale(float uniform) : x(uniform), y(uniform), z(uniform) {}
    Scale(float x, float y, float z) : x(x), y(y), z(z) {}

    [[nodiscard]] glm::vec3 toVec3() const { return {x, y, z}; }

    static Scale uniform(float s) { return {s, s, s}; }
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
    [[nodiscard]] glm::mat4 getMatrix() const;
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
