#pragma once

/**
 * @file PhysicsTypes.h
 * @brief Physics type definitions for VDE
 *
 * Provides core physics types used by PhysicsScene and PhysicsEntity:
 * body definitions, state, shapes, collision events, and configuration.
 */

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <limits>

namespace vde {

// ============================================================================
// Identifiers
// ============================================================================

/**
 * @brief Unique identifier for a physics body.
 */
using PhysicsBodyId = uint32_t;

/**
 * @brief Sentinel value indicating an invalid physics body ID.
 */
constexpr PhysicsBodyId INVALID_PHYSICS_BODY_ID = 0;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Shape type for physics collision detection.
 */
enum class PhysicsShape : uint8_t {
    Box,     ///< Axis-aligned box (2D: rectangle, 3D: cuboid)
    Circle,  ///< 2D circle
    Sphere,  ///< 3D sphere
    Capsule  ///< Capsule shape (reserved for future use)
};

/**
 * @brief Physics body simulation type.
 */
enum class PhysicsBodyType : uint8_t {
    Static,    ///< Does not move; infinite mass; participates in collision
    Dynamic,   ///< Moves under forces and gravity; full simulation
    Kinematic  ///< Moves via user code; not affected by forces/gravity; collides with dynamic
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for a PhysicsScene.
 */
struct PhysicsConfig {
    float fixedTimestep = 1.0f / 60.0f;  ///< Fixed physics step (seconds)
    glm::vec2 gravity = {0.0f, -9.81f};  ///< Gravity vector (2D, Y-down is negative)
    int maxSubSteps = 8;                 ///< Max sub-steps per frame (spiral-of-death cap)
    int iterations = 4;                  ///< Solver iterations per step
};

// ============================================================================
// Body Definition
// ============================================================================

/**
 * @brief Describes how to create a physics body.
 *
 * @example
 * @code
 * PhysicsBodyDef def;
 * def.type = PhysicsBodyType::Dynamic;
 * def.shape = PhysicsShape::Box;
 * def.position = {0.0f, 10.0f};
 * def.extents = {0.5f, 0.5f};
 * def.mass = 1.0f;
 * auto id = physicsScene->createBody(def);
 * @endcode
 */
struct PhysicsBodyDef {
    PhysicsBodyType type = PhysicsBodyType::Dynamic;  ///< Body type
    PhysicsShape shape = PhysicsShape::Box;           ///< Collision shape
    glm::vec2 position = {0.0f, 0.0f};                ///< Initial position
    float rotation = 0.0f;                            ///< Initial rotation (radians)
    glm::vec2 extents = {0.5f, 0.5f};  ///< Half-extents (box) or {radius, 0} (circle)
    float mass = 1.0f;                 ///< Mass (kg); ignored for Static/Kinematic
    float friction = 0.3f;             ///< Surface friction coefficient
    float restitution = 0.2f;          ///< Bounciness (0 = no bounce, 1 = perfect)
    float linearDamping = 1.0f;  ///< Linear velocity decay rate (1/s). Each physics step applies
                                 ///< v *= 1/(1 + linearDamping * dt), giving exponential decay of
                                 ///< approximately e^(-linearDamping) per second (approximately
                                 ///< timestep-independent at typical fixed timesteps). A value of
                                 ///< 1.0 retains ~37% of velocity per second; 0 = no damping.
                                 ///< Must be >= 0 (negative values are clamped to 0).
    bool isSensor = false;       ///< If true, triggers callbacks but no response

    // -----------------------------------------------------------------
    // Factory methods — named constructors for common body configurations
    // -----------------------------------------------------------------

    /**
     * @brief Create a dynamic box body definition.
     * @param pos       Initial position
     * @param halfExt   Half-extents {halfWidth, halfHeight}
     * @param mass      Mass in kg (default 1.0)
     * @param restitution Bounciness 0–1 (default 0.2)
     * @param friction  Surface friction (default 0.3)
     * @note linearDamping defaults to 1.0 (decay rate 1/s). Override on the
     *       returned def if you need a different value (e.g. 0.0 for space).
     */
    static PhysicsBodyDef dynamicBox(glm::vec2 pos, glm::vec2 halfExt, float mass = 1.0f,
                                     float restitution = 0.2f, float friction = 0.3f) {
        PhysicsBodyDef d;
        d.type = PhysicsBodyType::Dynamic;
        d.shape = PhysicsShape::Box;
        d.position = pos;
        d.extents = halfExt;
        d.mass = mass;
        d.restitution = restitution;
        d.friction = friction;
        return d;
    }

