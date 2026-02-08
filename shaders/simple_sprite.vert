#version 450

// Per-vertex attributes (quad vertices)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;      // Unused but needed for Vertex structure
layout(location = 2) in vec2 inTexCoord;

// Push constants for per-sprite data
layout(push_constant) uniform PushConstants {
    mat4 model;        // Sprite transform
    vec4 tint;         // Color tint (r, g, b, a)
    vec4 uvRect;       // UV rectangle (u, v, width, height)
} pc;

// Uniform buffer for camera (same layout as mesh shaders)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;   // Unused
    mat4 view;
    mat4 proj;
} ubo;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTint;

void main() {
    // Apply model transform to vertex position
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    
    // Apply camera view-projection
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Apply UV offset and scale for sprite sheets
    fragTexCoord = inTexCoord * pc.uvRect.zw + pc.uvRect.xy;
    
    // Pass through tint color
    fragTint = pc.tint;
}
