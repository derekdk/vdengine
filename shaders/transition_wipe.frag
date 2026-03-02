#version 450

// Directional wipe transition.
// direction encoding: 0=Left, 1=Right, 2=Up, 3=Down

layout(location = 0) in vec2 fragUV;

layout(binding = 0) uniform sampler2D sourceTexture;
layout(binding = 1) uniform sampler2D destTexture;

layout(push_constant) uniform TransitionPC {
    float progress;   // 0.0 to 1.0
    float direction;  // 0=Left, 1=Right, 2=Up, 3=Down
    float param0;     // unused
    float param1;     // unused
} transition;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);

    float edge;
    if (transition.direction < 0.5)
        edge = fragUV.x;           // Left wipe
    else if (transition.direction < 1.5)
        edge = 1.0 - fragUV.x;    // Right wipe
    else if (transition.direction < 2.5)
        edge = 1.0 - fragUV.y;    // Up wipe
    else
        edge = fragUV.y;           // Down wipe

    // Soft edge for a slightly anti-aliased boundary
    float softness = 0.01;
    float t = smoothstep(transition.progress - softness, transition.progress + softness, edge);
    outColor = mix(dst, src, t);
}
