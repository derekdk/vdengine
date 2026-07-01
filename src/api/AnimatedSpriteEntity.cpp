#include <vde/api/AnimatedSpriteEntity.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vde {

void AnimatedSpriteEntity::setSpriteSheet(std::shared_ptr<SpriteSheet> sheet) {
    m_spriteSheet = std::move(sheet);
    setTexture(m_spriteSheet ? m_spriteSheet->getTexture() : nullptr);

    if (currentAnimation() != nullptr) {
        applyCurrentFrame();
    }
}

void AnimatedSpriteEntity::addAnimation(const std::string& name, SpriteAnimation animation) {
    if (name.empty()) {
        throw std::invalid_argument("AnimatedSpriteEntity animation name cannot be empty");
    }
    if (animation.getFrameCount() == 0) {
        throw std::invalid_argument(
            "AnimatedSpriteEntity animation must contain at least one frame");
    }

    if (animation.getName().empty()) {
        animation.setName(name);
    }

    m_animations.insert_or_assign(name, std::move(animation));

    if (m_currentAnimationName == name) {
        resetPlayback();
        m_finished = false;
        applyCurrentFrame();
    }
}

bool AnimatedSpriteEntity::hasAnimation(const std::string& name) const {
    return m_animations.find(name) != m_animations.end();
}

const SpriteAnimation& AnimatedSpriteEntity::getAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    if (it == m_animations.end()) {
        throw std::out_of_range("AnimatedSpriteEntity animation not found: " + name);
    }

    return it->second;
}

void AnimatedSpriteEntity::play(const std::string& name, bool reset) {
    (void)getAnimation(name);

    bool shouldReset = reset || m_currentAnimationName != name || m_finished;
    m_currentAnimationName = name;
    m_isPlaying = true;
    m_isPaused = false;
    m_finished = false;

    if (shouldReset) {
        resetPlayback();
        applyCurrentFrame();
        fireFrameCallbacks(m_currentAnimationName, m_currentFrameIndex);
    }
}

void AnimatedSpriteEntity::pause() {
    if (m_isPlaying) {
        m_isPlaying = false;
        m_isPaused = true;
    }
}

void AnimatedSpriteEntity::resume() {
    if (!m_currentAnimationName.empty() && m_isPaused && !m_finished) {
        m_isPlaying = true;
        m_isPaused = false;
    }
}

void AnimatedSpriteEntity::stop() {
    m_isPlaying = false;
    m_isPaused = false;
    m_finished = false;
    m_currentFrameElapsed = 0.0f;

    if (currentAnimation() != nullptr) {
        m_currentFrameIndex = 0;
        applyCurrentFrame();
    }
}

void AnimatedSpriteEntity::setSpeed(float speed) {
    if (speed < 0.0f) {
        throw std::invalid_argument("AnimatedSpriteEntity speed cannot be negative");
    }

    m_speed = speed;
}

void AnimatedSpriteEntity::onFrameEvent(const std::string& animName, int frameIndex,
                                        FrameCallback callback) {
    if (frameIndex < 0) {
        throw std::invalid_argument("AnimatedSpriteEntity frame event index cannot be negative");
    }
    if (callback == nullptr) {
        throw std::invalid_argument("AnimatedSpriteEntity frame event callback cannot be empty");
    }

    auto it = m_animations.find(animName);
    if (it == m_animations.end()) {
        throw std::out_of_range("AnimatedSpriteEntity animation not found: " + animName);
    }
    if (frameIndex >= it->second.getFrameCount()) {
        throw std::out_of_range("AnimatedSpriteEntity frame event index out of range");
    }

    m_frameCallbacks[animName][frameIndex].push_back(std::move(callback));
}

void AnimatedSpriteEntity::addConditionalTransition(const std::string& from, const std::string& to,
                                                    TransitionPredicate predicate,
                                                    float blendDuration,
                                                    BlendCallback blendCallback,
                                                    bool resetPlayback) {
    if (from.empty() || to.empty()) {
        throw std::invalid_argument("AnimatedSpriteEntity transition states cannot be empty");
    }
    if (predicate == nullptr) {
        throw std::invalid_argument("AnimatedSpriteEntity transition predicate cannot be empty");
    }
    if (blendDuration < 0.0f) {
        throw std::invalid_argument(
            "AnimatedSpriteEntity transition blend duration cannot be negative");
    }

    validateTransitionEndpoints(from, to);
    m_transitions[from].push_back({.toAnimation = to,
                                   .predicate = std::move(predicate),
                                   .blendDuration = blendDuration,
                                   .blendCallback = std::move(blendCallback),
                                   .resetPlayback = resetPlayback});
}

void AnimatedSpriteEntity::addFinishedTransition(const std::string& from, const std::string& to,
                                                 float blendDuration, BlendCallback blendCallback,
                                                 bool resetPlayback) {
    addConditionalTransition(
        from, to, [](const AnimatedSpriteEntity& entity) { return entity.isAnimationFinished(); },
        blendDuration, std::move(blendCallback), resetPlayback);
}

void AnimatedSpriteEntity::clearTransitions(const std::string& from) {
    m_transitions.erase(from);
}

void AnimatedSpriteEntity::clearAllTransitions() {
    m_transitions.clear();
    m_activeBlend = {};
}

