#version 450

// Circle reveal transition: expanding circle from center revealing the destination.

layout(location = 0) in vec2 fragUV;

layout(binding = 0) uniform sampler2D sourceTexture;
layout(binding = 1) uniform sampler2D destTexture;

layout(push_constant) uniform TransitionPC {
    float progress;   // 0.0 to 1.0
    float direction;  // unused (Center)
    float param0;     // aspect ratio (width/height)
    float param1;     // unused
} transition;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);

    // Distance from center, corrected for aspect ratio
    vec2 center = vec2(0.5);
    vec2 delta = fragUV - center;
    // Correct for aspect ratio so the circle is round
    float aspect = max(transition.param0, 1.0);
    delta.x *= aspect;

    float maxDist = length(vec2(aspect * 0.5, 0.5));
    float dist = length(delta) / maxDist;

    // Expand circle from center; radius grows with progress
    float radius = transition.progress;
    float softness = 0.02;
    float t = smoothstep(radius - softness, radius + softness, dist);

    // Inside the circle = destination, outside = source
    outColor = mix(dst, src, t);
}
