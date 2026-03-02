#version 450

// Random block-fall transition.
// param0 = block width in UV space
// param1 = block height in UV space
// direction = random seed

layout(location = 0) in vec2 fragUV;

layout(binding = 0) uniform sampler2D sourceTexture;
layout(binding = 1) uniform sampler2D destTexture;

layout(push_constant) uniform TransitionPC {
    float progress;
    float direction;
    float param0;
    float param1;
} transition;

layout(location = 0) out vec4 outColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec4 dst = texture(destTexture, fragUV);

    vec2 blockUV = vec2(max(transition.param0, 1e-6), max(transition.param1, 1e-6));
    vec2 blockIndex = floor(fragUV / blockUV);

    // Random stagger per block: later-starting blocks wait longer before falling.
    float randomValue = hash12(blockIndex + vec2(transition.direction, transition.direction * 1.618));
    float startTime = randomValue * 0.75;

    if (transition.progress <= startTime) {
        outColor = texture(sourceTexture, fragUV);
        return;
    }

    float localProgress = clamp((transition.progress - startTime) / (1.0 - startTime), 0.0, 1.0);

    // Move each block downward until it passes beyond the screen.
    float fallDistance = 1.0 + blockUV.y * 2.0;
    vec2 sourceUV = fragUV + vec2(0.0, localProgress * fallDistance);

    if (sourceUV.x < 0.0 || sourceUV.x > 1.0 || sourceUV.y < 0.0 || sourceUV.y > 1.0) {
        outColor = dst;
        return;
    }

    outColor = texture(sourceTexture, sourceUV);
}
