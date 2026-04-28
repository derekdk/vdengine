/**
 * @file PlaybackClock.cpp
 * @brief PlaybackClock implementation
 */

#include <vde/PlaybackClock.h>

#include <algorithm>

namespace vde {

void PlaybackClock::setDuration(float seconds) {
    m_duration = std::max(seconds, 0.0001f);
}

void PlaybackClock::setDelay(float seconds) {
    m_delay = std::max(seconds, 0.0f);
}

void PlaybackClock::setSpeed(float speed) {
    m_speed = std::max(speed, 0.0001f);
}

void PlaybackClock::setLoopMode(LoopMode mode) {
    m_loopMode = mode;
}

void PlaybackClock::setOnComplete(std::function<void()> callback) {
    m_onComplete = std::move(callback);
}

void PlaybackClock::start() {
    m_elapsed = 0.0f;
    m_delayElapsed = 0.0f;
    m_progress = 0.0f;
    m_cycleIndex = 0;
    m_active = true;
    m_paused = false;
    m_complete = false;
    m_forward = true;
    m_delayConsumed = (m_delay <= 0.0f);
    m_completionFired = false;
}

void PlaybackClock::pause() {
    m_paused = true;
}

void PlaybackClock::resume() {
    m_paused = false;
}

void PlaybackClock::stop() {
    m_active = false;
    m_paused = false;
    m_complete = false;
    m_elapsed = 0.0f;
    m_delayElapsed = 0.0f;
    m_progress = 0.0f;
    m_cycleIndex = 0;
    m_forward = true;
    m_delayConsumed = false;
    m_completionFired = false;
}

bool PlaybackClock::tick(float deltaTime) {
    if (!m_active || m_complete || deltaTime <= 0.0f) {
        return false;
    }
    if (m_paused) {
        return false;
    }

    float dt = deltaTime * m_speed;

    // Consume the initial delay window first.
    if (!m_delayConsumed) {
        m_delayElapsed += dt;
        if (m_delayElapsed < m_delay) {
            m_progress = 0.0f;
            return false;
        }
        // Spill the time that exceeded the delay into the animation budget.
        dt = m_delayElapsed - m_delay;
        m_delayConsumed = true;
    }

    m_elapsed += dt;

    switch (m_loopMode) {
    case LoopMode::Once: {
        if (m_elapsed >= m_duration) {
            m_elapsed = m_duration;
            m_progress = 1.0f;
            m_complete = true;
            m_active = false;
            if (!m_completionFired && m_onComplete) {
                m_completionFired = true;
                m_onComplete();
            }
            return true;
        }
        m_progress = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 1.0f;
        break;
    }

    case LoopMode::Loop: {
        while (m_elapsed >= m_duration) {
            m_elapsed -= m_duration;
            ++m_cycleIndex;
        }
        m_progress = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 0.0f;
        break;
    }

    case LoopMode::PingPong: {
        while (m_elapsed >= m_duration) {
            m_elapsed -= m_duration;
            ++m_cycleIndex;
            m_forward = !m_forward;
        }
        float rawProgress = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 0.0f;
        m_progress = m_forward ? rawProgress : (1.0f - rawProgress);
        break;
    }
    }

    return false;
}

}  // namespace vde
