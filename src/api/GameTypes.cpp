/**
 * @file GameTypes.cpp
 * @brief Implementation of game type utility functions
 */

#include <vde/api/GameTypes.h>
#include <glm/gtc/matrix_transform.hpp>

namespace vde {

glm::mat4 Transform::getMatrix() const {
    // Build TRS matrix: Translation * Rotation * Scale
    glm::mat4 model = glm::mat4(1.0f);
    
    // Translation
    model = glm::translate(model, position.toVec3());
    
    // Rotation (apply in order: yaw (Y), pitch (X), roll (Z))
    model = glm::rotate(model, glm::radians(rotation.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.roll), glm::vec3(0.0f, 0.0f, 1.0f));
    
    // Scale
    model = glm::scale(model, scale.toVec3());
    
    return model;
}

} // namespace vde
