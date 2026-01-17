#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // For now, just use the vertex color
    // Texture sampling will be added in a later task
    outColor = vec4(fragColor, 1.0);
}
