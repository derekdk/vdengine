#pragma once

/**
 * @file PhysicsEntity.h
 * @brief Physics-driven entity classes for VDE
 *
 * Provides PhysicsEntity (mixin), PhysicsMeshEntity, and PhysicsSpriteEntity
 * that bind visual entities to physics bodies with automatic transform
 * synchronisation and interpolation.
 */

#include <glm/glm.hpp>

#include "Entity.h"
#include "PhysicsTypes.h"

namespace vde {

// Forward declarations
class PhysicsScene;
class Scene;

/**
 * @brief Mixin class that adds physics-body binding to an entity.
 *
 * PhysicsEntity does **not** inherit from Entity — it is designed to be
 * combined with a visual Entity subclass via multiple inheritance
 * (e.g. PhysicsSpriteEntity = SpriteEntity + PhysicsEntity) without
 * creating a diamond-inheritance problem.
 *
 * The concrete derived class must call `initPhysicsOwner(this)` so
 * that sync methods can manipulate the Entity transform.
 *
 * ## Lifecycle
 *   1. Entity is added to a Scene via `scene->addEntity<PhysicsSpriteEntity>()`.
 *   2. Derived `onAttach()` calls `attachPhysics(scene)`.
 *   3. User calls `createPhysicsBody(def)` to create the underlying body.
 *   4. Each frame the scheduler's PostPhysics task calls `syncFromPhysics(alpha)`
 *      for entities with `getAutoSync() == true`.
 *   5. Derived `onDetach()` calls `detachPhysics()`.
 *
 * @example
 * @code
 * auto entity = scene->addEntity<vde::PhysicsSpriteEntity>();
 * entity->setColor(vde::Color::red());
 * entity->setScale(1.0f);
 *
 * vde::PhysicsBodyDef def;
 * def.type  = vde::PhysicsBodyType::Dynamic;
 * def.shape = vde::PhysicsShape::Box;
 * def.position = {0.0f, 5.0f};
 * def.extents  = {0.5f, 0.5f};
 * def.mass     = 1.0f;
 * entity->createPhysicsBody(def);
 * @endcode
 */
class PhysicsEntity {
  public:
    PhysicsEntity();
    virtual ~PhysicsEntity();

    // -----------------------------------------------------------------
    // Body management
    // -----------------------------------------------------------------

    /**
     * @brief Create a physics body in the owning scene's PhysicsScene.
     *
     * The scene must have physics enabled (`scene->enablePhysics()`).
     *
     * @param def Body definition
     * @return The created body's ID
     * @throws std::runtime_error if the scene has no PhysicsScene
     */
    PhysicsBodyId createPhysicsBody(const PhysicsBodyDef& def);

    /**
     * @brief Get the physics body ID (INVALID_PHYSICS_BODY_ID if none).
     */
    PhysicsBodyId getPhysicsBodyId() const { return m_physicsBodyId; }

    /**
     * @brief Get the current physics body state.
     * @throws std::runtime_error if no valid body exists
     */
    PhysicsBodyState getPhysicsState() const;

    // -----------------------------------------------------------------
    // Force / impulse helpers  (delegate to PhysicsScene)
    // -----------------------------------------------------------------

    void applyForce(const glm::vec2& force);
    void applyImpulse(const glm::vec2& impulse);
    void setLinearVelocity(const glm::vec2& velocity);

    // -----------------------------------------------------------------
    // Synchronisation
    // -----------------------------------------------------------------

    /**
     * @brief Copy the interpolated physics position to the entity transform.
     * @param interpolationAlpha Blend factor in [0, 1)
     */
    void syncFromPhysics(float interpolationAlpha);

    /**
     * @brief Copy the entity's current position into the physics body.
     */
    void syncToPhysics();

    /**
     * @brief Enable or disable automatic PostPhysics sync (default: true).
     */
    void setAutoSync(bool enabled) { m_autoSync = enabled; }

    /**
     * @brief Check whether automatic sync is enabled.
     */
    bool getAutoSync() const { return m_autoSync; }

    // -----------------------------------------------------------------
    // Internal — called by derived classes
    // -----------------------------------------------------------------

    /**
     * @brief Set the Entity pointer that owns this physics mixin.
     *
     * Must be called in the constructor or onAttach of the concrete class.
     */
    void initPhysicsOwner(Entity* owner) { m_owner = owner; }

    /**
     * @brief Attach physics state from the given scene.
     */
    void attachPhysics(Scene* scene);

    /**
     * @brief Detach physics state and destroy the body.
     */
    void detachPhysics();

  protected:
    Entity* m_owner = nullptr;
    PhysicsBodyId m_physicsBodyId = INVALID_PHYSICS_BODY_ID;
    PhysicsScene* m_physicsScene = nullptr;
    bool m_autoSync = true;

    // Previous position/rotation for interpolation
    glm::vec2 m_prevPosition{0.0f, 0.0f};
    float m_prevRotation = 0.0f;
};

/**
 * @brief Sprite entity driven by physics.
 *
 * Combines SpriteEntity visuals with PhysicsEntity simulation
 * so a 2D sprite is automatically positioned by the physics engine.
 */
class PhysicsSpriteEntity : public SpriteEntity, public PhysicsEntity {
  public:
    using Ref = std::shared_ptr<PhysicsSpriteEntity>;

    PhysicsSpriteEntity();
    ~PhysicsSpriteEntity() override = default;

    void onAttach(Scene* scene) override;
    void onDetach() override;
    void update(float deltaTime) override;
    void render() override;
};

/**
 * @brief Mesh entity driven by physics.
 *
 * Combines MeshEntity visuals with PhysicsEntity simulation
 * so a 3D mesh is automatically positioned by the physics engine.
 */
class PhysicsMeshEntity : public MeshEntity, public PhysicsEntity {
  public:
    using Ref = std::shared_ptr<PhysicsMeshEntity>;

    PhysicsMeshEntity();
    ~PhysicsMeshEntity() override = default;

    void onAttach(Scene* scene) override;
    void onDetach() override;
    void update(float deltaTime) override;
    void render() override;
};

}  // namespace vde
