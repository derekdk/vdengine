/**
 * @file PhysicsScene.cpp
 * @brief Implementation of 2D physics simulation
 *
 * Implements fixed-timestep accumulator, body management,
 * AABB broad-phase collision detection, and impulse-based resolution.
 */

#include <vde/api/PhysicsScene.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vde {

// ============================================================================
// Internal body structure
// ============================================================================

struct PhysicsBody {
    PhysicsBodyId id = INVALID_PHYSICS_BODY_ID;
    PhysicsBodyDef def;
    PhysicsBodyState state;
    PhysicsBodyState prevState;  // For interpolation
    glm::vec2 accumulatedForce = {0.0f, 0.0f};
    bool alive = true;
};

// ============================================================================
// Collision pair hash
// ============================================================================

struct PairHash {
    size_t operator()(const std::pair<PhysicsBodyId, PhysicsBodyId>& p) const {
        auto h1 = std::hash<PhysicsBodyId>{}(p.first);
        auto h2 = std::hash<PhysicsBodyId>{}(p.second);
        return h1 ^ (h2 * 2654435761u);
    }
};

using CollisionPairSet = std::unordered_set<std::pair<PhysicsBodyId, PhysicsBodyId>, PairHash>;

// ============================================================================
// Impl
// ============================================================================

struct PhysicsScene::Impl {
    PhysicsConfig config;
    std::unordered_map<PhysicsBodyId, PhysicsBody> bodies;
    PhysicsBodyId nextId = 1;
    float accumulator = 0.0f;
    float interpolationAlpha = 0.0f;
    int lastStepCount = 0;

    CollisionCallback onCollisionBegin;
    CollisionCallback onCollisionEnd;

    // Per-body callbacks
    std::unordered_map<PhysicsBodyId, CollisionCallback> bodyOnCollisionBegin;
    std::unordered_map<PhysicsBodyId, CollisionCallback> bodyOnCollisionEnd;

    // Track active collision pairs for collision-end detection
    CollisionPairSet activePairs;
    CollisionPairSet previousPairs;  // Pairs from the last step

    // -----------------------------------------------------------------
    // Body management
    // -----------------------------------------------------------------

    PhysicsBodyId createBody(const PhysicsBodyDef& def) {
        PhysicsBodyId id = nextId++;
        PhysicsBody body;
        body.id = id;
        body.def = def;
        body.state.position = def.position;
        body.state.rotation = def.rotation;
        body.state.velocity = {0.0f, 0.0f};
        body.state.isAwake = (def.type == PhysicsBodyType::Dynamic);
        body.prevState = body.state;
        bodies[id] = body;
        return id;
    }

    void destroyBody(PhysicsBodyId id) {
        auto it = bodies.find(id);
        if (it != bodies.end()) {
            it->second.alive = false;
            bodies.erase(it);
        }
    }

    PhysicsBody& getBody(PhysicsBodyId id) {
        auto it = bodies.find(id);
        if (it == bodies.end()) {
            throw std::runtime_error("PhysicsScene: body " + std::to_string(id) + " not found");
        }
        return it->second;
    }

    const PhysicsBody& getBody(PhysicsBodyId id) const {
        auto it = bodies.find(id);
        if (it == bodies.end()) {
            throw std::runtime_error("PhysicsScene: body " + std::to_string(id) + " not found");
        }
        return it->second;
    }

    // -----------------------------------------------------------------
    // AABB helpers
    // -----------------------------------------------------------------

    struct AABB {
        glm::vec2 min;
        glm::vec2 max;
    };

    static AABB computeAABB(const PhysicsBody& body) {
        AABB aabb;
        if (body.def.shape == PhysicsShape::Circle || body.def.shape == PhysicsShape::Sphere) {
            float r = body.def.extents.x;
            aabb.min = body.state.position - glm::vec2(r, r);
            aabb.max = body.state.position + glm::vec2(r, r);
        } else {
            // Box
            aabb.min = body.state.position - body.def.extents;
            aabb.max = body.state.position + body.def.extents;
        }
        return aabb;
    }

    static bool aabbOverlap(const AABB& a, const AABB& b) {
        return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y;
    }

    // -----------------------------------------------------------------
    // Collision detection & resolution
    // -----------------------------------------------------------------

    struct CollisionInfo {
        PhysicsBodyId idA;
        PhysicsBodyId idB;
        glm::vec2 normal;
        float depth;
        glm::vec2 contactPoint;
    };

