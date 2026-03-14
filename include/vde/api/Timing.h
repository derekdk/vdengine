#pragma once

/**
 * @file Timing.h
 * @brief Reusable timing helpers for common gameplay patterns
 *
 * Provides Cooldown and RepeatingTimer classes that replace
 * open-coded accumulator logic in gameplay code.
 */

namespace vde {

/**
 * @brief One-shot cooldown timer.
 *
 * Tracks an elapsed duration and becomes ready when the duration expires.
 * Typical uses: fire rate limiting, invincibility frames, spawn delays.
 */
class Cooldown {
  public:
    explicit Cooldown(float durationSeconds = 0.0f) : m_duration(durationSeconds) {}

    /**
     * @brief Change the cooldown duration. Does not reset elapsed time.
     */
    void setDuration(float durationSeconds) { m_duration = durationSeconds; }

    /**
     * @brief Start the cooldown (resets elapsed to 0).
     */
    void start() { m_elapsed = 0.0f; }

    /**
     * @brief Reset elapsed time to 0 (same as start).
     */
    void reset() { m_elapsed = 0.0f; }

    /**
     * @brief Immediately finish the cooldown (elapsed = duration).
     */
    void finish() { m_elapsed = m_duration; }

    /**
     * @brief Advance time.
     */
    void advance(float deltaTime) {
        m_elapsed += deltaTime;
        if (m_elapsed > m_duration)
            m_elapsed = m_duration;
    }

    /**
     * @brief Check if the cooldown has elapsed.
     */
    bool ready() const { return m_elapsed >= m_duration; }

    /**
     * @brief If ready, consume the cooldown (reset) and return true. Otherwise return false.
     */
    bool tryConsume() {
        if (ready()) {
            reset();
            return true;
        }
        return false;
    }

    /**
     * @brief Time remaining until ready.
     */
    float remaining() const {
        float r = m_duration - m_elapsed;
        return r > 0.0f ? r : 0.0f;
    }

    /**
     * @brief Progress from 0 (just started) to 1 (ready).
     */
    float progress() const {
        if (m_duration <= 0.0f)
            return 1.0f;
        float p = m_elapsed / m_duration;
        return p > 1.0f ? 1.0f : p;
    }

  private:
    float m_duration = 0.0f;
    float m_elapsed = 0.0f;
};

/**
 * @brief Timer that fires repeatedly at a fixed interval.
 *
 * Typical uses: spawn intervals, blink/flash timing, periodic events.
 */
class RepeatingTimer {
  public:
    explicit RepeatingTimer(float intervalSeconds = 0.0f) : m_interval(intervalSeconds) {}

    /**
     * @brief Change the interval. Does not reset accumulated time.
     */
    void setInterval(float intervalSeconds) { m_interval = intervalSeconds; }

    /**
     * @brief Reset accumulated time to 0.
     */
    void reset() { m_accumulated = 0.0f; }

    /**
     * @brief Advance time and return the number of ticks that elapsed.
     *
     * If the interval is very small relative to deltaTime, multiple
     * ticks may fire in a single call.
     */
    int advance(float deltaTime) {
        if (m_interval <= 0.0f)
            return 0;
        m_accumulated += deltaTime;
        if (m_accumulated >= m_interval) {
            const int ticks = static_cast<int>(m_accumulated / m_interval);
            m_accumulated -= m_interval * static_cast<float>(ticks);
            return ticks;
        }
        return 0;
    }

  private:
    float m_interval = 0.0f;
    float m_accumulated = 0.0f;
};

}  // namespace vde
