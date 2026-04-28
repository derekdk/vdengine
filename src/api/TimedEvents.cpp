/**
 * @file TimedEvents.cpp
 * @brief TimedEvents implementation
 */

#include <vde/api/TimedEvents.h>

#include <algorithm>
#include <cmath>

namespace vde {

TimedEventHandle TimedEvents::after(float delay, std::function<void()> callback) {
    float clampedDelay = std::max(delay, 0.0f);
    TimedEventHandle handle = m_nextHandle++;
    m_events.push_back({.handle = handle,
                        .accumulator = 0.0f,
                        .target = clampedDelay,
                        .repeating = false,
                        .active = true,
                        .callback = std::move(callback)});
    return handle;
}

TimedEventHandle TimedEvents::every(float interval, std::function<void()> callback) {
    // Clamp to a small positive minimum to avoid infinite loops on zero intervals.
    float clampedInterval = std::max(interval, 0.0001f);
    TimedEventHandle handle = m_nextHandle++;
    m_events.push_back({.handle = handle,
                        .accumulator = 0.0f,
                        .target = clampedInterval,
                        .repeating = true,
                        .active = true,
                        .callback = std::move(callback)});
    return handle;
}

void TimedEvents::cancel(TimedEventHandle handle) {
    if (handle == INVALID_TIMED_EVENT_HANDLE) {
        return;
    }
    for (auto& entry : m_events) {
        if (entry.handle == handle) {
            entry.active = false;
            break;
        }
    }
}

void TimedEvents::cancelAll() {
    for (auto& entry : m_events) {
        entry.active = false;
    }
    // Compact immediately when not inside tick() to avoid memory retention.
    if (!m_ticking) {
        m_events.clear();
    }
}

void TimedEvents::pause() {
    m_paused = true;
}

void TimedEvents::resume() {
    m_paused = false;
}

void TimedEvents::setSpeed(float speed) {
    m_speed = std::max(speed, 0.0001f);
}

void TimedEvents::tick(float deltaTime) {
    if (m_paused || deltaTime <= 0.0f || m_events.empty()) {
        return;
    }

    float dt = deltaTime * m_speed;

    m_ticking = true;

    // Iterate by index so that events added inside a callback (via after()/every())
    // are appended past the current size and are not visited this tick.
    size_t count = m_events.size();
    for (size_t i = 0; i < count; ++i) {
        EventEntry& entry = m_events.at(i);
        if (!entry.active) {
            continue;
        }

        entry.accumulator += dt;

        if (entry.repeating) {
            // Fire once per elapsed interval without skipping any.
            // Check entry.active each iteration in case the callback cancels itself.
            while (entry.accumulator >= entry.target && entry.active) {
                entry.accumulator -= entry.target;
                entry.callback();
            }
        } else {
            // One-shot: fire once when the delay elapses.
            if (entry.accumulator >= entry.target) {
                entry.active = false;
                entry.callback();
            }
        }
    }

    m_ticking = false;

    // Compact: remove all inactive entries (cancelled or completed one-shots).
    std::erase_if(m_events, [](const EventEntry& e) { return !e.active; });
}

}  // namespace vde
