#version 450

// Per-vertex attributes (quad vertices - binding 0)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Per-instance attributes (from sprite batch - binding 1)
// mat4 uses 4 consecutive locations (2, 3, 4, 5)
layout(location = 2) in mat4 inInstanceTransform;
layout(location = 6) in vec4 inInstanceTint;
layout(location = 7) in vec2 inUVOffset;
layout(location = 8) in vec2 inUVScale;
layout(location = 9) in uint inTextureIndex;

// Uniform buffer for camera (same layout as other shaders)
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTint;
layout(location = 2) flat out uint fragTextureIndex;

void main() {
    // Apply instance transform to vertex position
    vec4 worldPos = inInstanceTransform * vec4(inPosition, 1.0);
    
    // Apply camera view-projection (model matrix is identity for sprites)
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Apply UV offset and scale for texture atlas support
    fragTexCoord = inTexCoord * inUVScale + inUVOffset;
    
    // Pass through tint color
    fragTint = inInstanceTint;
    
    // Pass through texture index for array textures
    fragTextureIndex = inTextureIndex;
}
