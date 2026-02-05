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

/**
 * @brief Maximum number of lights supported in the lighting UBO.
 */
constexpr uint32_t MAX_LIGHTS = 8;

/**
 * @brief GPU representation of a single light source.
 * 
 * Packed for GLSL std140 layout. Must match shader light struct exactly.
 * Size: 64 bytes (4 x vec4)
 */
struct GPULight {
    alignas(16) glm::vec4 positionAndType;   ///< xyz = position/direction, w = type (0=dir, 1=point, 2=spot)
    alignas(16) glm::vec4 directionAndRange; ///< xyz = direction (for spot/dir), w = range (for point/spot)
    alignas(16) glm::vec4 colorAndIntensity; ///< xyz = RGB color, w = intensity
    alignas(16) glm::vec4 spotParams;        ///< x = inner angle cos, y = outer angle cos, zw = reserved
};

static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes");

/**
 * @brief Lighting uniform buffer object for shader data.
 * 
 * Contains ambient lighting parameters and an array of light sources.
 * Follows GLSL std140 layout rules for proper GPU alignment.
 * 
 * Total size: 16 (ambient) + 16 (counts) + 64*8 (lights) = 544 bytes
 */
struct LightingUBO {
    alignas(16) glm::vec4 ambientColorAndIntensity;  ///< xyz = ambient color, w = intensity
    alignas(16) glm::ivec4 lightCounts;              ///< x = num lights, yzw = reserved
    GPULight lights[MAX_LIGHTS];                     ///< Array of light sources
};

static_assert(sizeof(LightingUBO) == 544, 
    "LightingUBO size must be 544 bytes");

/**
 * @brief Material data packed for GPU push constants.
 * 
 * This structure matches Material::GPUData and is used for push constants.
 * Size: 48 bytes (fits within typical 128-byte push constant limit)
 */
struct MaterialPushConstants {
    alignas(16) glm::vec4 albedo;        ///< RGB albedo + opacity
    alignas(16) glm::vec4 emission;      ///< RGB emission + intensity  
    float roughness;                      ///< Surface roughness (0-1)
    float metallic;                       ///< Metallic factor (0-1)
    float normalStrength;                 ///< Normal map strength
    float padding;                        ///< Padding for alignment
};

static_assert(sizeof(MaterialPushConstants) == 48,
    "MaterialPushConstants size must be 48 bytes");

} // namespace vde
