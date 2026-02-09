#pragma once

/**
 * @file PhysicsScene.h
 * @brief 2D physics simulation for VDE scenes
 *
 * Provides the PhysicsScene class that manages rigid body simulation
 * with fixed-timestep accumulation, AABB collision detection, and
 * impulse-based resolution.  Physics is opt-in per Scene via
 * Scene::enablePhysics().
 */

#include <memory>

#include "PhysicsTypes.h"

namespace vde {

/**
 * @brief 2D physics simulation with fixed-timestep accumulator.
 *
 * PhysicsScene owns a collection of physics bodies, steps them at a
 * fixed rate, performs AABB broad-phase collision detection, and
 * resolves collisions with impulse-based response.
 *
 * The class uses the pimpl idiom so the header stays lightweight.
 *
 * @example
 * @code
 * PhysicsConfig cfg;
 * cfg.gravity = {0.0f, -9.81f};
 * PhysicsScene physics(cfg);
 *
 * PhysicsBodyDef ground;
 * ground.type = PhysicsBodyType::Static;
 * ground.shape = PhysicsShape::Box;
 * ground.position = {0.0f, -1.0f};
 * ground.extents = {10.0f, 0.5f};
 * physics.createBody(ground);
 *
 * PhysicsBodyDef box;
 * box.type = PhysicsBodyType::Dynamic;
 * box.shape = PhysicsShape::Box;
 * box.position = {0.0f, 5.0f};
 * box.extents = {0.5f, 0.5f};
 * box.mass = 1.0f;
 * physics.createBody(box);
 *
 * physics.step(deltaTime);
 * @endcode
 */
class PhysicsScene {
  public:
    /**
     * @brief Construct a physics scene with the given configuration.
     * @param config Physics configuration
     */
    explicit PhysicsScene(const PhysicsConfig& config = PhysicsConfig{});

    ~PhysicsScene();

    // Non-copyable, movable
    PhysicsScene(const PhysicsScene&) = delete;
    PhysicsScene& operator=(const PhysicsScene&) = delete;
    PhysicsScene(PhysicsScene&&) noexcept;
    PhysicsScene& operator=(PhysicsScene&&) noexcept;

    // ---------------------------------------------------------------
    // Body management
    // ---------------------------------------------------------------

    /**
     * @brief Create a physics body.
     * @param def Body definition
     * @return Unique body ID
     * @throws std::runtime_error if body creation fails
     */
    PhysicsBodyId createBody(const PhysicsBodyDef& def);

    /**
     * @brief Destroy a physics body.
     * @param id Body to destroy
     */
    void destroyBody(PhysicsBodyId id);

    /**
     * @brief Get the current state of a physics body.
     * @param id Body to query
     * @return Current body state
     * @throws std::runtime_error if body does not exist
     */
    PhysicsBodyState getBodyState(PhysicsBodyId id) const;

    /**
     * @brief Get the definition that was used to create a body.
     * @param id Body to query
     * @return Body definition
     * @throws std::runtime_error if body does not exist
     */
    PhysicsBodyDef getBodyDef(PhysicsBodyId id) const;

    /**
     * @brief Check if a body exists.
     * @param id Body to check
     * @return true if body exists
     */
    bool hasBody(PhysicsBodyId id) const;

    // ---------------------------------------------------------------
    // Forces & velocity
    // ---------------------------------------------------------------

    /**
     * @brief Apply a force to a dynamic body (accumulated over step).
     * @param id Body to apply force to
     * @param force Force vector (Newtons)
     */
    void applyForce(PhysicsBodyId id, const glm::vec2& force);

    /**
     * @brief Apply an instantaneous impulse to a dynamic body.
     * @param id Body to apply impulse to
     * @param impulse Impulse vector (kg*m/s)
     */
    void applyImpulse(PhysicsBodyId id, const glm::vec2& impulse);

    /**
     * @brief Set the linear velocity of a body directly.
     * @param id Body to modify
     * @param velocity New velocity
     */
    void setLinearVelocity(PhysicsBodyId id, const glm::vec2& velocity);

    /**
     * @brief Set the position of a body directly.
     * @param id Body to modify
     * @param position New position
     */
    void setBodyPosition(PhysicsBodyId id, const glm::vec2& position);

    // ---------------------------------------------------------------
    // Simulation
    // ---------------------------------------------------------------

    /**
     * @brief Step the physics simulation.
     *
     * Uses a fixed-timestep accumulator. The actual number of sub-steps
     * is capped by PhysicsConfig::maxSubSteps to prevent spiral-of-death.
     *
     * @param deltaTime Frame delta time in seconds
     */
    void step(float deltaTime);

    /**
     * @brief Get the interpolation alpha for rendering.
     *
     * After step(), this returns the fraction of a fixed timestep
     * remaining in the accumulator. Use it to interpolate between
     * the previous and current positions for smooth rendering.
     *
     * @return Alpha in [0, 1)
     */
    float getInterpolationAlpha() const;

    /**
     * @brief Get the number of sub-steps taken in the last step() call.
     */
    int getLastStepCount() const;

    // ---------------------------------------------------------------
    // Configuration
    // ---------------------------------------------------------------

    /**
     * @brief Get the current physics configuration.
     */
    const PhysicsConfig& getConfig() const;

    /**
     * @brief Set gravity.
     * @param gravity New gravity vector
     */
    void setGravity(const glm::vec2& gravity);

    /**
     * @brief Get current gravity.
     */
    glm::vec2 getGravity() const;

    // ---------------------------------------------------------------
    // Collision callbacks
    // ---------------------------------------------------------------

    /**
     * @brief Set a callback for collision begin events.
     * @param callback Function called when two bodies start colliding
     */
    void setOnCollisionBegin(CollisionCallback callback);

    /**
     * @brief Set a callback for collision end events (reserved).
     * @param callback Function called when two bodies stop colliding
     */
    void setOnCollisionEnd(CollisionCallback callback);

    // ---------------------------------------------------------------
    // Queries
    // ---------------------------------------------------------------

    /**
     * @brief Get total number of bodies (including destroyed slots).
     */
    size_t getBodyCount() const;

    /**
     * @brief Get number of active (non-destroyed) bodies.
     */
    size_t getActiveBodyCount() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace vde