    /**
     * @brief Create a dynamic circle body definition.
     * @param pos       Initial position
     * @param radius    Circle radius
     * @param mass      Mass in kg (default 1.0)
     * @param restitution Bounciness 0–1 (default 0.2)
     * @param friction  Surface friction (default 0.1)
     * @note linearDamping defaults to 1.0 (decay rate 1/s). Override on the
     *       returned def if you need a different value (e.g. 0.0 for space).
     */
    static PhysicsBodyDef dynamicCircle(glm::vec2 pos, float radius, float mass = 1.0f,
                                        float restitution = 0.2f, float friction = 0.1f) {
        PhysicsBodyDef d;
        d.type = PhysicsBodyType::Dynamic;
        d.shape = PhysicsShape::Circle;
        d.position = pos;
        d.extents = {radius, 0.0f};
        d.mass = mass;
        d.restitution = restitution;
        d.friction = friction;
        return d;
    }

    /**
     * @brief Create a static box body definition (walls, platforms).
     * @param pos       Position
     * @param halfExt   Half-extents {halfWidth, halfHeight}
     */
    static PhysicsBodyDef staticBox(glm::vec2 pos, glm::vec2 halfExt) {
        PhysicsBodyDef d;
        d.type = PhysicsBodyType::Static;
        d.shape = PhysicsShape::Box;
        d.position = pos;
        d.extents = halfExt;
        d.mass = 0.0f;
        return d;
    }

    /**
     * @brief Create a kinematic box body definition (moving platforms).
     * @param pos       Initial position
     * @param halfExt   Half-extents {halfWidth, halfHeight}
     */
    static PhysicsBodyDef kinematicBox(glm::vec2 pos, glm::vec2 halfExt) {
        PhysicsBodyDef d;
        d.type = PhysicsBodyType::Kinematic;
        d.shape = PhysicsShape::Box;
        d.position = pos;
        d.extents = halfExt;
        d.mass = 0.0f;
        return d;
    }
};

// ============================================================================
// Body State
// ============================================================================

/**
 * @brief Runtime state of a physics body.
 */
struct PhysicsBodyState {
    glm::vec2 position = {0.0f, 0.0f};  ///< Current position
    float rotation = 0.0f;              ///< Current rotation (radians)
    glm::vec2 velocity = {0.0f, 0.0f};  ///< Current linear velocity
    bool isAwake = true;                ///< Whether the body is awake (simulated)
};

// ============================================================================
// Collision Events
// ============================================================================

/**
 * @brief Describes a collision between two physics bodies.
 */
struct CollisionEvent {
    PhysicsBodyId bodyA = INVALID_PHYSICS_BODY_ID;  ///< First body in collision
    PhysicsBodyId bodyB = INVALID_PHYSICS_BODY_ID;  ///< Second body in collision
    glm::vec2 contactPoint = {0.0f, 0.0f};          ///< Approximate contact point
    glm::vec2 normal = {0.0f, 0.0f};                ///< Collision normal (from A to B)
    float depth = 0.0f;                             ///< Penetration depth
};

/**
 * @brief Callback type for collision events.
 */
using CollisionCallback = std::function<void(const CollisionEvent&)>;

// ============================================================================
// Raycast
// ============================================================================

/**
 * @brief Result of a raycast query.
 */
struct RaycastHit {
    PhysicsBodyId bodyId = INVALID_PHYSICS_BODY_ID;  ///< Body that was hit
    glm::vec2 point = {0.0f, 0.0f};                  ///< World-space hit point
    glm::vec2 normal = {0.0f, 0.0f};                 ///< Surface normal at hit point
    float distance = 0.0f;                           ///< Distance from ray origin to hit
};

}  // namespace vde
