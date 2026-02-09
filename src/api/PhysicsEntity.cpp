/**
 * @file PhysicsEntity.cpp
 * @brief Implementation of physics-driven entity classes
 *
 * Implements PhysicsEntity (mixin), PhysicsSpriteEntity, and PhysicsMeshEntity
 * with body lifecycle management, interpolated sync, and force helpers.
 */

#include <vde/api/PhysicsEntity.h>
#include <vde/api/PhysicsScene.h>
#include <vde/api/Scene.h>

#include <stdexcept>

namespace vde {

// ============================================================================
// PhysicsEntity (mixin)
// ============================================================================

PhysicsEntity::PhysicsEntity() = default;

PhysicsEntity::~PhysicsEntity() {
    // If we still have a body, destroy it
    if (m_physicsScene && m_physicsBodyId != INVALID_PHYSICS_BODY_ID) {
        if (m_physicsScene->hasBody(m_physicsBodyId)) {
            m_physicsScene->destroyBody(m_physicsBodyId);
        }
    }
}

// -----------------------------------------------------------------
// Body management
// -----------------------------------------------------------------

PhysicsBodyId PhysicsEntity::createPhysicsBody(const PhysicsBodyDef& def) {
    if (!m_physicsScene) {
        throw std::runtime_error("PhysicsEntity::createPhysicsBody: scene has no PhysicsScene");
    }

    // Destroy existing body first
    if (m_physicsBodyId != INVALID_PHYSICS_BODY_ID && m_physicsScene->hasBody(m_physicsBodyId)) {
        m_physicsScene->destroyBody(m_physicsBodyId);
    }

    m_physicsBodyId = m_physicsScene->createBody(def);

    // Initialise interpolation state
    m_prevPosition = def.position;
    m_prevRotation = def.rotation;

    // Place the visual entity at the initial position
    if (m_owner) {
        m_owner->setPosition(Position(def.position.x, def.position.y, 0.0f));
    }

    return m_physicsBodyId;
}

PhysicsBodyState PhysicsEntity::getPhysicsState() const {
    if (!m_physicsScene || m_physicsBodyId == INVALID_PHYSICS_BODY_ID) {
        throw std::runtime_error("PhysicsEntity::getPhysicsState: no valid physics body");
    }
    return m_physicsScene->getBodyState(m_physicsBodyId);
}

// -----------------------------------------------------------------
// Force / impulse helpers
// -----------------------------------------------------------------

void PhysicsEntity::applyForce(const glm::vec2& force) {
    if (m_physicsScene && m_physicsBodyId != INVALID_PHYSICS_BODY_ID) {
        m_physicsScene->applyForce(m_physicsBodyId, force);
    }
}

void PhysicsEntity::applyImpulse(const glm::vec2& impulse) {
    if (m_physicsScene && m_physicsBodyId != INVALID_PHYSICS_BODY_ID) {
        m_physicsScene->applyImpulse(m_physicsBodyId, impulse);
    }
}

void PhysicsEntity::setLinearVelocity(const glm::vec2& velocity) {
    if (m_physicsScene && m_physicsBodyId != INVALID_PHYSICS_BODY_ID) {
        m_physicsScene->setLinearVelocity(m_physicsBodyId, velocity);
    }
}

// -----------------------------------------------------------------
// Synchronisation
// -----------------------------------------------------------------

void PhysicsEntity::syncFromPhysics(float interpolationAlpha) {
    if (!m_physicsScene || m_physicsBodyId == INVALID_PHYSICS_BODY_ID || !m_owner) {
        return;
    }
    if (!m_physicsScene->hasBody(m_physicsBodyId)) {
        return;
    }

    auto state = m_physicsScene->getBodyState(m_physicsBodyId);

    // Interpolate between previous and current positions
    float alpha = interpolationAlpha;
    float interpX = m_prevPosition.x * (1.0f - alpha) + state.position.x * alpha;
    float interpY = m_prevPosition.y * (1.0f - alpha) + state.position.y * alpha;

    m_owner->setPosition(Position(interpX, interpY, 0.0f));

    // Update previous state for next frame
    m_prevPosition = state.position;
    m_prevRotation = state.rotation;
}

void PhysicsEntity::syncToPhysics() {
    if (!m_physicsScene || m_physicsBodyId == INVALID_PHYSICS_BODY_ID || !m_owner) {
        return;
    }
    if (!m_physicsScene->hasBody(m_physicsBodyId)) {
        return;
    }

    const auto& pos = m_owner->getPosition();
    m_physicsScene->setBodyPosition(m_physicsBodyId, glm::vec2(pos.x, pos.y));
}

// -----------------------------------------------------------------
// Attach / Detach
// -----------------------------------------------------------------

void PhysicsEntity::attachPhysics(Scene* scene) {
    if (scene) {
        m_physicsScene = scene->getPhysicsScene();
    }
}

void PhysicsEntity::detachPhysics() {
    if (m_physicsScene && m_physicsBodyId != INVALID_PHYSICS_BODY_ID) {
        if (m_physicsScene->hasBody(m_physicsBodyId)) {
            m_physicsScene->destroyBody(m_physicsBodyId);
        }
    }
    m_physicsBodyId = INVALID_PHYSICS_BODY_ID;
    m_physicsScene = nullptr;
}

// ============================================================================
// PhysicsSpriteEntity
// ============================================================================

PhysicsSpriteEntity::PhysicsSpriteEntity() {
    initPhysicsOwner(this);  // 'this' is-a Entity via SpriteEntity
}

void PhysicsSpriteEntity::onAttach(Scene* scene) {
    SpriteEntity::onAttach(scene);
    attachPhysics(scene);
}

void PhysicsSpriteEntity::onDetach() {
    detachPhysics();
    SpriteEntity::onDetach();
}

void PhysicsSpriteEntity::update(float deltaTime) {
    SpriteEntity::update(deltaTime);
}

void PhysicsSpriteEntity::render() {
    SpriteEntity::render();
}

// ============================================================================
// PhysicsMeshEntity
// ============================================================================

PhysicsMeshEntity::PhysicsMeshEntity() {
    initPhysicsOwner(this);  // 'this' is-a Entity via MeshEntity
}

void PhysicsMeshEntity::onAttach(Scene* scene) {
    MeshEntity::onAttach(scene);
    attachPhysics(scene);
}

void PhysicsMeshEntity::onDetach() {
    detachPhysics();
    MeshEntity::onDetach();
}

void PhysicsMeshEntity::update(float deltaTime) {
    MeshEntity::update(deltaTime);
}

void PhysicsMeshEntity::render() {
    MeshEntity::render();
}

}  // namespace vde
