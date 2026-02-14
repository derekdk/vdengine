/**
 * @file Entity.cpp
 * @brief Implementation of Entity base class and MeshEntity
 */

#include <vde/DescriptorManager.h>
#include <vde/Texture.h>
#include <vde/Types.h>
#include <vde/VulkanContext.h>
#include <vde/api/Entity.h>
#include <vde/api/Game.h>
#include <vde/api/Material.h>
#include <vde/api/Mesh.h>
#include <vde/api/Scene.h>

#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>

namespace vde {

// Static member initialization
EntityId Entity::s_nextId = 1;

// Static sprite quad mesh (shared by all SpriteEntity instances)
static std::shared_ptr<Mesh> s_spriteQuad = nullptr;

// Descriptor set cache for textures per frame (maps {frame, texture} to descriptor set)
// We need per-frame caching because the UBO buffer changes each frame
// Note: Descriptor sets are allocated from Game's descriptor pool and cleaned up
// when the pool is destroyed. This map just tracks which sets exist.
static constexpr uint32_t MAX_FRAMES = 2;
static std::unordered_map<Texture*, VkDescriptorSet> s_textureDescriptorSets[MAX_FRAMES];
static std::unordered_map<Texture*, VkDescriptorSet> s_meshTextureDescriptorSets[MAX_FRAMES];

/**
 * @brief Clear sprite descriptor set cache (called on Game shutdown).
 *
 * This is a free function (not static) so it can be called from Game.cpp.
 * Declared as extern in Game.cpp.
 */
void clearSpriteDescriptorCache() {
    for (int i = 0; i < MAX_FRAMES; ++i) {
        s_textureDescriptorSets[i].clear();
        s_meshTextureDescriptorSets[i].clear();
    }
    // Clean up the static sprite quad mesh to ensure its Vulkan buffers
    // are destroyed before the device is destroyed
    s_spriteQuad.reset();
}

// Helper to get or create the sprite quad mesh
static std::shared_ptr<Mesh> getSpriteQuadMesh() {
    if (!s_spriteQuad) {
        s_spriteQuad = std::make_shared<Mesh>();

        // Create a unit quad centered at origin
        // Vertices: position, color (unused), texCoord
        std::vector<Vertex> vertices = {
            // Position             Color (unused)      TexCoord
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},  // bottom-left
            {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},   // bottom-right
            {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},    // top-right
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},   // top-left
        };

        std::vector<uint32_t> indices = {
            0, 1, 2,  // first triangle
            2, 3, 0   // second triangle
        };

        s_spriteQuad->setData(vertices, indices);
    }
    return s_spriteQuad;
}

// ============================================================================
// Entity Implementation
// ============================================================================

Entity::Entity() : m_id(s_nextId++), m_name(""), m_transform(), m_visible(true), m_scene(nullptr) {}

void Entity::setPosition(float x, float y, float z) {
    m_transform.position = Position(x, y, z);
}

void Entity::setPosition(const Position& pos) {
    m_transform.position = pos;
}

void Entity::setPosition(const glm::vec3& pos) {
    m_transform.position = Position(pos.x, pos.y, pos.z);
}

void Entity::setRotation(float pitch, float yaw, float roll) {
    m_transform.rotation = Rotation(pitch, yaw, roll);
}

void Entity::setRotation(const Rotation& rot) {
    m_transform.rotation = rot;
}

void Entity::setScale(float uniform) {
    m_transform.scale = Scale(uniform, uniform, uniform);
}

void Entity::setScale(float x, float y, float z) {
    m_transform.scale = Scale(x, y, z);
}

void Entity::setScale(const Scale& scl) {
    m_transform.scale = scl;
}

