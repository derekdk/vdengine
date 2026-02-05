#pragma once

/**
 * @file Mesh.h
 * @brief Mesh and 3D model support for VDE games
 *
 * Provides classes for loading and managing 3D geometry
 * including static meshes and animated models.
 */

#include <vde/Types.h>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "GameTypes.h"
#include "Resource.h"

namespace vde {

// Forward declarations
class VulkanContext;

/**
 * @brief Represents a 3D mesh resource.
 *
 * Meshes contain geometry data (vertices and indices) that can
 * be rendered. They can be loaded from files or created programmatically.
 */
class Mesh : public Resource {
  public:
    Mesh() = default;
    virtual ~Mesh();

    /**
     * @brief Load mesh from a file.
     * @param path Path to the mesh file (.obj, .gltf, etc.)
     * @return true if loading succeeded
     */
    virtual bool loadFromFile(const std::string& path);

    /**
     * @brief Create mesh from vertex and index data.
     * @param vertices Vertex data
     * @param indices Index data
     */
    virtual void setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    /**
     * @brief Get the vertex data.
     */
    const std::vector<Vertex>& getVertices() const { return m_vertices; }

    /**
     * @brief Get the index data.
     */
    const std::vector<uint32_t>& getIndices() const { return m_indices; }

    /**
     * @brief Get the number of vertices.
     */
    size_t getVertexCount() const { return m_vertices.size(); }

    /**
     * @brief Get the number of indices.
     */
    size_t getIndexCount() const { return m_indices.size(); }

    /**
     * @brief Get the axis-aligned bounding box minimum point.
     */
    const glm::vec3& getBoundsMin() const { return m_boundsMin; }

    /**
     * @brief Get the axis-aligned bounding box maximum point.
     */
    const glm::vec3& getBoundsMax() const { return m_boundsMax; }

    const char* getTypeName() const override { return "Mesh"; }

    // Factory methods for primitive shapes

    /**
     * @brief Create a cube mesh.
     * @param size Size of the cube
     * @return Shared pointer to the mesh
     */
    static ResourcePtr<Mesh> createCube(float size = 1.0f);

    /**
     * @brief Create a sphere mesh.
     * @param radius Radius of the sphere
     * @param segments Number of horizontal segments
     * @param rings Number of vertical rings
     * @return Shared pointer to the mesh
     */
    static ResourcePtr<Mesh> createSphere(float radius = 0.5f, int segments = 32, int rings = 16);

    /**
     * @brief Create a plane mesh.
     * @param width Width of the plane
     * @param height Height of the plane
     * @param subdivisionsX Number of X subdivisions
     * @param subdivisionsY Number of Y subdivisions
     * @return Shared pointer to the mesh
     */
    static ResourcePtr<Mesh> createPlane(float width = 1.0f, float height = 1.0f,
                                         int subdivisionsX = 1, int subdivisionsY = 1);

    /**
     * @brief Create a cylinder mesh.
     * @param radius Radius of the cylinder
     * @param height Height of the cylinder
     * @param segments Number of segments around the circumference
     * @return Shared pointer to the mesh
     */
    static ResourcePtr<Mesh> createCylinder(float radius = 0.5f, float height = 1.0f,
                                            int segments = 32);

    // GPU buffer management

    /**
     * @brief Upload mesh data to GPU.
     * @param context Vulkan context for buffer creation
     */
    void uploadToGPU(VulkanContext* context);

    /**
     * @brief Free GPU buffers.
     * @param device Vulkan device for cleanup
     */
    void freeGPUBuffers(VkDevice device);

    /**
     * @brief Check if mesh has been uploaded to GPU.
     */
    bool isOnGPU() const { return m_vertexBuffer != VK_NULL_HANDLE; }

    /**
     * @brief Bind vertex and index buffers for rendering.
     * @param commandBuffer Command buffer to bind to
     */
    void bind(VkCommandBuffer commandBuffer) const;

  protected:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};

    // GPU buffers (VK_NULL_HANDLE if not uploaded)
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

    // Device used for GPU buffer creation (needed for cleanup in destructor)
    VkDevice m_device = VK_NULL_HANDLE;

    void calculateBounds();
};

}  // namespace vde