    static bool computeBoxBoxCollision(const PhysicsBody& a, const PhysicsBody& b,
                                       CollisionInfo& info) {
        AABB aabbA = computeAABB(a);
        AABB aabbB = computeAABB(b);

        if (!aabbOverlap(aabbA, aabbB)) {
            return false;
        }

        // Compute overlap on each axis
        float overlapX1 = aabbA.max.x - aabbB.min.x;
        float overlapX2 = aabbB.max.x - aabbA.min.x;
        float overlapY1 = aabbA.max.y - aabbB.min.y;
        float overlapY2 = aabbB.max.y - aabbA.min.y;

        float overlapX = std::min(overlapX1, overlapX2);
        float overlapY = std::min(overlapY1, overlapY2);

        info.idA = a.id;
        info.idB = b.id;
        info.contactPoint = (a.state.position + b.state.position) * 0.5f;

        if (overlapX < overlapY) {
            // Separate on X axis
            info.depth = overlapX;
            info.normal = (a.state.position.x < b.state.position.x) ? glm::vec2(1.0f, 0.0f)
                                                                    : glm::vec2(-1.0f, 0.0f);
        } else {
            // Separate on Y axis
            info.depth = overlapY;
            info.normal = (a.state.position.y < b.state.position.y) ? glm::vec2(0.0f, 1.0f)
                                                                    : glm::vec2(0.0f, -1.0f);
        }

        return true;
    }

    static bool computeCircleCircleCollision(const PhysicsBody& a, const PhysicsBody& b,
                                             CollisionInfo& info) {
        float rA = a.def.extents.x;
        float rB = b.def.extents.x;
        glm::vec2 diff = b.state.position - a.state.position;
        float dist = glm::length(diff);
        float sumRadii = rA + rB;

        if (dist >= sumRadii || dist < 1e-8f) {
            return false;
        }

        info.idA = a.id;
        info.idB = b.id;
        info.depth = sumRadii - dist;
        info.normal = diff / dist;
        info.contactPoint = a.state.position + info.normal * rA;
        return true;
    }

    static bool computeCollision(const PhysicsBody& a, const PhysicsBody& b, CollisionInfo& info) {
        // For simplicity: if either is circle/sphere, and both are circle/sphere, use circle test
        bool aIsCircle =
            (a.def.shape == PhysicsShape::Circle || a.def.shape == PhysicsShape::Sphere);
        bool bIsCircle =
            (b.def.shape == PhysicsShape::Circle || b.def.shape == PhysicsShape::Sphere);

        if (aIsCircle && bIsCircle) {
            return computeCircleCircleCollision(a, b, info);
        }
        // Default: use box-box AABB collision for everything else
        return computeBoxBoxCollision(a, b, info);
    }

    void resolveCollision(PhysicsBody& a, PhysicsBody& b, const CollisionInfo& info) {
        // Sensor bodies: fire callback but do not resolve
        if (a.def.isSensor || b.def.isSensor) {
            return;
        }

        bool aStatic = (a.def.type != PhysicsBodyType::Dynamic);
        bool bStatic = (b.def.type != PhysicsBodyType::Dynamic);

        if (aStatic && bStatic) {
            return;  // Two non-dynamic bodies don't resolve
        }

        // Compute inverse masses
        float invMassA = aStatic ? 0.0f : (a.def.mass > 0.0f ? 1.0f / a.def.mass : 0.0f);
        float invMassB = bStatic ? 0.0f : (b.def.mass > 0.0f ? 1.0f / b.def.mass : 0.0f);
        float totalInvMass = invMassA + invMassB;

        if (totalInvMass < 1e-8f) {
            return;
        }

        // Positional correction (push apart)
        const float correctionPercent = 0.8f;
        const float slop = 0.01f;
        float correctionMag = std::max(info.depth - slop, 0.0f) / totalInvMass * correctionPercent;
        glm::vec2 correction = correctionMag * info.normal;

        if (!aStatic) {
            a.state.position -= invMassA * correction;
        }
        if (!bStatic) {
            b.state.position += invMassB * correction;
        }

        // Impulse-based velocity resolution
        glm::vec2 relVel = b.state.velocity - a.state.velocity;
        float velAlongNormal = glm::dot(relVel, info.normal);

        // Don't resolve if bodies are separating
        if (velAlongNormal > 0.0f) {
            return;
        }

        // Restitution (minimum of the two bodies)
        float e = std::min(a.def.restitution, b.def.restitution);

        // Impulse magnitude
        float j = -(1.0f + e) * velAlongNormal / totalInvMass;

        glm::vec2 impulse = j * info.normal;

        if (!aStatic) {
            a.state.velocity -= invMassA * impulse;
        }
        if (!bStatic) {
            b.state.velocity += invMassB * impulse;
        }

        // Friction impulse
        glm::vec2 tangent = relVel - glm::dot(relVel, info.normal) * info.normal;
        float tangentLen = glm::length(tangent);
        if (tangentLen > 1e-6f) {
            tangent /= tangentLen;
            float jt = -glm::dot(relVel, tangent) / totalInvMass;
            float mu = (a.def.friction + b.def.friction) * 0.5f;

            // Coulomb's law: clamp friction impulse
            glm::vec2 frictionImpulse;
            if (std::abs(jt) < j * mu) {
                frictionImpulse = jt * tangent;
            } else {
                frictionImpulse = -j * mu * tangent;
            }

            if (!aStatic) {
                a.state.velocity -= invMassA * frictionImpulse;
            }
            if (!bStatic) {
                b.state.velocity += invMassB * frictionImpulse;
            }
        }
    }