glm::mat4 Entity::getModelMatrix() const {
    // Build TRS matrix: Translation * Rotation * Scale
    glm::mat4 model = glm::mat4(1.0f);

    // Translation
    model = glm::translate(model, m_transform.position.toVec3());

    // Rotation (apply in order: yaw (Y), pitch (X), roll (Z))
    model = glm::rotate(model, glm::radians(m_transform.rotation.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model =
        glm::rotate(model, glm::radians(m_transform.rotation.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    model =
        glm::rotate(model, glm::radians(m_transform.rotation.roll), glm::vec3(0.0f, 0.0f, 1.0f));

    // Scale
    model = glm::scale(model, m_transform.scale.toVec3());

    return model;
}

// ============================================================================
// MeshEntity Implementation
// ============================================================================

MeshEntity::MeshEntity()
    : Entity(), m_mesh(nullptr), m_texture(nullptr), m_material(nullptr),
      m_meshId(INVALID_RESOURCE_ID), m_textureId(INVALID_RESOURCE_ID), m_color(Color::white()) {}

void MeshEntity::render() {
    // Get the mesh (either direct or via resource ID)
    std::shared_ptr<Mesh> mesh = m_mesh;
    if (!mesh && m_scene && m_meshId != INVALID_RESOURCE_ID) {
        // TODO: Get mesh from scene resources when resource management is implemented
        // For now, only direct mesh references work
        return;
    }

    if (!mesh || !m_scene) {
        return;
    }

    // Get Game and Vulkan context
    Game* game = m_scene->getGame();
    if (!game) {
        return;
    }

    VulkanContext* context = game->getVulkanContext();
    if (!context) {
        return;
    }

    // Resolve texture (or fallback to default white)
    std::shared_ptr<Texture> texture = m_texture;
    if (!texture && m_scene && m_textureId != INVALID_RESOURCE_ID) {
        // TODO: Get texture from scene resources when resource management is implemented
    }

    if (texture && !texture->isOnGPU()) {
        texture->uploadToGPU(context);
    }

    Texture* texturePtr = texture ? texture.get() : game->getDefaultWhiteTexture();
    if (!texturePtr || !texturePtr->isValid()) {
        return;
    }

    // Upload mesh to GPU if needed
    if (!mesh->isOnGPU()) {
        mesh->uploadToGPU(context);
    }

    // Update lighting UBO with scene lighting data
    game->updateLightingUBO(m_scene);

    // Get current command buffer
    VkCommandBuffer cmd = context->getCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE) {
        return;
    }

    // Get pipeline
    VkPipeline pipeline = game->getMeshPipeline();
    VkPipelineLayout pipelineLayout = game->getMeshPipelineLayout();
    if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Set viewport and scissor (dynamic state) — uses override if set
    VkViewport viewport = context->getEffectiveViewport();
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = context->getEffectiveScissor();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind UBO descriptor set (set 0)
    VkDescriptorSet uboDescriptorSet = context->getCurrentUBODescriptorSet();
    if (uboDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                                &uboDescriptorSet, 0, nullptr);
    }

    // Bind lighting descriptor set (set 1)
    VkDescriptorSet lightingDescriptorSet = game->getCurrentLightingDescriptorSet();
    if (lightingDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1,
                                &lightingDescriptorSet, 0, nullptr);
    }

    // Bind texture descriptor set (set 2)
    uint32_t currentFrame = context->getCurrentFrame();
    if (currentFrame >= MAX_FRAMES) {
        currentFrame = 0;
    }

    VkDescriptorSet meshTextureDescSet = VK_NULL_HANDLE;
    auto& frameCache = s_meshTextureDescriptorSets[currentFrame];
    auto textureSetIt = frameCache.find(texturePtr);
    if (textureSetIt != frameCache.end()) {
        meshTextureDescSet = textureSetIt->second;
    } else {
        meshTextureDescSet = game->allocateMeshTextureDescriptorSet();
        if (meshTextureDescSet != VK_NULL_HANDLE) {
            game->updateMeshTextureDescriptor(meshTextureDescSet, texturePtr->getImageView(),
                                              texturePtr->getSampler());
            frameCache[texturePtr] = meshTextureDescSet;
        }
    }

    if (meshTextureDescSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1,
                                &meshTextureDescSet, 0, nullptr);
    }

    // Prepare push constants: model matrix + material properties
    struct MeshPushConstants {
        glm::mat4 model;
        MaterialPushConstants material;
    } pushData;

    pushData.model = getModelMatrix();

    // Get material properties (use defaults if no material)
    if (m_material) {
        Material::GPUData gpuData = m_material->getGPUData();
        pushData.material.albedo = gpuData.albedo;
        pushData.material.emission = gpuData.emission;
        pushData.material.roughness = gpuData.roughness;
        pushData.material.metallic = gpuData.metallic;
        pushData.material.normalStrength = gpuData.normalStrength;
        pushData.material.padding = 0.0f;
    } else {
        // Default material: entity color, medium roughness, non-metallic
        pushData.material.albedo = glm::vec4(m_color.r, m_color.g, m_color.b, 1.0f);
        pushData.material.emission = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        pushData.material.roughness = 0.5f;
        pushData.material.metallic = 0.0f;
        pushData.material.normalStrength = 1.0f;
        pushData.material.padding = 0.0f;
    }

    // Push model matrix and material as push constants
    vkCmdPushConstants(cmd, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MeshPushConstants), &pushData);

    // Bind mesh buffers
    mesh->bind(cmd);

    // Draw
    if (mesh->getIndexCount() > 0) {
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->getIndexCount()), 1, 0, 0, 0);
    } else if (mesh->getVertexCount() > 0) {
        vkCmdDraw(cmd, static_cast<uint32_t>(mesh->getVertexCount()), 1, 0, 0);
    }
}

