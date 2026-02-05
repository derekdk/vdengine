#pragma once

/**
 * @file Entity.h
 * @brief Entity system for VDE games
 *
 * Provides base entity classes for game objects including
 * mesh entities, sprite entities, and other renderable objects.
 */

#include <memory>
#include <string>

#include "GameTypes.h"
#include "Resource.h"

namespace vde {

// Forward declarations
class Scene;
class Mesh;
class Material;
class Texture;

/**
 * @brief Base class for all game entities.
 *
 * An entity represents an object in the game world with a transform
 * (position, rotation, scale) and optional visual representation.
 */
class Entity {
  public:
    using Ref = std::shared_ptr<Entity>;

    Entity();
    virtual ~Entity() = default;

    /**
     * @brief Get the unique ID of this entity.
     */
    EntityId getId() const { return m_id; }

    /**
     * @brief Get the entity's name.
     */
    const std::string& getName() const { return m_name; }

    /**
     * @brief Set the entity's name.
     */
    void setName(const std::string& name) { m_name = name; }

    // Transform manipulation

    /**
     * @brief Set the entity's position.
     */
    void setPosition(float x, float y, float z);
    void setPosition(const Position& pos);
    void setPosition(const glm::vec3& pos);

    /**
     * @brief Get the entity's position.
     */
    const Position& getPosition() const { return m_transform.position; }

    /**
     * @brief Set the entity's rotation (Euler angles in degrees).
     */
    void setRotation(float pitch, float yaw, float roll);
    void setRotation(const Rotation& rot);

    /**
     * @brief Get the entity's rotation.
     */
    const Rotation& getRotation() const { return m_transform.rotation; }

    /**
     * @brief Set the entity's scale.
     */
    void setScale(float uniform);
    void setScale(float x, float y, float z);
    void setScale(const Scale& scl);

    /**
     * @brief Get the entity's scale.
     */
    const Scale& getScale() const { return m_transform.scale; }

    /**
     * @brief Get the full transform.
     */
    const Transform& getTransform() const { return m_transform; }

    /**
     * @brief Set the full transform.
     */
    void setTransform(const Transform& transform) { m_transform = transform; }

    /**
     * @brief Get the model matrix for rendering.
     */
    glm::mat4 getModelMatrix() const;

    // Visibility

    /**
     * @brief Set whether the entity is visible.
     */
    void setVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Check if the entity is visible.
     */
    bool isVisible() const { return m_visible; }

    // Lifecycle methods (override in subclasses)

    /**
     * @brief Called when the entity is added to a scene.
     */
    virtual void onAttach(Scene* scene) { m_scene = scene; }

    /**
     * @brief Called when the entity is removed from a scene.
     */
    virtual void onDetach() { m_scene = nullptr; }

    /**
     * @brief Called every frame to update entity state.
     * @param deltaTime Time since last update in seconds
     */
    virtual void update([[maybe_unused]] float deltaTime) {}

    /**
     * @brief Called every frame to render the entity.
     */
    virtual void render() {}

  protected:
    EntityId m_id;
    std::string m_name;
    Transform m_transform;
    bool m_visible = true;
    Scene* m_scene = nullptr;

  private:
    static EntityId s_nextId;
};

/**
 * @brief Entity that renders a 3D mesh.
 *
 * MeshEntity can hold either a direct mesh reference or a resource ID.
 * For simple cases (primitives), use setMesh(shared_ptr<Mesh>).
 * For resource-managed meshes, use setMeshId(ResourceId).
 */
class MeshEntity : public Entity {
  public:
    using Ref = std::shared_ptr<MeshEntity>;

    MeshEntity();
    virtual ~MeshEntity() = default;

    /**
     * @brief Set the mesh directly (takes shared ownership).
     * @param mesh The mesh to render
     */
    void setMesh(std::shared_ptr<Mesh> mesh) { m_mesh = std::move(mesh); }

    /**
     * @brief Get the mesh.
     */
    std::shared_ptr<Mesh> getMesh() const { return m_mesh; }

    /**
     * @brief Set the mesh by resource ID (loaded via Scene).
     * @param meshId Resource ID of the mesh
     */
    void setMeshId(ResourceId meshId) { m_meshId = meshId; }

