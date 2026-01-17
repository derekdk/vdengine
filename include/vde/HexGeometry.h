#pragma once

/**
 * @file HexGeometry.h
 * @brief Generates vertex and index data for hexagon geometry.
 */

#include <vector>
#include <glm/glm.hpp>
#include <vde/Types.h>

namespace vde {

/**
 * @brief Hexagon orientation types.
 * 
 * Flat-top: Flat edge at top (common for strategy games)
 * Pointy-top: Point at top
 */
enum class HexOrientation {
    FlatTop,    ///< Flat edge at top (standard for strategy games)
    PointyTop   ///< Point at top
};

/**
 * @brief Mesh data for a hexagon.
 */
struct HexMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @brief Generates vertex and index data for hexagon geometry.
 * 
 * Creates hexagons using a center vertex plus 6 corner vertices,
 * forming 6 triangles. Supports both flat-top and pointy-top orientations.
 * 
 * For a flat-top hex:
 * - Width (tip to tip) = 2 * size
 * - Height (flat to flat) = sqrt(3) * size
 * 
 * For a pointy-top hex:
 * - Width (flat to flat) = sqrt(3) * size
 * - Height (tip to tip) = 2 * size
 */
class HexGeometry {
public:
    /**
     * @brief Construct a HexGeometry generator.
     * @param size Outer radius (center to corner distance)
     * @param orientation Flat-top or pointy-top orientation
     */
    explicit HexGeometry(float size = 1.0f, 
                         HexOrientation orientation = HexOrientation::FlatTop);
    
    /**
     * @brief Generate a hex mesh centered at the given world position.
     * @param center World position of the hex center
     * @return HexMesh containing vertices and indices
     */
    HexMesh generateHex(const glm::vec3& center = glm::vec3(0.0f)) const;
    
    /**
     * @brief Get the corner positions without generating a full mesh.
     * @param center World position of the hex center
     * @return Vector of 6 corner positions
     */
    std::vector<glm::vec3> getCornerPositions(const glm::vec3& center = glm::vec3(0.0f)) const;
    
    // Dimension accessors
    
    /** @brief Get the size (outer radius) */
    float getSize() const { return m_size; }
    
    /** @brief Get the width (tip to tip for flat-top, flat to flat for pointy-top) */
    float getWidth() const;
    
    /** @brief Get the height (flat to flat for flat-top, tip to tip for pointy-top) */
    float getHeight() const;
    
    /** @brief Get the orientation */
    HexOrientation getOrientation() const { return m_orientation; }

private:
    float m_size;  ///< Outer radius (center to corner)
    HexOrientation m_orientation;
    
    /**
     * @brief Get the starting angle for corner 0.
     * @return Angle in radians
     */
    float getStartAngle() const;
    
    /**
     * @brief Calculate UV coordinates for a point relative to center.
     * @param localPos Position relative to hex center
     * @return UV coordinates in [0, 1] range
     */
    glm::vec2 calculateUV(const glm::vec2& localPos) const;
};

} // namespace vde
