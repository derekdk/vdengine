#include <vde/api/SpriteAnimation.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace vde {

SpriteAnimation::SpriteAnimation(std::string name) : m_name(std::move(name)) {}

void SpriteAnimation::addFrame(int spriteIndex, float duration) {
    if (spriteIndex < 0) {
        throw std::invalid_argument("SpriteAnimation frame index cannot be negative");
    }
    if (duration <= 0.0f) {
        throw std::invalid_argument("SpriteAnimation frame duration must be positive");
    }

    m_frames.push_back({spriteIndex, duration});
    m_totalDuration += duration;
}

void SpriteAnimation::setLooping(bool loop) {
    m_looping = loop;
}

void SpriteAnimation::setName(std::string name) {
    m_name = std::move(name);
}

int SpriteAnimation::getFrameAtTime(float time) const {
    if (m_frames.empty()) {
        throw std::logic_error("SpriteAnimation has no frames");
    }

    if (time < 0.0f) {
        time = 0.0f;
    }

    float localTime = time;
    if (m_looping && m_totalDuration > 0.0f) {
        localTime = std::fmod(localTime, m_totalDuration);
        if (localTime < 0.0f) {
            localTime += m_totalDuration;
        }
    } else if (localTime >= m_totalDuration) {
        return getFrameCount() - 1;
    }

    float accumulated = 0.0f;
    for (int index = 0; index < getFrameCount(); ++index) {
        accumulated += m_frames[static_cast<size_t>(index)].duration;
        if (localTime < accumulated) {
            return index;
        }
    }

    return getFrameCount() - 1;
}

const SpriteAnimation::Frame& SpriteAnimation::getFrame(int index) const {
    if (index < 0 || index >= getFrameCount()) {
        throw std::out_of_range("SpriteAnimation frame index out of range");
    }

    return m_frames[static_cast<size_t>(index)];
}

}  // namespace vde