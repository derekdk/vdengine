#pragma once

/**
 * @file WorldBounds.h
 * @brief World and scene boundary definitions with cardinal directions
 * 
 * Provides axis-aligned bounding box types for defining game world
 * extents using intuitive cardinal direction terminology.
 * 
 * @example
 * @code
 * using namespace vde;
 * 
 * // Scene from 100m north to 100m south, 100m west to 100m east,
 * // 20m up to 10m down = 200x200x30 meters
 * WorldBounds bounds = WorldBounds::fromDirectionalLimits(
 *     100_m,   // north limit
 *     -100_m,  // south limit
 *     -100_m,  // west limit  
 *     100_m,   // east limit
 *     20_m,    // up limit
 *     -10_m    // down limit
 * );
 * 
 * // Or using helper functions for clarity:
 * WorldBounds bounds2 = WorldBounds::fromDirectionalLimits(
 *     100_m, WorldBounds::south(100_m),
 *     WorldBounds::west(100_m), 100_m,
 *     20_m, WorldBounds::down(10_m)
 * );
 * @endcode
 */

#include "WorldUnits.h"
#include <glm/glm.hpp>
#include <algorithm>

namespace vde {

/**
 * @brief Defines a 3D axis-aligned bounding box in world space.
 * 
 * Bounds are internally stored as min/max corners but provide
 * cardinal direction accessors for intuitive game development.
 * 
 * Default coordinate system (Y-up):
 * - X axis: West (-) to East (+)
 * - Y axis: Down (-) to Up (+)
 * - Z axis: South (-) to North (+)
 */
struct WorldBounds {
    WorldPoint min;  ///< Corner with smallest x, y, z values
    WorldPoint max;  ///< Corner with largest x, y, z values
    
    WorldBounds() = default;
    WorldBounds(const WorldPoint& minPt, const WorldPoint& maxPt) : min(minPt), max(maxPt) {}
    
    // Cardinal direction accessors (default Y-up coordinate system)
    
    /// @return Maximum Z value (north limit)
    Meters northLimit() const { return max.z; }
    /// @return Minimum Z value (south limit)  
    Meters southLimit() const { return min.z; }
    /// @return Maximum X value (east limit)
    Meters eastLimit() const { return max.x; }
    /// @return Minimum X value (west limit)
    Meters westLimit() const { return min.x; }
    /// @return Maximum Y value (up/ceiling limit)
    Meters upLimit() const { return max.y; }
    /// @return Minimum Y value (down/floor limit)
    Meters downLimit() const { return min.y; }
    
    // Dimensions
    
    /**
     * @brief Get the full 3D extent of the bounds.
     */
    WorldExtent extent() const {
        return WorldExtent(
            max.x - min.x,  // width (east-west)
            max.y - min.y,  // height (up-down)
            max.z - min.z   // depth (north-south)
        );
    }
    
    /// @return East-west span in meters
    Meters width() const { return max.x - min.x; }
    /// @return Up-down span in meters (0 for 2D bounds)
    Meters height() const { return max.y - min.y; }
    /// @return North-south span in meters
    Meters depth() const { return max.z - min.z; }
    
    /**
     * @brief Get the center point of the bounds.
     */
    WorldPoint center() const {
        return WorldPoint(
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f
        );
    }
    