    /**
     * @brief Get the mesh resource ID.
     */
    ResourceId getMeshId() const { return m_meshId; }

    /**
     * @brief Set the texture directly (takes shared ownership).
     * @param texture The texture to use
     */
    void setTexture(std::shared_ptr<Texture> texture) { m_texture = std::move(texture); }

    /**
     * @brief Get the texture.
     */
    std::shared_ptr<Texture> getTexture() const { return m_texture; }

    /**
     * @brief Set the texture by resource ID.
     * @param textureId Resource ID of the texture
     */
    void setTextureId(ResourceId textureId) { m_textureId = textureId; }

    /**
     * @brief Get the texture resource ID.
     */
    ResourceId getTextureId() const { return m_textureId; }

    /**
     * @brief Set the base color/tint of the mesh.
     */
    void setColor(const Color& color) { m_color = color; }

    /**
     * @brief Get the base color/tint.
     */
    const Color& getColor() const { return m_color; }

    /**
     * @brief Set the material (takes shared ownership).
     * @param material The material to use for rendering
     */
    void setMaterial(std::shared_ptr<Material> material) { m_material = std::move(material); }

    /**
     * @brief Get the material.
     */
    std::shared_ptr<Material> getMaterial() const { return m_material; }

    /**
     * @brief Check if entity has a material.
     */
    bool hasMaterial() const { return m_material != nullptr; }

    void render() override;

  protected:
    // Direct references (preferred for simplicity)
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<Material> m_material;

    // Resource IDs (for Scene-managed resources)
    ResourceId m_meshId = INVALID_RESOURCE_ID;
    ResourceId m_textureId = INVALID_RESOURCE_ID;

    Color m_color = Color::white();
};

/**
 * @brief Entity that renders a 2D sprite.
 */
class SpriteEntity : public Entity {
  public:
    using Ref = std::shared_ptr<SpriteEntity>;

    SpriteEntity();
    SpriteEntity(ResourceId textureId);
    virtual ~SpriteEntity() = default;

    /**
     * @brief Set the sprite texture directly (takes shared ownership).
     * @param texture The texture to render
     */
    void setTexture(std::shared_ptr<Texture> texture) { m_texture = std::move(texture); }

    /**
     * @brief Get the texture.
     */
    std::shared_ptr<Texture> getTexture() const { return m_texture; }

    /**
     * @brief Set the sprite texture by resource ID.
     * @param textureId Resource ID of the texture
     */
    void setTextureId(ResourceId textureId) { m_textureId = textureId; }

    /**
     * @brief Get the texture resource ID.
     */
    ResourceId getTextureId() const { return m_textureId; }

    /**
     * @brief Set the sprite color/tint.
     */
    void setColor(const Color& color) { m_color = color; }

    /**
     * @brief Get the sprite color/tint.
     */
    const Color& getColor() const { return m_color; }

    /**
     * @brief Set the UV rectangle for sprite sheets.
     * @param u U coordinate (0-1)
     * @param v V coordinate (0-1)
     * @param width Width in UV space (0-1)
     * @param height Height in UV space (0-1)
     */
    void setUVRect(float u, float v, float width, float height);

    /**
     * @brief Set the sprite's anchor point (0-1, where 0.5,0.5 is center).
     */
    void setAnchor(float x, float y) {
        m_anchorX = x;
        m_anchorY = y;
    }

    /**
     * @brief Get the sprite anchor point X.
     */
    float getAnchorX() const { return m_anchorX; }

    /**
     * @brief Get the sprite anchor point Y.
     */
    float getAnchorY() const { return m_anchorY; }

    void render() override;

  protected:
    // Direct texture reference (preferred for simplicity)
    std::shared_ptr<Texture> m_texture;

    // Resource ID (for Scene-managed resources)
    ResourceId m_textureId = INVALID_RESOURCE_ID;

    Color m_color = Color::white();
    float m_uvX = 0.0f, m_uvY = 0.0f;
    float m_uvWidth = 1.0f, m_uvHeight = 1.0f;
    float m_anchorX = 0.5f, m_anchorY = 0.5f;
};

}  // namespace vde