// ============================================================================
// SpriteEntity Implementation (Phase 3)
// ============================================================================

SpriteEntity::SpriteEntity()
    : Entity(), m_texture(nullptr), m_textureId(INVALID_RESOURCE_ID), m_color(Color::white()),
      m_uvX(0.0f), m_uvY(0.0f), m_uvWidth(1.0f), m_uvHeight(1.0f), m_anchorX(0.5f),
      m_anchorY(0.5f) {}

SpriteEntity::SpriteEntity(ResourceId textureId)
    : Entity(), m_texture(nullptr), m_textureId(textureId), m_color(Color::white()), m_uvX(0.0f),
      m_uvY(0.0f), m_uvWidth(1.0f), m_uvHeight(1.0f), m_anchorX(0.5f), m_anchorY(0.5f) {}

void SpriteEntity::setUVRect(float u, float v, float width, float height) {
    m_uvX = u;
    m_uvY = v;
    m_uvWidth = width;
    m_uvHeight = height;
}

void SpriteEntity::render() {
    // Get the texture (either direct or via resource ID)
    std::shared_ptr<Texture> texture = m_texture;
    if (!texture && m_scene && m_textureId != INVALID_RESOURCE_ID) {
        // TODO: Get texture from scene resources when resource management is implemented
        return;
    }

    // Get Game and Vulkan context
    Game* game = m_scene ? m_scene->getGame() : nullptr;
    if (!game) {
        return;
    }

    VulkanContext* context = game->getVulkanContext();
    if (!context) {
        return;
    }

    // Use default white texture if no texture is assigned (for solid color sprites)
    Texture* texturePtr = texture ? texture.get() : game->getDefaultWhiteTexture();
    if (!texturePtr || !texturePtr->isValid()) {
        return;
    }

    // Get or create sprite quad mesh
    auto quadMesh = getSpriteQuadMesh();
    if (!quadMesh) {
        return;
    }

    // Upload mesh to GPU if needed
    if (!quadMesh->isOnGPU()) {
        quadMesh->uploadToGPU(context);
    }

    // Get current command buffer
    VkCommandBuffer cmd = context->getCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE) {
        return;
    }

    // Get pipeline
    VkPipeline pipeline = game->getSpritePipeline();
    VkPipelineLayout pipelineLayout = game->getSpritePipelineLayout();
    if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    // Get current frame index for per-frame descriptor set caching
    uint32_t currentFrame = context->getCurrentFrame();
    if (currentFrame >= MAX_FRAMES) {
        currentFrame = 0;
    }

    // Get or create combined sprite descriptor set for this texture (per-frame)
    // The descriptor set contains both UBO (binding 0) and texture (binding 1)
    VkDescriptorSet spriteDescSet = VK_NULL_HANDLE;
    auto& frameCache = s_textureDescriptorSets[currentFrame];
    auto it = frameCache.find(texturePtr);
    if (it != frameCache.end()) {
        spriteDescSet = it->second;
    } else {
        // Allocate new combined sprite descriptor set
        spriteDescSet = game->allocateSpriteDescriptorSet();
        if (spriteDescSet == VK_NULL_HANDLE) {
            return;
        }

        // Update descriptor with UBO and texture info
        VkBuffer uboBuffer = context->getCurrentUniformBuffer();
        game->updateSpriteDescriptor(spriteDescSet, uboBuffer, 192,  // sizeof(UniformBufferObject)
                                     texturePtr->getImageView(), texturePtr->getSampler());

        // Cache it for this frame
        frameCache[texturePtr] = spriteDescSet;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Set viewport and scissor (dynamic state) — uses override if set
    VkViewport viewport = context->getEffectiveViewport();
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = context->getEffectiveScissor();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind combined sprite descriptor set (contains both UBO and texture)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                            &spriteDescSet, 0, nullptr);

    // Push constants: model matrix (64 bytes) + tint (16 bytes) + uvRect (16 bytes)
    struct SpritePushConstants {
        glm::mat4 model;
        glm::vec4 tint;
        glm::vec4 uvRect;
    } pushData;

    // Apply anchor offset to model matrix
    glm::mat4 anchorOffset =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.5f - m_anchorX, 0.5f - m_anchorY, 0.0f));
    pushData.model = getModelMatrix() * anchorOffset;
    pushData.tint = glm::vec4(m_color.r, m_color.g, m_color.b, m_color.a);
    pushData.uvRect = glm::vec4(m_uvX, m_uvY, m_uvWidth, m_uvHeight);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(SpritePushConstants), &pushData);

    // Bind quad mesh buffers
    quadMesh->bind(cmd);

    // Draw
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(quadMesh->getIndexCount()), 1, 0, 0, 0);
}

}  // namespace vde
