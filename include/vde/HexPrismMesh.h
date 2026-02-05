#pragma once

/**
 * @file HexPrismMesh.h
 * @brief Generates 3D hexagonal prism mesh geometry.
 */

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace vde {

/**
 * @brief Vertex data for hex prism mesh.
 *
 * Each vertex includes position, normal, texture coordinates, and face type.
 * Face type is used by shaders to select appropriate textures for top vs side faces.
 */
struct HexPrismVertex {
    glm::vec3 position;  ///< Local position (unit prism: y in [0, 1])
    glm::vec3 normal;    ///< Surface normal for lighting
    glm::vec2 texCoord;  ///< UV coordinates for texturing
    uint8_t faceType;    ///< 0=top face, 1=side face, 2=bottom face
    uint8_t padding[3];  ///< Alignment padding to 32 bytes

    HexPrismVertex() = default;

    HexPrismVertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv, uint8_t face)
        : position(pos), normal(norm), texCoord(uv), faceType(face), padding{0, 0, 0} {}

    // Vulkan vertex input binding (binding 0 for vertex data)
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(HexPrismVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    // Vulkan vertex input attributes
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributes{};

        // Position (location 0)
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(HexPrismVertex, position);

        // Normal (location 1)
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(HexPrismVertex, normal);

        // TexCoord (location 2)
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(HexPrismVertex, texCoord);

        // Note: faceType is not passed as a vertex attribute
        // It's embedded in texCoord or can be computed in shader

        return attributes;
    }
};

/**
 * @brief Mesh data containing vertices and indices for a hex prism.
 */
struct HexPrismMeshData {
    std::vector<HexPrismVertex> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @brief Generates 3D hexagonal prism mesh geometry.
 *
 * Creates procedural hex prism meshes with:
 * - Hexagonal top face (6 triangles)
 * - Hexagonal bottom face (6 triangles)
 * - 6 rectangular side faces (12 triangles total)
 *
 * Unit prism has height 1.0, which is scaled per-instance via shaders.
 * Top face at y=1.0, bottom face at y=0.0.
 */
class HexPrismMeshGenerator {
  public:
    /**
     * @brief Generate a hex prism with unit height (1.0).
     *
     * Height is scaled per-instance via shader to allow efficient instancing
     * of terrain layers with different thicknesses.
     *
     * @param hexRadius Outer radius (center to corner distance)
     * @param outVertices Output vertex array
     * @param outIndices Output index array
     */
    static void generate(float hexRadius, std::vector<HexPrismVertex>& outVertices,
                         std::vector<uint32_t>& outIndices);

    /**
     * @brief Get expected vertex count for buffer sizing.
     *
     * Count:
     * - Top face: 7 vertices (1 center + 6 corners)
     * - Bottom face: 7 vertices (1 center + 6 corners)
     * - Side faces: 24 vertices (6 faces * 4 corners per face, no sharing)
     *
     * @return Number of vertices (7 + 7 + 24 = 38)
     */
    static size_t getVertexCount() { return 38; }

    /**
     * @brief Get expected index count for buffer sizing.
     *
     * Count:
     * - Top face: 18 indices (6 triangles * 3 indices)
     * - Bottom face: 18 indices (6 triangles * 3 indices)
     * - Side faces: 36 indices (6 faces * 2 triangles * 3 indices)
     *
     * @return Number of indices (18 + 18 + 36 = 72)
     */
    static size_t getIndexCount() { return 72; }

  private:
    /**
     * @brief Generate the top hexagonal face.
     *
     * Creates a center vertex with 6 corner vertices forming 6 triangles.
     * Normals point up (0, 1, 0). UVs centered in unit square.
     *
     * @param radius Hex outer radius
     * @param verts Output vertex array
     * @param indices Output index array
     * @param baseIndex Starting index for vertices
     */
    static void generateTopFace(float radius, std::vector<HexPrismVertex>& verts,
                                std::vector<uint32_t>& indices, uint32_t baseIndex);

    /**
     * @brief Generate the bottom hexagonal face.
     *
     * Creates a center vertex with 6 corner vertices forming 6 triangles.
     * Normals point down (0, -1, 0). UVs centered in unit square.
     *
     * @param radius Hex outer radius
     * @param verts Output vertex array
     * @param indices Output index array
     * @param baseIndex Starting index for vertices
     */
    static void generateBottomFace(float radius, std::vector<HexPrismVertex>& verts,
                                   std::vector<uint32_t>& indices, uint32_t baseIndex);

    /**
     * @brief Generate the 6 rectangular side faces.
     *
     * Each side face is a quad (2 triangles) connecting top and bottom hex edges.
     * Normals face outward perpendicular to each face.
     * UVs tile horizontally around the prism.
     *
     * @param radius Hex outer radius
     * @param verts Output vertex array
     * @param indices Output index array
     * @param baseIndex Starting index for vertices
     */
    static void generateSideFaces(float radius, std::vector<HexPrismVertex>& verts,
                                  std::vector<uint32_t>& indices, uint32_t baseIndex);

    /**
     * @brief Get hex corner position in XZ plane.
     *
     * Flat-top hex with corner 0 at angle 0 (pointing right).
     * Corners numbered counter-clockwise.
     *
     * @param radius Hex outer radius
     * @param cornerIndex Corner number (0-5)
     * @param y Y coordinate for the vertex
     * @return 3D position of corner
     */
    static glm::vec3 getCornerPosition(float radius, int cornerIndex, float y);

    /**
     * @brief Calculate UV coordinates for hex face.
     *
     * Maps hex coordinates to unit square UV space [0, 1].
     *
     * @param localPos 2D position relative to hex center
     * @param radius Hex outer radius
     * @return UV coordinates
     */
    static glm::vec2 calculateUV(const glm::vec2& localPos, float radius);
};

}  // namespace vde
