#version 450

// Vertex input from vertex buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// Push constant for model matrix (updated per-entity)
layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

// Set 0, Binding 0: UBO with model/view/proj (same as triangle shader)
// We'll use view and proj from this, and ignore the model
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;  // Unused (we use push.model instead)
    mat4 view;
    mat4 proj;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
}