    /**
     * @brief Check if a point is inside the bounds.
     */
    bool contains(const WorldPoint& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
    
    /**
     * @brief Check if this bounds intersects another.
     */
    bool intersects(const WorldBounds& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    
    /**
     * @brief Check if this is a 2D bounds (no height dimension).
     */
    bool is2D() const { return height().value == 0.0f; }
    
    /**
     * @brief Create bounds from directional limits.
     * 
     * @param northLimit How far north from origin (positive Z)
     * @param southLimit How far south from origin (negative Z, pass as negative)
     * @param westLimit How far west from origin (negative X, pass as negative)
     * @param eastLimit How far east from origin (positive X)
     * @param upLimit How far up from origin (positive Y)
     * @param downLimit How far down from origin (negative Y, pass as negative)
     * @return WorldBounds properly ordered min/max
     */
    static WorldBounds fromDirectionalLimits(
        Meters northLimit, Meters southLimit,
        Meters westLimit, Meters eastLimit,
        Meters upLimit = Meters(0.0f), Meters downLimit = Meters(0.0f)
    ) {
        // Ensure proper min/max ordering regardless of input order
        WorldPoint minPt(
            std::min(westLimit.value, eastLimit.value),
            std::min(downLimit.value, upLimit.value),
            std::min(southLimit.value, northLimit.value)
        );
        WorldPoint maxPt(
            std::max(westLimit.value, eastLimit.value),
            std::max(downLimit.value, upLimit.value),
            std::max(southLimit.value, northLimit.value)
        );
        return WorldBounds(minPt, maxPt);
    }
    
    /**
     * @brief Create bounds centered at a point with given extent.
     * 
     * @param center Center point of the bounds
     * @param extent Size of the bounds
     * @return WorldBounds centered on the point
     */
    static WorldBounds fromCenterAndExtent(const WorldPoint& center, const WorldExtent& extent) {
        Meters halfW = extent.width * 0.5f;
        Meters halfH = extent.height * 0.5f;
        Meters halfD = extent.depth * 0.5f;
        return WorldBounds(
            WorldPoint(center.x - halfW, center.y - halfH, center.z - halfD),
            WorldPoint(center.x + halfW, center.y + halfH, center.z + halfD)
        );
    }
    
    /**
     * @brief Create 2D bounds (height = 0).
     * 
     * @param northLimit Northern boundary
     * @param southLimit Southern boundary (negative)
     * @param westLimit Western boundary (negative)
     * @param eastLimit Eastern boundary
     * @return WorldBounds with zero height
     */
    static WorldBounds flat(Meters northLimit, Meters southLimit,
                            Meters westLimit, Meters eastLimit) {
        return fromDirectionalLimits(northLimit, southLimit, westLimit, eastLimit,
                                     Meters(0.0f), Meters(0.0f));
    }
    
    // Helper functions for negative direction values (for readability)
    
    /// @brief Helper to express south distance (returns negative value)
    static Meters south(Meters m) { return -m; }
    /// @brief Helper to express west distance (returns negative value)
    static Meters west(Meters m) { return -m; }
    /// @brief Helper to express down distance (returns negative value)
    static Meters down(Meters m) { return -m; }
};

/**
 * @brief 2D bounds for flat games (no height dimension).
 * 
 * Simplifies working with top-down or side-scrolling games where
 * the Y axis typically represents the vertical screen dimension
 * rather than world height.
 * 
 * @note For top-down games, Y typically maps to north-south.
 *       For side-scrollers, Y typically maps to up-down.
 */
struct WorldBounds2D {
    Meters minX, minY;  ///< Minimum corner (West/South or Left/Bottom)
    Meters maxX, maxY;  ///< Maximum corner (East/North or Right/Top)
    
    WorldBounds2D() = default;
    WorldBounds2D(Meters minX, Meters minY, Meters maxX, Meters maxY)
        : minX(minX), minY(minY), maxX(maxX), maxY(maxY) {}
    
    /// @return Width (X span) in meters
    Meters width() const { return maxX - minX; }
    /// @return Height (Y span) in meters
    Meters height() const { return maxY - minY; }
    
    /// @return 2D extent
    WorldExtent extent() const { return WorldExtent::flat(width(), height()); }
    
    /**
     * @brief Get the center point.
     */
    glm::vec2 center() const {
        return glm::vec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }
    
    /**
     * @brief Check if a point is inside the bounds.
     */
    bool contains(Meters x, Meters y) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }
    
    /**
     * @brief Check if a point is inside the bounds.
     */
    bool contains(const glm::vec2& point) const {
        return contains(Meters(point.x), Meters(point.y));
    }
    
    /**
     * @brief Create from cardinal limits (for top-down games).
     * 
     * Maps north/south to +Y/-Y and east/west to +X/-X.
     * 
     * @param north Northern limit (positive Y)
     * @param south Southern limit (negative Y)
     * @param west Western limit (negative X)
     * @param east Eastern limit (positive X)
     */
    static WorldBounds2D fromCardinal(
        Meters north, Meters south, Meters west, Meters east
    ) {
        return WorldBounds2D(
            std::min(west.value, east.value),
            std::min(south.value, north.value),
            std::max(west.value, east.value),
            std::max(south.value, north.value)
        );
    }
    
    /**
     * @brief Create from left/right/top/bottom (for side-scrollers).
     * 
     * @param left Left edge (minimum X)
     * @param right Right edge (maximum X)
     * @param top Top edge (maximum Y)
     * @param bottom Bottom edge (minimum Y)
     */
    static WorldBounds2D fromLRTB(
        Meters left, Meters right, Meters top, Meters bottom
    ) {
        return WorldBounds2D(
            std::min(left.value, right.value),
            std::min(bottom.value, top.value),
            std::max(left.value, right.value),
            std::max(bottom.value, top.value)
        );
    }
    
    /**
     * @brief Create centered bounds.
     * 
     * @param centerX Center X coordinate
     * @param centerY Center Y coordinate
     * @param width Total width
     * @param height Total height
     */
    static WorldBounds2D fromCenter(Meters centerX, Meters centerY, 
                                     Meters width, Meters height) {
        Meters halfW = width * 0.5f;
        Meters halfH = height * 0.5f;
        return WorldBounds2D(
            centerX - halfW, centerY - halfH,
            centerX + halfW, centerY + halfH
        );
    }
    
    /**
     * @brief Convert to 3D bounds.
     * 
     * @param upLimit Upper Y limit in 3D space
     * @param downLimit Lower Y limit in 3D space
     * @return WorldBounds with this 2D extent on the XZ plane
     */
    WorldBounds toWorldBounds(Meters upLimit = Meters(0.0f), 
                              Meters downLimit = Meters(0.0f)) const {
        // Map 2D bounds to XZ plane, Y is vertical
        return WorldBounds(
            WorldPoint(minX, downLimit, minY),
            WorldPoint(maxX, upLimit, maxY)
        );
    }
};

} // namespace vde