    // -----------------------------------------------------------------
    // Fixed-step simulation
    // -----------------------------------------------------------------

    void singleStep() {
        float dt = config.fixedTimestep;

        // Collect body pointers for iteration
        std::vector<PhysicsBodyId> ids;
        ids.reserve(bodies.size());
        for (auto& [id, body] : bodies) {
            ids.push_back(id);
        }

        // Save previous state for interpolation & integrate
        for (auto bodyId : ids) {
            auto& body = bodies[bodyId];
            body.prevState = body.state;

            if (body.def.type != PhysicsBodyType::Dynamic) {
                continue;
            }

            // Apply gravity
            glm::vec2 acceleration =
                config.gravity +
                body.accumulatedForce * (body.def.mass > 0.0f ? 1.0f / body.def.mass : 0.0f);
            body.state.velocity += acceleration * dt;

            // Apply damping
            body.state.velocity *= (1.0f - body.def.linearDamping);

            // Integrate position
            body.state.position += body.state.velocity * dt;

            // Clear accumulated force
            body.accumulatedForce = {0.0f, 0.0f};
        }

        // Collision detection and resolution (multiple iterations)
        CollisionPairSet currentFramePairs;

        for (int iter = 0; iter < config.iterations; ++iter) {
            for (size_t i = 0; i < ids.size(); ++i) {
                for (size_t j = i + 1; j < ids.size(); ++j) {
                    auto& bodyA = bodies[ids[i]];
                    auto& bodyB = bodies[ids[j]];

                    // Skip if both are static/kinematic
                    if (bodyA.def.type != PhysicsBodyType::Dynamic &&
                        bodyB.def.type != PhysicsBodyType::Dynamic) {
                        continue;
                    }

                    CollisionInfo info;
                    if (computeCollision(bodyA, bodyB, info)) {
                        auto pair = std::make_pair(std::min(info.idA, info.idB),
                                                   std::max(info.idA, info.idB));
                        currentFramePairs.insert(pair);

                        // Fire begin callbacks on first iteration only
                        if (iter == 0) {
                            // Only fire begin if this pair wasn't active last step
                            bool isNew = (previousPairs.find(pair) == previousPairs.end());
                            if (isNew) {
                                CollisionEvent evt;
                                evt.bodyA = info.idA;
                                evt.bodyB = info.idB;
                                evt.contactPoint = info.contactPoint;
                                evt.normal = info.normal;
                                evt.depth = info.depth;

                                if (onCollisionBegin) {
                                    onCollisionBegin(evt);
                                }
                                // Per-body callbacks
                                auto itA = bodyOnCollisionBegin.find(info.idA);
                                if (itA != bodyOnCollisionBegin.end()) {
                                    itA->second(evt);
                                }
                                auto itB = bodyOnCollisionBegin.find(info.idB);
                                if (itB != bodyOnCollisionBegin.end()) {
                                    itB->second(evt);
                                }
                            }
                        }

                        resolveCollision(bodyA, bodyB, info);
                    }
                }
            }
        }

        // Detect collision-end: pairs that were active previously but not now
        for (const auto& oldPair : previousPairs) {
            if (currentFramePairs.find(oldPair) == currentFramePairs.end()) {
                // Both bodies must still exist for a meaningful end event
                bool aExists = (bodies.find(oldPair.first) != bodies.end());
                bool bExists = (bodies.find(oldPair.second) != bodies.end());
                if (aExists && bExists) {
                    CollisionEvent evt;
                    evt.bodyA = oldPair.first;
                    evt.bodyB = oldPair.second;
                    // Contact point / normal / depth are zero for end events

                    if (onCollisionEnd) {
                        onCollisionEnd(evt);
                    }
                    // Per-body end callbacks
                    auto itA = bodyOnCollisionEnd.find(oldPair.first);
                    if (itA != bodyOnCollisionEnd.end()) {
                        itA->second(evt);
                    }
                    auto itB = bodyOnCollisionEnd.find(oldPair.second);
                    if (itB != bodyOnCollisionEnd.end()) {
                        itB->second(evt);
                    }
                }
            }
        }

        // Update active pairs for the next step
        activePairs = currentFramePairs;
        previousPairs = currentFramePairs;
    }

