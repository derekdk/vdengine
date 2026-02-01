#pragma once

/**
 * @file WorldUnits.h
 * @brief Type-safe world units and coordinate system definitions
 * 
 * Provides explicit unit types (meters) and cardinal direction mapping
 * to make coordinate systems self-documenting and less error-prone.
 * 
 * VDE uses a right-handed coordinate system by default:
 * - North = +Z, South = -Z
 * - East = +X, West = -X  
 * - Up = +Y, Down = -Y
 * 
 * @example
 * @code
 * using namespace vde;
 * 
 * // Type-safe distances with user-defined literals
 * Meters distance = 100_m;
 * Meters halfDist = distance / 2;  // 50 meters
 * 
 * // Create a point 100m north, 50m east, 20m up from origin
 * WorldPoint pt = WorldPoint::fromDirections(100_m, 50_m, 20_m);
 * @endcode
 */

#include <glm/glm.hpp>

namespace vde {

/**
 * @brief Defines how cardinal directions map to coordinate axes.
 * 
 * VDE uses a right-handed coordinate system by default:
 * - North = +Z, South = -Z
 * - East = +X, West = -X  
 * - Up = +Y, Down = -Y
 * 
 * Alternative coordinate systems (e.g., Z-up for CAD/GIS) can be
 * configured using the presets.
 */
struct CoordinateSystem {
    glm::vec3 north{0.0f, 0.0f, 1.0f};   ///< Direction of North (+Z default)
    glm::vec3 east{1.0f, 0.0f, 0.0f};    ///< Direction of East (+X default)
    glm::vec3 up{0.0f, 1.0f, 0.0f};      ///< Direction of Up (+Y default)
    
    /// @return Direction of South (opposite of North)
    glm::vec3 south() const { return -north; }
    /// @return Direction of West (opposite of East)
    glm::vec3 west() const { return -east; }
    /// @return Direction of Down (opposite of Up)
    glm::vec3 down() const { return -up; }
    
    /**
     * @brief Y-up coordinate system (VDE default).
     * 
     * - North = +Z
     * - East = +X
     * - Up = +Y
     */
    static CoordinateSystem yUp() {
        CoordinateSystem cs;
        cs.north = glm::vec3(0.0f, 0.0f, 1.0f);
        cs.east = glm::vec3(1.0f, 0.0f, 0.0f);
        cs.up = glm::vec3(0.0f, 1.0f, 0.0f);
        return cs;
    }
    
    /**
     * @brief Z-up coordinate system (CAD/GIS style).
     * 
     * - North = +Y
     * - East = +X
     * - Up = +Z
     */
    static CoordinateSystem zUp() {
        CoordinateSystem cs;
        cs.north = glm::vec3(0.0f, 1.0f, 0.0f);
        cs.east = glm::vec3(1.0f, 0.0f, 0.0f);
        cs.up = glm::vec3(0.0f, 0.0f, 1.0f);
        return cs;
    }
};

/**
 * @brief Type-safe distance in meters.
 * 
 * Use this to make world coordinate units explicit in APIs.
 * Implicit conversion from/to float for convenience, but the type
 * documents intent and prevents unit confusion.
 * 
 * @example
 * @code
 * Meters distance = 100_m;           // Using literal
 * Meters other = Meters(50.0f);      // Explicit construction
 * float raw = distance;              // Implicit conversion to float
 * Meters sum = distance + other;     // Arithmetic operations
 * @endcode
 */
struct Meters {
    float value;
    
    constexpr Meters() : value(0.0f) {}
    constexpr Meters(float v) : value(v) {}
    constexpr operator float() const { return value; }
    
    constexpr Meters operator-() const { return Meters(-value); }
    constexpr Meters operator+(Meters other) const { return Meters(value + other.value); }
    constexpr Meters operator-(Meters other) const { return Meters(value - other.value); }
    constexpr Meters operator*(float scalar) const { return Meters(value * scalar); }
    constexpr Meters operator/(float scalar) const { return Meters(value / scalar); }
    
    Meters& operator+=(Meters other) { value += other.value; return *this; }
    Meters& operator-=(Meters other) { value -= other.value; return *this; }
    Meters& operator*=(float scalar) { value *= scalar; return *this; }
    Meters& operator/=(float scalar) { value /= scalar; return *this; }
    
    constexpr bool operator==(Meters other) const { return value == other.value; }
    constexpr bool operator!=(Meters other) const { return value != other.value; }
    constexpr bool operator<(Meters other) const { return value < other.value; }
    constexpr bool operator<=(Meters other) const { return value <= other.value; }
    constexpr bool operator>(Meters other) const { return value > other.value; }
    constexpr bool operator>=(Meters other) const { return value >= other.value; }
    
