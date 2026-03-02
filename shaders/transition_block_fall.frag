#version 450

// Random block-fall transition — proper forward-mapped blocks.
//
// Each source block carries its original image content as it falls
// downward off screen, revealing the destination beneath.  Blocks
// start falling at staggered random times and drift horizontally.
//
// UV convention (set by transition_fullscreen.vert):
//   fragUV.y = 1 at the top of the screen, 0 at the bottom.
//   "Falling down" means Y decreases.
//
// param0 = block width  in UV space
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

// Fast 2-component → 1-component hash.
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 blockUV = vec2(max(transition.param0, 1e-6), max(transition.param1, 1e-6));

    // Default: destination scene (revealed as source blocks fall away).
    outColor = texture(destTexture, fragUV);

    // Grid column of this fragment.
    float col = floor(fragUV.x / blockUV.x);
    // Grid row whose original top edge is at or below fragUV.y.
    float startRow = floor(fragUV.y / blockUV.y);

    // Maximum vertical distance any block can travel.
    float maxFallDist = 1.0 + blockUV.y * 2.0;
    // How many rows above us could have fallen far enough to reach us.
    float maxExtraRows = ceil(maxFallDist / blockUV.y) + 1.0;

    // 2-pixel border, expressed in block-local [0,1] coordinates.
    vec2 pixelUV = vec2(abs(dFdx(fragUV.x)), abs(dFdy(fragUV.y)));
    vec2 border  = (2.0 * pixelUV) / blockUV;

    // Search from the highest candidate row downward.  The first
    // (highest) block that covers this fragment wins — it is visually
    // "on top" of anything below it.
    for (float r = startRow + maxExtraRows; r >= startRow; r -= 1.0) {
        vec2 obIdx    = vec2(col, r);
        vec2 obTopLeft = obIdx * blockUV;

        // Skip blocks whose original position is above the source image.
        if (obTopLeft.y >= 1.0) continue;

        // Per-block random stagger.
        float rv = hash12(obIdx + vec2(transition.direction,
                                       transition.direction * 1.618));
        float st = rv * 0.75;

        float fallOffset = 0.0;
        float dx         = 0.0;

        if (transition.progress > st) {
            float lp = clamp((transition.progress - st) / (1.0 - st),
                             0.0, 1.0);
            fallOffset = lp * maxFallDist;

            // Horizontal drift.
            float ds = (hash12(obIdx + vec2(19.37, 47.11)
                               + transition.direction) < 0.5) ? -1.0 : 1.0;
            float dstr = mix(0.35, 1.0,
                             hash12(obIdx + vec2(73.21, 11.49)
                                    + transition.direction * 0.5));
            float md = blockUV.x * 0.45 * dstr;
            dx = ds * md * (lp * lp);
        }

        // Current screen position of this block (fall = −Y).
        vec2 curTL = obTopLeft + vec2(dx, -fallOffset);

        // Hit-test: does the displaced block cover our fragment?
        if (fragUV.x >= curTL.x          && fragUV.x < curTL.x + blockUV.x &&
            fragUV.y >= curTL.y          && fragUV.y < curTL.y + blockUV.y) {

            // Local [0,1] position within the displaced block.
            vec2 local = (fragUV - curTL) / blockUV;

            // Map back to the source texture at the block's *original* UV.
            vec2 srcUV = obTopLeft + local * blockUV;

            // Block border (2-pixel outline).
            if (local.x <= border.x || local.x >= 1.0 - border.x ||
                local.y <= border.y || local.y >= 1.0 - border.y) {
                outColor = vec4(0.0, 0.0, 0.0, 1.0);
            } else {
                outColor = texture(sourceTexture, srcUV);
            }
            return;
        }
    }
    // No source block covers this fragment — destination is already set.
}
