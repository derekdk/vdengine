#pragma once

/**
 * @file Types.h
 * @brief Common types for Vulkan Display Engine
 * 
 * Contains fundamental data structures used throughout the engine
 * including vertex formats, colors, and other common types.
 */

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

namespace vde {

/**
 * @brief Represents a vertex with position, color, and texture coordinates.
 * 
 * This structure is used for rendering geometry with Vulkan. It includes
 * static methods to describe the vertex layout to the Vulkan pipeline.
 */
struct Vertex {
    glm::vec3 position;   ///< 3D position of the vertex
    glm::vec3 color;      ///< RGB color of the vertex
    glm::vec2 texCoord;   ///< UV texture coordinates
    
    /**
     * @brief Gets the binding description for the vertex buffer.
     * @return VkVertexInputBindingDescription describing how to read vertex data.
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    
    /**
     * @brief Gets the attribute descriptions for the vertex structure.
     * @return Array of VkVertexInputAttributeDescription for position, color, and texCoord.
     */
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        
        // Position attribute (location = 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);
        
        // Color attribute (location = 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        // Texture coordinate attribute (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
        
        return attributeDescriptions;
    }
    
    /**
     * @brief Equality operator for comparing vertices.
     * @param other The vertex to compare with.
     * @return true if vertices are equal.
     */
    bool operator==(const Vertex& other) const {
        return position == other.position && 
               color == other.color && 
               texCoord == other.texCoord;
    }
};

/**
 * @brief Uniform buffer object for shader data.
 * 
 * Contains the Model-View-Projection matrices used for rendering.
 * Alignment must match GLSL std140 layout rules:
 * - vec4/mat4 must be 16-byte aligned
 * - Structures are padded to multiple of 16 bytes
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;  ///< Model matrix (object -> world)
    alignas(16) glm::mat4 view;   ///< View matrix (world -> camera)
    alignas(16) glm::mat4 proj;   ///< Projection matrix (camera -> clip)
};

// Static assert to verify UBO structure size.
// UBO should be exactly 3 mat4s = 3 * 64 = 192 bytes.
static_assert(sizeof(UniformBufferObject) == 192, 
    "UniformBufferObject size must be 192 bytes (3 aligned mat4)");

} // namespace vde
