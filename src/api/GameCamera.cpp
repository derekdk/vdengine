/**
 * @file GameCamera.cpp
 * @brief Implementation of game camera classes
 */

#include <vde/VulkanContext.h>
#include <vde/api/Defaults.h>
#include <vde/api/GameCamera.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace vde {

namespace {

constexpr float kEpsilon = 0.0001f;

float smoothingFactor(float speed, float deltaTime) {
    if (speed <= 0.0f || deltaTime <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - std::exp(-speed * deltaTime);
}

float smoothToward(float current, float target, float speed, float deltaTime) {
    return current + ((target - current) * smoothingFactor(speed, deltaTime));
}

glm::vec2 smoothToward(const glm::vec2& current, const glm::vec2& target, float speed,
                       float deltaTime) {
    return current + ((target - current) * smoothingFactor(speed, deltaTime));
}

glm::vec2 clampMagnitude(const glm::vec2& value, float maxMagnitude) {
    if (maxMagnitude <= 0.0f) {
        return glm::vec2(0.0f);
    }

    const float lengthSquared = glm::dot(value, value);
    if (lengthSquared <= (maxMagnitude * maxMagnitude)) {
        return value;
    }

    const float scale = maxMagnitude / std::sqrt(lengthSquared);
    return value * scale;
}

glm::vec2 applyDeadzoneFollow(const glm::vec2& currentPosition, const glm::vec2& focusPosition,
                              const glm::vec2& deadzoneSize) {
    glm::vec2 desiredPosition = currentPosition;

    const float halfWidth = deadzoneSize.x * 0.5f;
    if (halfWidth > 0.0f) {
        if (focusPosition.x < (currentPosition.x - halfWidth)) {
            desiredPosition.x = focusPosition.x + halfWidth;
        } else if (focusPosition.x > (currentPosition.x + halfWidth)) {
            desiredPosition.x = focusPosition.x - halfWidth;
        }
    } else {
        desiredPosition.x = focusPosition.x;
    }

    const float halfHeight = deadzoneSize.y * 0.5f;
    if (halfHeight > 0.0f) {
        if (focusPosition.y < (currentPosition.y - halfHeight)) {
            desiredPosition.y = focusPosition.y + halfHeight;
        } else if (focusPosition.y > (currentPosition.y + halfHeight)) {
            desiredPosition.y = focusPosition.y - halfHeight;
        }
    } else {
        desiredPosition.y = focusPosition.y;
    }

    return desiredPosition;
}

}  // namespace

// ============================================================================
// GameCamera Base Implementation
// ============================================================================

void GameCamera::applyTo(VulkanContext& context) {
    // Copy view and projection matrices to the context's camera
    Camera& cam = context.getCamera();

    // Set projection parameters
    cam.setPerspective(60.0f, m_aspectRatio, m_nearPlane, m_farPlane);

    // Copy position and target from our internal camera
    // This ensures the VulkanContext's camera has the correct view matrix
    cam.setPosition(m_camera.getPosition());
    cam.setTarget(m_camera.getTarget());
    cam.setUp(m_camera.getUp());
}

Ray GameCamera::screenToWorldRay(float screenX, float screenY, float screenWidth,
                                 float screenHeight) const {
    // Mouse -> Vulkan NDC (Y down, Z 0..1)
    float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
    float ndcY = (2.0f * screenY / screenHeight) - 1.0f;

    glm::mat4 invVP = glm::inverse(getViewProjectionMatrix());

    glm::vec4 nearClip = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
    glm::vec4 farClip = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearClip /= nearClip.w;
    farClip /= farClip.w;

    Ray ray;
    ray.origin = glm::vec3(nearClip);
    ray.direction = glm::normalize(glm::vec3(farClip) - ray.origin);
    return ray;
}

// ============================================================================
// SimpleCamera Implementation
// ============================================================================

SimpleCamera::SimpleCamera() : GameCamera(), m_position(0.0f, 0.0f, 5.0f) {
    updateVectors();
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

SimpleCamera::SimpleCamera(const Position& position, const Direction& direction)
    : GameCamera(), m_position(position) {
    // Calculate pitch and yaw from direction
    glm::vec3 dir = glm::normalize(direction.toVec3());
    m_pitch = glm::degrees(std::asin(dir.y));
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));