void AnimatedSpriteEntity::update(float deltaTime) {
    if (deltaTime > 0.0f) {
        updateActiveBlend(deltaTime);
    }

    if (m_isPlaying && !m_isPaused && deltaTime > 0.0f && m_speed > 0.0f) {
        float remaining = deltaTime * m_speed;
        constexpr float kEpsilon = 1e-6f;

        while (remaining > kEpsilon && m_isPlaying) {
            const SpriteAnimation* animation = currentAnimation();
            if (animation == nullptr) {
                return;
            }

            const auto& frame = animation->getFrame(m_currentFrameIndex);
            float frameRemaining = frame.duration - m_currentFrameElapsed;

            if (remaining + kEpsilon < frameRemaining) {
                m_currentFrameElapsed += remaining;
                break;
            }

            remaining -= frameRemaining;
            m_currentFrameElapsed = 0.0f;
            advanceFrame();
        }
    }

    evaluateTransitions();
}

void AnimatedSpriteEntity::applyCurrentFrame() {
    const SpriteAnimation* animation = currentAnimation();
    if (animation == nullptr || m_spriteSheet == nullptr) {
        return;
    }

    const auto& frame = animation->getFrame(m_currentFrameIndex);
    const auto uv = m_spriteSheet->getUVRect(frame.spriteIndex);
    setUVRect(uv.u, uv.v, uv.width, uv.height);
}

const SpriteAnimation* AnimatedSpriteEntity::currentAnimation() const {
    if (m_currentAnimationName.empty()) {
        return nullptr;
    }

    auto it = m_animations.find(m_currentAnimationName);
    if (it == m_animations.end()) {
        return nullptr;
    }

    return &it->second;
}

void AnimatedSpriteEntity::resetPlayback() {
    m_currentFrameIndex = 0;
    m_currentFrameElapsed = 0.0f;
}

void AnimatedSpriteEntity::advanceFrame() {
    const SpriteAnimation* animation = currentAnimation();
    if (animation == nullptr) {
        return;
    }

    int nextFrame = m_currentFrameIndex + 1;
    if (nextFrame < animation->getFrameCount()) {
        m_currentFrameIndex = nextFrame;
        applyCurrentFrame();
        fireFrameCallbacks(m_currentAnimationName, m_currentFrameIndex);
        return;
    }

    if (animation->isLooping()) {
        m_currentFrameIndex = 0;
        applyCurrentFrame();
        fireFrameCallbacks(m_currentAnimationName, m_currentFrameIndex);
        return;
    }

    m_currentFrameIndex = animation->getFrameCount() - 1;
    m_isPlaying = false;
    m_isPaused = false;
    m_finished = true;
}

void AnimatedSpriteEntity::fireFrameCallbacks(const std::string& animName, int frameIndex) {
    auto animIt = m_frameCallbacks.find(animName);
    if (animIt == m_frameCallbacks.end()) {
        return;
    }

    auto frameIt = animIt->second.find(frameIndex);
    if (frameIt == animIt->second.end()) {
        return;
    }

    for (const auto& callback : frameIt->second) {
        callback();
    }
}

void AnimatedSpriteEntity::validateTransitionEndpoints(const std::string& from,
                                                       const std::string& to) const {
    (void)getAnimation(from);
    (void)getAnimation(to);
}

void AnimatedSpriteEntity::evaluateTransitions() {
    if (m_currentAnimationName.empty()) {
        return;
    }

    auto transitionsIt = m_transitions.find(m_currentAnimationName);
    if (transitionsIt == m_transitions.end()) {
        return;
    }

    const std::string fromAnimation = m_currentAnimationName;
    for (const auto& transition : transitionsIt->second) {
        if (transition.predicate != nullptr && transition.predicate(*this)) {
            beginTransition(fromAnimation, transition);
            return;
        }
    }
}

void AnimatedSpriteEntity::beginTransition(const std::string& fromAnimation,
                                           const TransitionRule& transition) {
    play(transition.toAnimation, transition.resetPlayback);

    if (transition.blendDuration > 0.0f) {
        m_activeBlend = {.fromAnimation = fromAnimation,
                         .toAnimation = transition.toAnimation,
                         .duration = transition.blendDuration,
                         .elapsed = 0.0f,
                         .progress = 0.0f,
                         .callback = transition.blendCallback,
                         .active = true};
        if (m_activeBlend.callback != nullptr) {
            m_activeBlend.callback(*this, m_activeBlend.fromAnimation, m_activeBlend.toAnimation,
                                   0.0f);
        }
        return;
    }

    m_activeBlend = {};
}

void AnimatedSpriteEntity::updateActiveBlend(float deltaTime) {
    if (!m_activeBlend.active || m_activeBlend.duration <= 0.0f) {
        return;
    }

    m_activeBlend.elapsed = std::min(m_activeBlend.elapsed + deltaTime, m_activeBlend.duration);
    m_activeBlend.progress =
        std::clamp(m_activeBlend.elapsed / m_activeBlend.duration, 0.0f, 1.0f);

    if (m_activeBlend.callback != nullptr) {
        m_activeBlend.callback(*this, m_activeBlend.fromAnimation, m_activeBlend.toAnimation,
                               m_activeBlend.progress);
    }

    if (m_activeBlend.elapsed >= m_activeBlend.duration) {
        m_activeBlend = {};
    }
}

}  // namespace vde