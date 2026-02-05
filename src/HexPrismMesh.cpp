#include <vde/HexPrismMesh.h>

#include <cmath>

namespace vde {

namespace {
// Helper constants for flat-top hexagon
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_60 = PI / 3.0f;  // 60 degrees in radians
}  // namespace

void HexPrismMeshGenerator::generate(float hexRadius, std::vector<HexPrismVertex>& outVertices,
                                     std::vector<uint32_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    // Reserve space for efficiency
    outVertices.reserve(getVertexCount());
    outIndices.reserve(getIndexCount());

    // Generate top face (y = 1.0)
    generateTopFace(hexRadius, outVertices, outIndices, 0);

    // Generate bottom face (y = 0.0)
    uint32_t bottomBaseIndex = static_cast<uint32_t>(outVertices.size());
    generateBottomFace(hexRadius, outVertices, outIndices, bottomBaseIndex);

    // Generate side faces connecting top and bottom
    uint32_t sideBaseIndex = static_cast<uint32_t>(outVertices.size());
    generateSideFaces(hexRadius, outVertices, outIndices, sideBaseIndex);
}

void HexPrismMeshGenerator::generateTopFace(float radius, std::vector<HexPrismVertex>& verts,
                                            std::vector<uint32_t>& indices, uint32_t baseIndex) {
    const glm::vec3 normal(0.0f, 1.0f, 0.0f);  // Pointing up
    const float y = 1.0f;                      // Top of unit prism
    const uint8_t faceType = 0;                // Top face

    // Center vertex
    verts.emplace_back(glm::vec3(0.0f, y, 0.0f), normal,
                       glm::vec2(0.5f, 0.5f),  // Center of UV space
                       faceType);

    // Corner vertices
    for (int i = 0; i < 6; ++i) {
        glm::vec3 pos = getCornerPosition(radius, i, y);
        glm::vec2 uv = calculateUV(glm::vec2(pos.x, pos.z), radius);
        verts.emplace_back(pos, normal, uv, faceType);
    }

    // Generate triangles (fan from center)
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nextCorner = (i + 1) % 6;

        // Triangle: center -> corner i -> corner i+1
        indices.push_back(baseIndex);                   // Center
        indices.push_back(baseIndex + 1 + i);           // Current corner
        indices.push_back(baseIndex + 1 + nextCorner);  // Next corner
    }
}

void HexPrismMeshGenerator::generateBottomFace(float radius, std::vector<HexPrismVertex>& verts,
                                               std::vector<uint32_t>& indices, uint32_t baseIndex) {
    const glm::vec3 normal(0.0f, -1.0f, 0.0f);  // Pointing down
    const float y = 0.0f;                       // Bottom of unit prism
    const uint8_t faceType = 2;                 // Bottom face

    // Center vertex
    verts.emplace_back(glm::vec3(0.0f, y, 0.0f), normal,
                       glm::vec2(0.5f, 0.5f),  // Center of UV space
                       faceType);

    // Corner vertices (same XZ positions as top, different Y and winding order)
    for (int i = 0; i < 6; ++i) {
        glm::vec3 pos = getCornerPosition(radius, i, y);
        glm::vec2 uv = calculateUV(glm::vec2(pos.x, pos.z), radius);
        verts.emplace_back(pos, normal, uv, faceType);
    }

    // Generate triangles (fan from center, reversed winding for bottom)
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t nextCorner = (i + 1) % 6;

        // Triangle: center -> corner i+1 -> corner i (reversed winding)
        indices.push_back(baseIndex);                   // Center
        indices.push_back(baseIndex + 1 + nextCorner);  // Next corner
        indices.push_back(baseIndex + 1 + i);           // Current corner
    }
}

void HexPrismMeshGenerator::generateSideFaces(float radius, std::vector<HexPrismVertex>& verts,
                                              std::vector<uint32_t>& indices, uint32_t baseIndex) {
    const uint8_t faceType = 1;  // Side face
    const float topY = 1.0f;
    const float bottomY = 0.0f;

    // Generate 6 rectangular side faces
    for (int i = 0; i < 6; ++i) {
        int nextCorner = (i + 1) % 6;

        // Get corner positions
        glm::vec3 bottomLeft = getCornerPosition(radius, i, bottomY);
        glm::vec3 bottomRight = getCornerPosition(radius, nextCorner, bottomY);
        glm::vec3 topLeft = getCornerPosition(radius, i, topY);
        glm::vec3 topRight = getCornerPosition(radius, nextCorner, topY);

        // Calculate face normal (pointing outward)
        glm::vec3 edge1 = bottomRight - bottomLeft;
        glm::vec3 edge2 = topLeft - bottomLeft;
        glm::vec3 normal =
            glm::normalize(glm::cross(edge2, edge1));  // Swapped order for outward normal

        // UV coordinates for side face (tiled horizontally)
        float uLeft = static_cast<float>(i) / 6.0f;
        float uRight = static_cast<float>(nextCorner) / 6.0f;
        if (uRight == 0.0f)
            uRight = 1.0f;  // Wrap around for last face

        // Add 4 vertices for this quad
        uint32_t quadBase = baseIndex + i * 4;

        verts.emplace_back(bottomLeft, normal, glm::vec2(uLeft, 1.0f), faceType);
        verts.emplace_back(bottomRight, normal, glm::vec2(uRight, 1.0f), faceType);
        verts.emplace_back(topRight, normal, glm::vec2(uRight, 0.0f), faceType);
        verts.emplace_back(topLeft, normal, glm::vec2(uLeft, 0.0f), faceType);

        // Two triangles for this quad
        // Triangle 1: bottom-left, bottom-right, top-right
        indices.push_back(quadBase + 0);
        indices.push_back(quadBase + 1);
        indices.push_back(quadBase + 2);

        // Triangle 2: bottom-left, top-right, top-left
        indices.push_back(quadBase + 0);
        indices.push_back(quadBase + 2);
        indices.push_back(quadBase + 3);
    }
}

glm::vec3 HexPrismMeshGenerator::getCornerPosition(float radius, int cornerIndex, float y) {
    // Flat-top hexagon: corner 0 at angle 0 (pointing right)
    // Corners numbered counter-clockwise
    float angle = static_cast<float>(cornerIndex) * DEG_60;
    float x = radius * std::cos(angle);
    float z = radius * std::sin(angle);
    return glm::vec3(x, y, z);
}

glm::vec2 HexPrismMeshGenerator::calculateUV(const glm::vec2& localPos, float radius) {
    // Map from [-radius, radius] to [0, 1]
    float u = (localPos.x / radius + 1.0f) * 0.5f;
    float v = (localPos.y / radius + 1.0f) * 0.5f;
    return glm::vec2(u, v);
}

}  // namespace vde
