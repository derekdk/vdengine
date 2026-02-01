#include <vde/Camera.h>
#include <algorithm>
#include <cmath>

namespace vde {

Camera::Camera() 
    : m_position(0.0f, 10.0f, 10.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_pitch(45.0f)
    , m_yaw(0.0f) {
    // Default distance for typical use
    m_distance = 20.0f;
    
    // Update position to match the distance
    updatePositionFromOrbit();
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
    // Update orbital parameters to match
    m_distance = glm::length(m_target - m_position);
}

void Camera::setTarget(const glm::vec3& target) {
    m_target = target;
    // Update orbital parameters to match
    m_distance = glm::length(m_target - m_position);
}

void Camera::setUp(const glm::vec3& up) {
    m_up = glm::normalize(up);
}

void Camera::setFromPitchYaw(float distance, float pitch, float yaw, const glm::vec3& target) {
    m_distance = std::clamp(distance, MIN_DISTANCE, MAX_DISTANCE);
    m_pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);
    m_yaw = yaw;
    m_target = target;
    updatePositionFromOrbit();
}

void Camera::updatePositionFromOrbit() {
    // Convert pitch/yaw to position on sphere around target
    float pitchRad = glm::radians(m_pitch);
    float yawRad = glm::radians(m_yaw);
    
    // Spherical to Cartesian conversion
    // Y is up, so pitch is angle from horizontal XZ plane
    m_position.x = m_target.x + m_distance * cos(pitchRad) * sin(yawRad);
    m_position.y = m_target.y + m_distance * sin(pitchRad);
    m_position.z = m_target.z + m_distance * cos(pitchRad) * cos(yawRad);
}

void Camera::translate(const glm::vec3& delta) {
    m_position += delta;
    m_target += delta;
}

void Camera::pan(float deltaX, float deltaY) {
    // Pan in the view plane
    glm::vec3 right = getRight();
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Move both position and target to maintain view direction
    glm::vec3 panDelta = right * deltaX + worldUp * deltaY;
    translate(panDelta);
}

void Camera::zoom(float delta) {
    // Move camera closer/further from target along view direction
    glm::vec3 forward = getForward();
    
    // Update distance
    m_distance -= delta;
    m_distance = std::clamp(m_distance, MIN_DISTANCE, MAX_DISTANCE);
    
    // Recalculate position
    m_position = m_target - forward * m_distance;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

glm::vec3 Camera::getForward() const {
    return glm::normalize(m_target - m_position);
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), m_up));
}

void Camera::setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    m_fov = glm::clamp(fov, 10.0f, 120.0f);
    m_aspectRatio = aspectRatio;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::setAspectRatio(float aspectRatio) {
    m_aspectRatio = aspectRatio;
}

void Camera::setFOV(float fov) {
    m_fov = glm::clamp(fov, 10.0f, 120.0f);
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_orthographic = true;
    m_orthoLeft = left;
    m_orthoRight = right;
    m_orthoBottom = bottom;
    m_orthoTop = top;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

glm::mat4 Camera::getProjectionMatrix() const {
    glm::mat4 proj;
    
    if (m_orthographic) {
        proj = glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_nearPlane, m_farPlane);
    } else {
        proj = glm::perspective(
            glm::radians(m_fov),
            m_aspectRatio,
            m_nearPlane,
            m_farPlane
        );
    }
    
    // Vulkan Y-flip: Vulkan has Y pointing down, unlike OpenGL
    proj[1][1] *= -1;
    
    return proj;
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

float Camera::calculateDistanceForContent(
    float contentWidth,
    float viewportWidthPercent,
    float pitch,
    float fov,
    float aspectRatio,
    float padding) {
    
    // Convert angles to radians
    const float pitchRad = glm::radians(pitch);
    const float fovRad = glm::radians(fov);
    
    // Calculate horizontal FOV from vertical FOV and aspect ratio
    const float halfFovH = std::atan(std::tan(fovRad / 2.0f) * aspectRatio);
    
    // Calculate effective visible width on ground plane
    const float effectiveWidth = 2.0f * std::tan(halfFovH) * (1.0f / std::cos(pitchRad)) * viewportWidthPercent;
    const float distance = (contentWidth * padding) / effectiveWidth;
    
    return std::clamp(distance, MIN_DISTANCE, MAX_DISTANCE);
}

} // namespace vde