    void step(float deltaTime) {
        accumulator += deltaTime;
        lastStepCount = 0;

        int steps = 0;
        while (accumulator >= config.fixedTimestep && steps < config.maxSubSteps) {
            singleStep();
            accumulator -= config.fixedTimestep;
            steps++;
        }

        // Clamp accumulator to prevent spiral of death
        if (accumulator > config.fixedTimestep) {
            accumulator = config.fixedTimestep;
        }

        lastStepCount = steps;
        interpolationAlpha =
            config.fixedTimestep > 0.0f ? accumulator / config.fixedTimestep : 0.0f;
    }
};

// ============================================================================
// Public interface (delegates to Impl)
// ============================================================================

PhysicsScene::PhysicsScene(const PhysicsConfig& config) : m_impl(std::make_unique<Impl>()) {
    m_impl->config = config;
}

PhysicsScene::~PhysicsScene() = default;

PhysicsScene::PhysicsScene(PhysicsScene&&) noexcept = default;
PhysicsScene& PhysicsScene::operator=(PhysicsScene&&) noexcept = default;

PhysicsBodyId PhysicsScene::createBody(const PhysicsBodyDef& def) {
    return m_impl->createBody(def);
}

void PhysicsScene::destroyBody(PhysicsBodyId id) {
    m_impl->destroyBody(id);
}

PhysicsBodyState PhysicsScene::getBodyState(PhysicsBodyId id) const {
    return m_impl->getBody(id).state;
}

PhysicsBodyDef PhysicsScene::getBodyDef(PhysicsBodyId id) const {
    return m_impl->getBody(id).def;
}

bool PhysicsScene::hasBody(PhysicsBodyId id) const {
    return m_impl->bodies.find(id) != m_impl->bodies.end();
}

void PhysicsScene::applyForce(PhysicsBodyId id, const glm::vec2& force) {
    auto& body = m_impl->getBody(id);
    if (body.def.type == PhysicsBodyType::Dynamic) {
        body.accumulatedForce += force;
    }
}

void PhysicsScene::applyImpulse(PhysicsBodyId id, const glm::vec2& impulse) {
    auto& body = m_impl->getBody(id);
    if (body.def.type == PhysicsBodyType::Dynamic && body.def.mass > 0.0f) {
        body.state.velocity += impulse / body.def.mass;
    }
}

void PhysicsScene::setLinearVelocity(PhysicsBodyId id, const glm::vec2& velocity) {
    auto& body = m_impl->getBody(id);
    body.state.velocity = velocity;
}

void PhysicsScene::setBodyPosition(PhysicsBodyId id, const glm::vec2& position) {
    auto& body = m_impl->getBody(id);
    body.state.position = position;
}

void PhysicsScene::step(float deltaTime) {
    m_impl->step(deltaTime);
}

float PhysicsScene::getInterpolationAlpha() const {
    return m_impl->interpolationAlpha;
}

int PhysicsScene::getLastStepCount() const {
    return m_impl->lastStepCount;
}

const PhysicsConfig& PhysicsScene::getConfig() const {
    return m_impl->config;
}

void PhysicsScene::setGravity(const glm::vec2& gravity) {
    m_impl->config.gravity = gravity;
}

