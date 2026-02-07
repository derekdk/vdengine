/**
 * @file GameCamera.cpp
 * @brief Implementation of game camera classes
 */

#include <vde/VulkanContext.h>
#include <vde/api/GameCamera.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace vde {

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

SimpleCamera::SimpleCamera()
    : GameCamera(), m_position(0.0f, 0.0f, 5.0f), m_pitch(0.0f), m_yaw(-90.0f)  // Looking toward -Z
      ,
      m_fov(60.0f) {
    updateVectors();
    updateProjection();
}

SimpleCamera::SimpleCamera(const Position& position, const Direction& direction)
    : GameCamera(), m_position(position), m_fov(60.0f) {
    // Calculate pitch and yaw from direction
    glm::vec3 dir = glm::normalize(direction.toVec3());
    m_pitch = glm::degrees(std::asin(dir.y));
    m_yaw = glm::degrees(std::atan2(dir.z, dir.x));

    updateVectors();
    updateProjection();
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

OrbitCamera::OrbitCamera()
    : GameCamera(), m_target(0.0f, 0.0f, 0.0f), m_distance(10.0f), m_pitch(45.0f), m_yaw(0.0f),
      m_fov(60.0f), m_minDistance(1.0f), m_maxDistance(100.0f), m_minPitch(5.0f),
      m_maxPitch(85.0f) {
    updateCamera();
    updateProjection();
}

OrbitCamera::OrbitCamera(const Position& target, float distance, float pitch, float yaw)
    : GameCamera(), m_target(target), m_distance(distance), m_pitch(pitch), m_yaw(yaw),
      m_fov(60.0f), m_minDistance(1.0f), m_maxDistance(100.0f), m_minPitch(5.0f),
      m_maxPitch(85.0f) {
    updateCamera();
    updateProjection();
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
    if (m_yaw < 0.0f)
        m_yaw += 360.0f;
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

Camera2D::Camera2D()
    : GameCamera(), m_position(0.0f, 0.0f, 0.0f), m_zoom(1.0f), m_rotation(0.0f),
      m_viewportWidth(1920.0f), m_viewportHeight(1080.0f) {
    updateCamera();
}

Camera2D::Camera2D(float width, float height)
    : GameCamera(), m_position(0.0f, 0.0f, 0.0f), m_zoom(1.0f), m_rotation(0.0f),
      m_viewportWidth(width), m_viewportHeight(height) {
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

void Camera2D::updateProjection() {
    // 2D camera uses orthographic projection
    // This is handled in updateCamera()
}

void Camera2D::updateCamera() {
    // Set camera for 2D rendering
    // Position camera looking down the -Z axis
    m_camera.setPosition(glm::vec3(m_position.x, m_position.y, 10.0f));
    m_camera.setTarget(glm::vec3(m_position.x, m_position.y, 0.0f));

    // Use orthographic projection for proper 2D rendering
    float halfWidth = (m_viewportWidth * 0.5f) / m_zoom;
    float halfHeight = (m_viewportHeight * 0.5f) / m_zoom;

    m_camera.setOrthographic(-halfWidth,   // left
                             halfWidth,    // right
                             -halfHeight,  // bottom
                             halfHeight,   // top
                             0.1f,         // near
                             100.0f        // far
    );
}

}  // namespace vde