    updateVectors();
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void SimpleCamera::setPosition(const Position& position) {
    m_position = position;
    updateVectors();
}

Position SimpleCamera::getPosition() const {
    return m_position;
}

void SimpleCamera::setDirection(const Direction& direction) {
    glm::vec3 dir = glm::normalize(direction.toVec3());
    m_pitch = glm::degrees(std::asin(dir.y));
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));
    updateVectors();
}

Direction SimpleCamera::getDirection() const {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    forward.y = std::sin(glm::radians(m_pitch));
    forward.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    return Direction(glm::normalize(forward));
}

void SimpleCamera::setFieldOfView(float fov) {
    m_fov = std::clamp(fov, 10.0f, 120.0f);
    updateProjection();
}

void SimpleCamera::move(const Direction& delta) {
    m_position.x += delta.x;
    m_position.y += delta.y;
    m_position.z += delta.z;
    updateVectors();
}

void SimpleCamera::rotate(float deltaPitch, float deltaYaw) {
    m_pitch += deltaPitch;
    m_yaw += deltaYaw;

    // Clamp pitch to avoid flipping
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateVectors();
}

void SimpleCamera::updateProjection() {
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void SimpleCamera::updateVectors() {
    // Calculate forward direction
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    forward.y = std::sin(glm::radians(m_pitch));
    forward.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    forward = glm::normalize(forward);

    // Set camera position and target
    glm::vec3 pos = m_position.toVec3();
    m_camera.setPosition(pos);
    m_camera.setTarget(pos + forward);
}

// ============================================================================
// OrbitCamera Implementation
// ============================================================================

OrbitCamera::OrbitCamera() : GameCamera(), m_target(0.0f, 0.0f, 0.0f) {
    updateCamera();
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

OrbitCamera::OrbitCamera(const Position& target, float distance, float pitch, float yaw)
    : GameCamera(), m_target(target), m_distance(distance), m_pitch(pitch), m_yaw(yaw) {
    updateCamera();
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void OrbitCamera::setTarget(const Position& target) {
    m_target = target;
    updateCamera();
}

Position OrbitCamera::getTarget() const {
    return m_target;
}

void OrbitCamera::setDistance(float distance) {
    m_distance = std::clamp(distance, m_minDistance, m_maxDistance);
    updateCamera();
}

void OrbitCamera::setPitch(float pitch) {
    m_pitch = std::clamp(pitch, m_minPitch, m_maxPitch);
    updateCamera();
}

void OrbitCamera::setYaw(float yaw) {
    // Wrap yaw to 0-360 range
    m_yaw = std::fmod(yaw, 360.0f);
    if (m_yaw < 0.0f) {
        m_yaw += 360.0f;
    }
    updateCamera();
}

void OrbitCamera::setFieldOfView(float fov) {
    m_fov = std::clamp(fov, 10.0f, 120.0f);
    updateProjection();
}

void OrbitCamera::rotate(float deltaPitch, float deltaYaw) {
    setPitch(m_pitch + deltaPitch);
    setYaw(m_yaw + deltaYaw);
}

void OrbitCamera::zoom(float delta) {
    setDistance(m_distance - delta);
}

void OrbitCamera::pan(float deltaX, float deltaY) {
    // Calculate right and up vectors in world space
    glm::vec3 forward = glm::normalize(m_target.toVec3() - m_camera.getPosition());
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, forward);

    // Move target
    glm::vec3 delta3D = right * deltaX + up * deltaY;
    m_target.x += delta3D.x;
    m_target.y += delta3D.y;
    m_target.z += delta3D.z;

    updateCamera();
}

void OrbitCamera::setZoomLimits(float minDistance, float maxDistance) {
    m_minDistance = minDistance;
    m_maxDistance = maxDistance;
    setDistance(m_distance);  // Re-clamp current distance
}

void OrbitCamera::setPitchLimits(float minPitch, float maxPitch) {
    m_minPitch = minPitch;
    m_maxPitch = maxPitch;
    setPitch(m_pitch);  // Re-clamp current pitch
}

void OrbitCamera::updateProjection() {
    m_camera.setPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void OrbitCamera::updateCamera() {
    // Convert spherical coordinates to Cartesian
    float pitchRad = glm::radians(m_pitch);
    float yawRad = glm::radians(m_yaw);

    float cosPitch = std::cos(pitchRad);
    float sinPitch = std::sin(pitchRad);
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);

    // Calculate camera position relative to target
    glm::vec3 offset;
    offset.x = m_distance * cosPitch * sinYaw;
    offset.y = m_distance * sinPitch;
    offset.z = m_distance * cosPitch * cosYaw;

    glm::vec3 cameraPos = m_target.toVec3() + offset;

    m_camera.setPosition(cameraPos);
    m_camera.setTarget(m_target.toVec3());
}

// ============================================================================
// Camera2D Implementation
// ============================================================================

Camera2D::Camera2D() : GameCamera(), m_position(0.0f, 0.0f, 0.0f), m_zoomTarget(m_zoom) {
    updateCamera();
}

Camera2D::Camera2D(float width, float height)
    : GameCamera(), m_position(0.0f, 0.0f, 0.0f), m_viewportWidth(width), m_viewportHeight(height),
      m_zoomTarget(m_zoom) {
    m_aspectRatio = width / height;
    updateCamera();
}

void Camera2D::setPosition(float x, float y) {
    m_position.x = x;
    m_position.y = y;
    updateCamera();
}

void Camera2D::setPosition(const Position& pos) {
    m_position = pos;
    updateCamera();
}

void Camera2D::setZoom(float zoom) {
    m_zoom = std::max(0.01f, zoom);
    m_zoomTarget = m_zoom;
    m_zoomSmoothing = 0.0f;
    updateCamera();
}

void Camera2D::setRotation(float degrees) {
    m_rotation = degrees;
    updateCamera();
}

void Camera2D::setViewportSize(float width, float height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
    m_aspectRatio = width / height;
    updateCamera();
}

void Camera2D::move(float deltaX, float deltaY) {
    m_position.x += deltaX;
    m_position.y += deltaY;
    updateCamera();
}

void Camera2D::followTarget(const glm::vec2& target, float speed) {
    m_followTarget = target;
    m_followSpeed = speed;
    m_followRequested = true;
}

void Camera2D::setDeadzone(float width, float height) {
    m_deadzoneSize.x = std::max(0.0f, width);
    m_deadzoneSize.y = std::max(0.0f, height);
}

void Camera2D::setLookAhead(float maxDistance, float lookAheadSeconds, float smoothing) {
    m_lookAheadMaxDistance = std::max(0.0f, maxDistance);
    m_lookAheadSeconds = std::max(0.0f, lookAheadSeconds);
    m_lookAheadSmoothing = std::max(0.0f, smoothing);

    if (m_lookAheadMaxDistance <= 0.0f) {
        m_lookAheadOffset = glm::vec2(0.0f);
        m_hasPreviousFollowTarget = false;
    }
}

void Camera2D::shake(float intensity, float durationSec, float decayRate) {
    if (intensity <= 0.0f || durationSec <= 0.0f) {
        m_shakeIntensity = 0.0f;
        m_shakeDuration = 0.0f;
        m_shakeTimeRemaining = 0.0f;
        m_shakeTimeElapsed = 0.0f;
        m_renderOffset = glm::vec2(0.0f);
        updateCamera();
        return;
    }

    m_shakeIntensity = intensity;
    m_shakeDuration = durationSec;
    m_shakeTimeRemaining = durationSec;
    m_shakeDecayRate = std::max(0.0f, decayRate);
    m_shakeTimeElapsed = 0.0f;
}

void Camera2D::zoomTo(float targetZoom, float speed) {
    if (speed <= 0.0f) {
        setZoom(targetZoom);
        return;
    }

    m_zoomTarget = std::max(0.01f, targetZoom);
    m_zoomSmoothing = speed;
}

void Camera2D::update(float deltaTime) {
    const float clampedDeltaTime = std::max(0.0f, deltaTime);

    if (std::fabs(m_zoom - m_zoomTarget) > kEpsilon) {
        m_zoom = smoothToward(m_zoom, m_zoomTarget, m_zoomSmoothing, clampedDeltaTime);
        if (std::fabs(m_zoom - m_zoomTarget) <= kEpsilon) {
            m_zoom = m_zoomTarget;
        }
    }

    if (m_followRequested) {
        glm::vec2 desiredLookAhead(0.0f);
        if (m_lookAheadMaxDistance > 0.0f && m_hasPreviousFollowTarget &&
            clampedDeltaTime > kEpsilon) {
            const glm::vec2 velocity = (m_followTarget - m_previousFollowTarget) / clampedDeltaTime;
            desiredLookAhead =
                clampMagnitude(velocity * m_lookAheadSeconds, m_lookAheadMaxDistance);
        }

        if (m_lookAheadMaxDistance > 0.0f) {
            m_lookAheadOffset = smoothToward(m_lookAheadOffset, desiredLookAhead,
                                             m_lookAheadSmoothing, clampedDeltaTime);
        } else {
            m_lookAheadOffset = glm::vec2(0.0f);
        }

        const glm::vec2 currentPosition(m_position.x, m_position.y);
        const glm::vec2 desiredPosition = applyDeadzoneFollow(
            currentPosition, m_followTarget + m_lookAheadOffset, m_deadzoneSize);
        const glm::vec2 nextPosition =
            smoothToward(currentPosition, desiredPosition, m_followSpeed, clampedDeltaTime);
        m_position.x = nextPosition.x;
        m_position.y = nextPosition.y;

        m_previousFollowTarget = m_followTarget;
        m_hasPreviousFollowTarget = true;
        m_followRequested = false;
    } else if (glm::dot(m_lookAheadOffset, m_lookAheadOffset) > kEpsilon) {
        m_lookAheadOffset = smoothToward(m_lookAheadOffset, glm::vec2(0.0f), m_lookAheadSmoothing,
                                         clampedDeltaTime);
        if (glm::dot(m_lookAheadOffset, m_lookAheadOffset) <= kEpsilon) {
            m_lookAheadOffset = glm::vec2(0.0f);
        }
        m_hasPreviousFollowTarget = false;
    } else {
        m_hasPreviousFollowTarget = false;
    }

    m_renderOffset = glm::vec2(0.0f);
    if (m_shakeTimeRemaining > 0.0f) {
        m_shakeTimeElapsed += clampedDeltaTime;
        m_shakeTimeRemaining = std::max(0.0f, m_shakeTimeRemaining - clampedDeltaTime);

        const float durationRatio =
            (m_shakeDuration > kEpsilon) ? (m_shakeTimeRemaining / m_shakeDuration) : 0.0f;
        const float amplitude =
            m_shakeIntensity * durationRatio * std::exp(-m_shakeDecayRate * m_shakeTimeElapsed);

        m_renderOffset.x = std::sin(m_shakeTimeElapsed * 37.0f) * amplitude;
        m_renderOffset.y = std::cos(m_shakeTimeElapsed * 53.0f) * amplitude;

        if (m_shakeTimeRemaining <= 0.0f) {
            m_renderOffset = glm::vec2(0.0f);
        }
    }

    updateCamera();
}

void Camera2D::updateProjection() {
    // 2D camera uses orthographic projection
    // This is handled in updateCamera()
}

void Camera2D::updateCamera() {
    // Set camera for 2D rendering
    // Position camera looking down the -Z axis
    const float cameraX = m_position.x + m_renderOffset.x;
    const float cameraY = m_position.y + m_renderOffset.y;
    m_camera.setPosition(glm::vec3(cameraX, cameraY, 10.0f));
    m_camera.setTarget(glm::vec3(cameraX, cameraY, 0.0f));

    // Use orthographic projection for proper 2D rendering
    float halfWidth = (m_viewportWidth * 0.5f) / m_zoom;
    float halfHeight = (m_viewportHeight * 0.5f) / m_zoom;

    m_camera.setOrthographic(-halfWidth,   // left
                             halfWidth,    // right
                             -halfHeight,  // bottom
                             halfHeight,   // top
                             m_nearPlane,  // near
                             m_farPlane    // far
    );
}

void Camera2D::applyTo(VulkanContext& context) {
    // Ensure internal camera state is fresh before copying to context
    updateCamera();

    Camera& cam = context.getCamera();

    float halfWidth = (m_viewportWidth * 0.5f) / m_zoom;
    float halfHeight = (m_viewportHeight * 0.5f) / m_zoom;

    cam.setOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, m_nearPlane, m_farPlane);

    cam.setPosition(m_camera.getPosition());
    cam.setTarget(m_camera.getTarget());
    cam.setUp(m_camera.getUp());
}

Rect2D Camera2D::getVisibleRect() const {
    float halfWidth = (m_viewportWidth * 0.5f) / m_zoom;
    float halfHeight = (m_viewportHeight * 0.5f) / m_zoom;
    const float centerX = m_position.x + m_renderOffset.x;
    const float centerY = m_position.y + m_renderOffset.y;

    return Rect2D{.left = centerX - halfWidth,
                  .right = centerX + halfWidth,
                  .bottom = centerY - halfHeight,
                  .top = centerY + halfHeight};
}

}  // namespace vde