glm::vec2 PhysicsScene::getGravity() const {
    return m_impl->config.gravity;
}

void PhysicsScene::setOnCollisionBegin(CollisionCallback callback) {
    m_impl->onCollisionBegin = std::move(callback);
}

void PhysicsScene::setOnCollisionEnd(CollisionCallback callback) {
    m_impl->onCollisionEnd = std::move(callback);
}

void PhysicsScene::setBodyOnCollisionBegin(PhysicsBodyId id, CollisionCallback callback) {
    m_impl->bodyOnCollisionBegin[id] = std::move(callback);
}

void PhysicsScene::setBodyOnCollisionEnd(PhysicsBodyId id, CollisionCallback callback) {
    m_impl->bodyOnCollisionEnd[id] = std::move(callback);
}

bool PhysicsScene::raycast(const glm::vec2& origin, const glm::vec2& direction, float maxDistance,
                           RaycastHit& outHit) const {
    float dirLen = glm::length(direction);
    if (dirLen < 1e-8f || maxDistance <= 0.0f) {
        return false;
    }
    glm::vec2 dir = direction / dirLen;  // normalize

    bool hit = false;
    float closestDist = maxDistance;

    for (const auto& [id, body] : m_impl->bodies) {
        if (!body.alive)
            continue;

        PhysicsScene::Impl::AABB aabb = PhysicsScene::Impl::computeAABB(body);

        // Ray-AABB intersection (slab method)
        float tmin = 0.0f;
        float tmax = closestDist;

        // X slab
        if (std::abs(dir.x) < 1e-8f) {
            // Ray is parallel to X slab
            if (origin.x < aabb.min.x || origin.x > aabb.max.x) {
                continue;  // No intersection
            }
        } else {
            float invD = 1.0f / dir.x;
            float t1 = (aabb.min.x - origin.x) * invD;
            float t2 = (aabb.max.x - origin.x) * invD;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax)
                continue;
        }

        // Y slab
        if (std::abs(dir.y) < 1e-8f) {
            if (origin.y < aabb.min.y || origin.y > aabb.max.y) {
                continue;
            }
        } else {
            float invD = 1.0f / dir.y;
            float t1 = (aabb.min.y - origin.y) * invD;
            float t2 = (aabb.max.y - origin.y) * invD;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax)
                continue;
        }

        // We have a valid intersection at tmin
        if (tmin >= 0.0f && tmin < closestDist) {
            closestDist = tmin;
            outHit.bodyId = id;
            outHit.point = origin + dir * tmin;
            outHit.distance = tmin;

            // Compute hit normal (which face was hit)
            glm::vec2 hitPoint = outHit.point;
            float eps = 0.001f;
            if (std::abs(hitPoint.x - aabb.min.x) < eps)
                outHit.normal = {-1.0f, 0.0f};
            else if (std::abs(hitPoint.x - aabb.max.x) < eps)
                outHit.normal = {1.0f, 0.0f};
            else if (std::abs(hitPoint.y - aabb.min.y) < eps)
                outHit.normal = {0.0f, -1.0f};
            else if (std::abs(hitPoint.y - aabb.max.y) < eps)
                outHit.normal = {0.0f, 1.0f};
            else
                outHit.normal = -dir;  // fallback

            hit = true;
        }
    }

    return hit;
}

std::vector<PhysicsBodyId> PhysicsScene::queryAABB(const glm::vec2& min,
                                                   const glm::vec2& max) const {
    std::vector<PhysicsBodyId> result;

    PhysicsScene::Impl::AABB queryBox;
    queryBox.min = min;
    queryBox.max = max;

    for (const auto& [id, body] : m_impl->bodies) {
        if (!body.alive)
            continue;

        PhysicsScene::Impl::AABB bodyAABB = PhysicsScene::Impl::computeAABB(body);
        if (PhysicsScene::Impl::aabbOverlap(queryBox, bodyAABB)) {
            result.push_back(id);
        }
    }

    return result;
}

size_t PhysicsScene::getBodyCount() const {
    return m_impl->bodies.size();
}

size_t PhysicsScene::getActiveBodyCount() const {
    size_t count = 0;
    for (const auto& [id, body] : m_impl->bodies) {
        if (body.alive) {
            ++count;
        }
    }
    return count;
}

}  // namespace vde
