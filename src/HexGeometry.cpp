#include <vde/HexGeometry.h>
#include <cmath>

namespace vde {

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
}

HexGeometry::HexGeometry(float size, HexOrientation orientation)
    : m_size(size)
    , m_orientation(orientation)
{
}

float HexGeometry::getStartAngle() const {
    // Flat-top: first corner at 0° (right side)
    // Pointy-top: first corner at 30°
    return (m_orientation == HexOrientation::FlatTop) ? 0.0f : 30.0f * DEG_TO_RAD;
}

float HexGeometry::getWidth() const {
    // For flat-top: width = 2 * size (tip to tip)
    // For pointy-top: width = sqrt(3) * size (flat to flat)
    if (m_orientation == HexOrientation::FlatTop) {
        return 2.0f * m_size;
    } else {
        return std::sqrt(3.0f) * m_size;
    }
}

float HexGeometry::getHeight() const {
    // For flat-top: height = sqrt(3) * size (flat to flat)
    // For pointy-top: height = 2 * size (tip to tip)
    if (m_orientation == HexOrientation::FlatTop) {
        return std::sqrt(3.0f) * m_size;
    } else {
        return 2.0f * m_size;
    }
}

std::vector<glm::vec3> HexGeometry::getCornerPositions(const glm::vec3& center) const {
    std::vector<glm::vec3> corners;
    corners.reserve(6);
    
    float startAngle = getStartAngle();
    
    for (int i = 0; i < 6; i++) {
        float angle = startAngle + i * (PI / 3.0f);  // 60 degrees between corners
        // Generate corners in XZ plane (Y is up)
        float x = center.x + m_size * std::cos(angle);
        float z = center.z + m_size * std::sin(angle);
        corners.emplace_back(x, center.y, z);
    }
    
    return corners;
}

glm::vec2 HexGeometry::calculateUV(const glm::vec2& localPos) const {
    // Map local position to [0, 1] UV range
    // Local positions range from [-size, size] in both axes
    float u = (localPos.x / m_size + 1.0f) * 0.5f;
    float v = (localPos.y / m_size + 1.0f) * 0.5f;
    
    return glm::vec2(u, v);
}

HexMesh HexGeometry::generateHex(const glm::vec3& center) const {
    HexMesh mesh;
    
    // Using center + 6 corners = 7 vertices
    // 6 triangles from center to each edge
    mesh.vertices.reserve(7);
    mesh.indices.reserve(18);  // 6 triangles * 3 indices
    
    // Default color (white for texture tinting)
    glm::vec3 color(1.0f, 1.0f, 1.0f);
    
    // Center vertex (index 0)
    glm::vec2 centerUV = calculateUV(glm::vec2(0.0f, 0.0f));
    mesh.vertices.push_back({center, color, centerUV});
    
    // Corner vertices (indices 1-6)
    std::vector<glm::vec3> corners = getCornerPositions(center);
    for (int i = 0; i < 6; i++) {
        // Use XZ for local position (hexes are in XZ plane)
        glm::vec2 localPos(corners[i].x - center.x, corners[i].z - center.z);
        glm::vec2 uv = calculateUV(localPos);
        mesh.vertices.push_back({corners[i], color, uv});
    }
    
    // Indices for 6 triangles (center + 2 adjacent corners each)
    // Wind counter-clockwise for front-facing (Vulkan default)
    for (int i = 0; i < 6; i++) {
        uint32_t current = static_cast<uint32_t>(i + 1);
        uint32_t next = static_cast<uint32_t>((i + 1) % 6 + 1);
        
        mesh.indices.push_back(0);        // Center
        mesh.indices.push_back(current);  // Current corner
        mesh.indices.push_back(next);     // Next corner
    }
    
    return mesh;
}

} // namespace vde
