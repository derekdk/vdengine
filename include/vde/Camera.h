#pragma once

/**
 * @file Camera.h
 * @brief Camera class for 3D view management
 * 
 * Provides functionality for view/projection matrix generation,
 * orbital camera control, and common camera operations.
 */

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vde {

/**
 * @brief Camera class for 3D view management.
 * 
 * Provides functionality for:
 * - Position and target-based camera setup
 * - Orbital camera control (pitch/yaw around target)
 * - View matrix generation using glm::lookAt
 * - Pan, zoom, and translate operations
 * 
 * Designed for hex grid games with tilted isometric-style views.
 */
class Camera {
public:
    Camera();
    
    // Direct camera setup
    
    /**
     * @brief Set the camera position in world space.
     * @param position World coordinates of the camera
     */
    void setPosition(const glm::vec3& position);
    
    /**
     * @brief Set the point the camera is looking at.
     * @param target World coordinates of the focus point
     */
    void setTarget(const glm::vec3& target);
    
    /**
     * @brief Set the up direction for the camera.
     * @param up Direction considered "up" (will be normalized)
     */
    void setUp(const glm::vec3& up);
    
    // Orbital camera setup
    
    /**
     * @brief Set camera using orbital parameters around a target.
     * 
     * This provides a more intuitive way to position the camera
     * for strategy game views.
     * 
     * @param distance Distance from target point
     * @param pitch Angle above horizontal (degrees, 0=horizontal, 90=overhead)
     * @param yaw Rotation around Y axis (degrees)
     * @param target Point to orbit around
     */
    void setFromPitchYaw(float distance, float pitch, float yaw, const glm::vec3& target);
    
    // Camera movement
    
    /**
     * @brief Translate camera and target by a delta.
     * @param delta World-space translation vector
     */
    void translate(const glm::vec3& delta);
    
    /**
     * @brief Pan camera in the view plane.
     * @param deltaX Horizontal pan amount (world units)
     * @param deltaY Vertical pan amount (world units)
     */
    void pan(float deltaX, float deltaY);
    
    /**
     * @brief Zoom by moving camera toward/away from target.
     * @param delta Positive = zoom in, negative = zoom out
     */
    void zoom(float delta);
    
    // Getters
    
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getTarget() const { return m_target; }
    const glm::vec3& getUp() const { return m_up; }
    float getDistance() const { return m_distance; }
    float getPitch() const { return m_pitch; }
    float getYaw() const { return m_yaw; }
    
    /**
     * @brief Get the view matrix for rendering.
     * @return View matrix transforming world to camera space
     */
    glm::mat4 getViewMatrix() const;
    
    /**
     * @brief Get the forward direction (toward target).
     * @return Normalized forward vector
     */
    glm::vec3 getForward() const;
    
    /**
     * @brief Get the right direction (perpendicular to forward and up).
     * @return Normalized right vector
     */
    glm::vec3 getRight() const;
    
    // Projection methods
    
    /**
     * @brief Set perspective projection parameters.
     * @param fov Field of view in degrees
     * @param aspectRatio Width / height ratio
     * @param nearPlane Near clipping plane distance
     * @param farPlane Far clipping plane distance
     */
    void setPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);
    
    /**
     * @brief Set orthographic projection parameters.
     * @param left Left edge of the view volume
     * @param right Right edge of the view volume
     * @param bottom Bottom edge of the view volume
     * @param top Top edge of the view volume
     * @param nearPlane Near clipping plane distance
     * @param farPlane Far clipping plane distance
     */
    void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    
    /**
     * @brief Check if using orthographic projection.
     * @return true if orthographic, false if perspective
     */
    bool isOrthographic() const { return m_orthographic; }
    
    /**
     * @brief Update aspect ratio (e.g., on window resize).
     * @param aspectRatio Width / height ratio
     */
    void setAspectRatio(float aspectRatio);
    
    /**
     * @brief Set field of view (clamped to 10-120 degrees).
     * @param fov Field of view in degrees
     */
    void setFOV(float fov);
    
    /**
     * @brief Get the projection matrix (Vulkan-corrected with Y-flip).
     * @return Projection matrix
     */
    glm::mat4 getProjectionMatrix() const;
    
    /**
     * @brief Get combined view-projection matrix.
     * @return VP matrix (projection * view)
     */
    glm::mat4 getViewProjectionMatrix() const;
    
    // Projection accessors
    float getFOV() const { return m_fov; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getNearPlane() const { return m_nearPlane; }
    float getFarPlane() const { return m_farPlane; }
    
    // Configuration
    
    static constexpr float MIN_DISTANCE = 1.0f;
    static constexpr float MAX_DISTANCE = 100.0f;
    static constexpr float MIN_PITCH = 5.0f;   ///< Prevent going below horizon
    static constexpr float MAX_PITCH = 89.0f;  ///< Prevent gimbal lock at vertical
    
    /**
     * @brief Calculate optimal camera distance to fit content in viewport.
     * @param contentWidth World-space width of content to fit
     * @param viewportWidthPercent Percentage of viewport width to use (0.0-1.0)
     * @param pitch Camera pitch angle in degrees
     * @param fov Vertical field of view in degrees
     * @param aspectRatio Viewport aspect ratio (width/height)
     * @param padding Extra padding factor (1.0 = exact fit, 1.1 = 10% padding)
     * @return Calculated distance from target
     */
    static float calculateDistanceForContent(
        float contentWidth,
        float viewportWidthPercent = 1.0f,
        float pitch = 45.0f,
        float fov = 45.0f,
        float aspectRatio = 16.0f/9.0f,
        float padding = 1.1f);

private:
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
    
    // Orbital camera parameters
    float m_distance;
    float m_pitch;  ///< Degrees above horizontal
    float m_yaw;    ///< Degrees around Y axis
    
    // Projection parameters
    float m_fov = 45.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 200.0f;
    
    // Orthographic projection parameters
    bool m_orthographic = false;
    float m_orthoLeft = -1.0f;
    float m_orthoRight = 1.0f;
    float m_orthoBottom = -1.0f;
    float m_orthoTop = 1.0f;
    
    /**
     * @brief Update position from orbital parameters.
     */
    void updatePositionFromOrbit();
};

} // namespace vde
