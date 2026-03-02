#include <vde/api/ScreenTransition.h>

#include <algorithm>

namespace vde {

void TransitionState::reset() {
    type = TransitionType::NONE;
    phase = TransitionPhase::NONE;
    targetScene.clear();
    halfDuration = 0.25f;
    elapsed = 0.0f;
    overlayAlpha = 0.0f;
    onComplete = nullptr;
}

void TransitionState::start(TransitionType transitionType, const std::string& target,
                            float duration) {
    type = transitionType;
    phase = TransitionPhase::FADING_OUT;
    targetScene = target;
    halfDuration = std::max(duration * 0.5f, 0.001f);  // avoid division by zero
    elapsed = 0.0f;
    overlayAlpha = 0.0f;
}

bool TransitionState::update(float deltaTime) {
    if (phase == TransitionPhase::NONE) {
        return false;
    }

    elapsed += deltaTime;
    bool midpointReached = false;

    if (phase == TransitionPhase::FADING_OUT) {
        // During fade-out, overlay alpha goes from 0 → 1
        overlayAlpha = std::clamp(elapsed / halfDuration, 0.0f, 1.0f);

        if (elapsed >= halfDuration) {
            // Midpoint: transition to fade-in phase
            midpointReached = true;
            phase = TransitionPhase::FADING_IN;
            elapsed = elapsed - halfDuration;  // carry over excess time
            overlayAlpha = 1.0f;
        }
    }

    if (phase == TransitionPhase::FADING_IN) {
        // During fade-in, overlay alpha goes from 1 → 0
        overlayAlpha = std::clamp(1.0f - (elapsed / halfDuration), 0.0f, 1.0f);

        if (elapsed >= halfDuration) {
            // Transition complete
            overlayAlpha = 0.0f;
            auto callback = std::move(onComplete);
            reset();
            if (callback) {
                callback();
            }
        }
    }

    return midpointReached;
}

}  // namespace vde
