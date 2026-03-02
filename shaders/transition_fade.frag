#version 450

// Cross-fade transition: simple alpha blend between source and destination.

layout(location = 0) in vec2 fragUV;

layout(binding = 0) uniform sampler2D sourceTexture;
layout(binding = 1) uniform sampler2D destTexture;

layout(push_constant) uniform TransitionPC {
    float progress;   // 0.0 to 1.0
    float direction;  // unused for fade
    float param0;     // unused
    float param1;     // unused
} transition;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);
    outColor = mix(src, dst, transition.progress);
}