    /// @return Absolute value
    constexpr Meters abs() const { return Meters(value < 0 ? -value : value); }
};

/// User-defined literal for meters (e.g., 100_m)
constexpr Meters operator""_m(long double v) { return Meters(static_cast<float>(v)); }
/// User-defined literal for meters from integer (e.g., 100_m)
constexpr Meters operator""_m(unsigned long long v) { return Meters(static_cast<float>(v)); }

/**
 * @brief A 3D point in world space with explicit meter units.
 * 
 * Provides both direct coordinate access and directional constructors
 * for clarity when working with real-world coordinate systems.
 * 
 * @example
 * @code
 * // Direct construction
 * WorldPoint pt(10_m, 5_m, 20_m);
 * 
 * // From cardinal directions (north, east, up)
 * WorldPoint pt2 = WorldPoint::fromDirections(100_m, 50_m, 20_m);
 * 
 * // Convert to GLM vector for rendering
 * glm::vec3 v = pt.toVec3();
 * @endcode
 */
struct WorldPoint {
    Meters x, y, z;
    
    WorldPoint() = default;
    WorldPoint(Meters x, Meters y, Meters z) : x(x), y(y), z(z) {}
    explicit WorldPoint(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
    
    /// @return GLM vec3 for use with rendering APIs
    glm::vec3 toVec3() const { return glm::vec3(x, y, z); }
    
    /**
     * @brief Create point from cardinal directions (uses default Y-up system).
     * 
     * @param northSouth Positive = north (+Z), negative = south (-Z)
     * @param eastWest Positive = east (+X), negative = west (-X)
     * @param upDown Positive = up (+Y), negative = down (-Y)
     * @return WorldPoint in the default coordinate system
     */
    static WorldPoint fromDirections(Meters northSouth, Meters eastWest, Meters upDown) {
        // Default Y-up: X=east, Y=up, Z=north
        return WorldPoint(eastWest, upDown, northSouth);
    }
    
    /**
     * @brief Create point using custom coordinate system.
     * 
     * @param northSouth Distance in north/south direction
     * @param eastWest Distance in east/west direction
     * @param upDown Distance in up/down direction
     * @param coords The coordinate system to use
     * @return WorldPoint transformed by the coordinate system
     */
    static WorldPoint fromDirections(Meters northSouth, Meters eastWest, Meters upDown,
                                      const CoordinateSystem& coords) {
        glm::vec3 pos = coords.north * northSouth.value +
                        coords.east * eastWest.value +
                        coords.up * upDown.value;
        return WorldPoint(pos);
    }
    
    WorldPoint operator+(const WorldPoint& other) const {
        return WorldPoint(x + other.x, y + other.y, z + other.z);
    }
    
    WorldPoint operator-(const WorldPoint& other) const {
        return WorldPoint(x - other.x, y - other.y, z - other.z);
    }
    
    WorldPoint operator*(float scalar) const {
        return WorldPoint(x * scalar, y * scalar, z * scalar);
    }
};

/**
 * @brief A 3D size/extent in world space with explicit meter units.
 * 
 * Represents dimensions rather than positions. Uses width/height/depth
 * naming for clarity in game development contexts.
 */
struct WorldExtent {
    Meters width;   ///< X dimension (east-west span)
    Meters height;  ///< Y dimension (up-down span)
    Meters depth;   ///< Z dimension (north-south span)
    
    WorldExtent() = default;
    WorldExtent(Meters w, Meters h, Meters d) : width(w), height(h), depth(d) {}
    
    /**
     * @brief Create a 2D extent (no height dimension).
     * 
     * @param width East-west span
     * @param depth North-south span
     * @return WorldExtent with height = 0
     */
    static WorldExtent flat(Meters width, Meters depth) {
        return WorldExtent(width, Meters(0.0f), depth);
    }
    
    /// @return GLM vec3 for use with rendering APIs
    glm::vec3 toVec3() const { return glm::vec3(width, height, depth); }
    
    /// @return True if this is a 2D extent (height == 0)
    bool is2D() const { return height.value == 0.0f; }
    
    /// @return Volume in cubic meters (0 for 2D extents)
    float volume() const { return width.value * height.value * depth.value; }
    
    /// @return Area of the base (width * depth) in square meters
    float baseArea() const { return width.value * depth.value; }
};

} // namespace vde
