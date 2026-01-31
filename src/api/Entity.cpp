/**
 * @file Entity.cpp
 * @brief Implementation of Entity base class and MeshEntity
 */

#include <vde/api/Entity.h>#include <vde/api/Scene.h>
#include <vde/api/Game.h>
#include <vde/api/Mesh.h>
#include <vde/VulkanContext.h>#include <glm/gtc/matrix_transform.hpp>

namespace vde {

// Static member initialization
EntityId Entity::s_nextId = 1;

// ============================================================================
// Entity Implementation
// ============================================================================

Entity::Entity()
    : m_id(s_nextId++)
    , m_name("")
    , m_transform()
    , m_visible(true)
    , m_scene(nullptr)
{
}

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
    model = glm::rotate(model, glm::radians(m_transform.rotation.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_transform.rotation.roll), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Scale
    model = glm::scale(model, m_transform.scale.toVec3());
    
    return model;
}

// ============================================================================
// MeshEntity Implementation
// ============================================================================

MeshEntity::MeshEntity()
    : Entity()
    , m_mesh(nullptr)
    , m_texture(nullptr)
    , m_meshId(INVALID_RESOURCE_ID)
    , m_textureId(INVALID_RESOURCE_ID)
    , m_color(Color::white())
{
}

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
    
    // Upload mesh to GPU if needed
    if (!mesh->isOnGPU()) {
        mesh->uploadToGPU(context);
    }
    
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
    
    // Set viewport and scissor (dynamic state)
    VkExtent2D extent = context->getSwapChainExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    // Bind UBO descriptor set (set 0)
    VkDescriptorSet uboDescriptorSet = context->getCurrentUBODescriptorSet();
    if (uboDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               pipelineLayout, 0, 1, &uboDescriptorSet, 0, nullptr);
    }
    
    // Push model matrix as push constant
    glm::mat4 modelMatrix = getModelMatrix();
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &modelMatrix);
    
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
// SpriteEntity Implementation (stub for Phase 3)
// ============================================================================

SpriteEntity::SpriteEntity()
    : Entity()
    , m_textureId(INVALID_RESOURCE_ID)
    , m_color(Color::white())
    , m_uvX(0.0f)
    , m_uvY(0.0f)
    , m_uvWidth(1.0f)
    , m_uvHeight(1.0f)
    , m_anchorX(0.5f)
    , m_anchorY(0.5f)
{
}

SpriteEntity::SpriteEntity(ResourceId textureId)
    : Entity()
    , m_textureId(textureId)
    , m_color(Color::white())
    , m_uvX(0.0f)
    , m_uvY(0.0f)
    , m_uvWidth(1.0f)
    , m_uvHeight(1.0f)
    , m_anchorX(0.5f)
    , m_anchorY(0.5f)
{
}

void SpriteEntity::setUVRect(float u, float v, float width, float height) {
    m_uvX = u;
    m_uvY = v;
    m_uvWidth = width;
    m_uvHeight = height;
}

void SpriteEntity::render() {
    // Phase 3 stub: No actual rendering yet
}

} // namespace vde
