#version 450

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragTint;

// Texture sampler
layout(set = 0, binding = 1) uniform sampler2D texSampler;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);
    
    // Apply tint (multiplicative blending)
    outColor = texColor * fragTint;
    
    // Discard fully transparent pixels
    if (outColor.a < 0.01) {
        discard;
    }
}
