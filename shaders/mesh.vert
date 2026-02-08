#version 450

// Vertex input from vertex buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// Push constants for model matrix and material properties
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 albedo;        // RGB albedo + opacity
    vec4 emission;      // RGB emission + intensity
    float roughness;
    float metallic;
    float normalStrength;
    float padding;
} push;

// Set 0, Binding 0: UBO with model/view/proj
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;  // Unused (we use push.model instead)
    mat4 view;
    mat4 proj;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragWorldNormal;
layout(location = 4) out vec3 fragViewPos;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Pass vertex color (may be used as base color fallback)
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
    
    // Transform normal to world space (assuming uniform scale)
    // Note: For proper normal transformation with non-uniform scale,
    // use inverse transpose of model matrix
    mat3 normalMatrix = mat3(push.model);
    fragWorldNormal = normalize(normalMatrix * inColor); // Using color as normal for now
    
    // Calculate view position (camera position in world space)
    // This is a simplified approach - view matrix inverse would give exact position
    fragViewPos = -vec3(ubo.view[3]);
}
