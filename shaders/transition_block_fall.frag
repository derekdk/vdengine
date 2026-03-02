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

bool isBlockBorder(vec2 uv, vec2 blockUV) {
    vec2 safeBlockUV = max(blockUV, vec2(1e-6));
    vec2 local = fract(uv / safeBlockUV);

    // 2-pixel border in screen space, converted to block-local UV.
    vec2 pixelUV = vec2(abs(dFdx(fragUV.x)), abs(dFdy(fragUV.y)));
    vec2 border = (2.0 * pixelUV) / safeBlockUV;

    return (local.x <= border.x) || (local.x >= 1.0 - border.x) ||
           (local.y <= border.y) || (local.y >= 1.0 - border.y);
}

void main() {
    vec4 dst = texture(destTexture, fragUV);

    vec2 blockUV = vec2(max(transition.param0, 1e-6), max(transition.param1, 1e-6));
    vec2 blockIndex = floor(fragUV / blockUV);

    // Random stagger per block: later-starting blocks wait longer before falling.
    float randomValue = hash12(blockIndex + vec2(transition.direction, transition.direction * 1.618));
    float startTime = randomValue * 0.75;

    if (transition.progress <= startTime) {
        if (isBlockBorder(fragUV, blockUV)) {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        outColor = texture(sourceTexture, fragUV);
        return;
    }

    float localProgress = clamp((transition.progress - startTime) / (1.0 - startTime), 0.0, 1.0);

    // Move each block downward until it passes beyond the screen.
    float fallDistance = 1.0 + blockUV.y * 2.0;

    // Add subtle horizontal momentum per block for visual interest.
    float driftSign = (hash12(blockIndex + vec2(19.37, 47.11) + transition.direction) < 0.5) ? -1.0 : 1.0;
    float driftStrength = mix(0.35, 1.0, hash12(blockIndex + vec2(73.21, 11.49) + transition.direction * 0.5));
    float maxDrift = blockUV.x * 0.45 * driftStrength;
    float driftX = driftSign * maxDrift * (localProgress * localProgress);

    vec2 sourceUV = fragUV + vec2(driftX, localProgress * fallDistance);

    if (sourceUV.x < 0.0 || sourceUV.x > 1.0 || sourceUV.y < 0.0 || sourceUV.y > 1.0) {
        outColor = dst;
        return;
    }

    if (isBlockBorder(sourceUV, blockUV)) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    outColor = texture(sourceTexture, sourceUV);
}
