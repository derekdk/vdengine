#include <vde/api/AnimatedSpriteEntity.h>

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

void AnimatedSpriteEntity::update(float deltaTime) {
    if (!m_isPlaying || m_isPaused || deltaTime <= 0.0f || m_speed <= 0.0f) {
        return;
    }

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
            return;
        }

        remaining -= frameRemaining;
        m_currentFrameElapsed = 0.0f;
        advanceFrame();
    }
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

}  // namespace vde