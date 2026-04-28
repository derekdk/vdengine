#pragma once

/**
 * @file PlaybackClock.h
 * @brief Shared playback state helper for animations and timed sequences.
 *
 * PlaybackClock tracks delay, elapsed time, progress, loop behavior,
 * pause/resume, and speed scaling in one reusable place.
 * It is used by the animation system and any other engine subsystem
 * that needs controllable playback semantics.
 *
 * Does not allocate after construction.
 */

#include <cstdint>
#include <functional>

namespace vde {

/**
 * @brief Loop behavior for a PlaybackClock.
 */
enum class LoopMode : uint8_t {
    Once,     ///< Play once then stop at progress 1.0
    Loop,     ///< Restart at progress 0.0 after each cycle
    PingPong  ///< Reverse direction after each pass (0→1 then 1→0 alternating)
};

/**
 * @brief Shared playback state for animations and timed sequences.
 *
 * Tracks delay, elapsed time, progress, loop mode, pause/resume, and
 * speed scaling.  Does not allocate after construction.
 *
 * @example
 * @code
 * vde::PlaybackClock clock;
 * clock.setDuration(2.0f);
 * clock.setDelay(0.5f);
 * clock.setLoopMode(vde::LoopMode::Loop);
 * clock.start();
 *
 * // Each frame:
 * clock.tick(deltaTime);
 * float t = clock.getProgress();  // 0→1 per cycle
 * @endcode
 */
class PlaybackClock {
  public:
    PlaybackClock() = default;

    /**
     * @brief Set the playback duration in seconds.
     * @param seconds Duration (clamped to a small positive value if <= 0)
     */
    void setDuration(float seconds);

    /**
     * @brief Set an initial delay before playback begins.
     * @param seconds Delay before the animation starts (clamped to >= 0)
     */
    void setDelay(float seconds);

    /**
     * @brief Set the playback speed multiplier.
     * @param speed Must be positive (clamped if <= 0)
     */
    void setSpeed(float speed);

    /**
     * @brief Set loop mode (Once, Loop, PingPong).
     */
    void setLoopMode(LoopMode mode);

    /**
     * @brief Set a callback to invoke when the clock completes (Once mode only).
     *
     * The callback fires exactly once when progress reaches 1.0 in Once mode.
     * It does not fire for Loop or PingPong modes.
     */
    void setOnComplete(std::function<void()> callback);

    /**
     * @brief Start (or restart) playback from the beginning.
     */
    void start();

    /**
     * @brief Pause the clock. Time stops advancing until resume() is called.
     */
    void pause();

    /**
     * @brief Resume the clock after a pause.
     */
    void resume();

    /**
     * @brief Stop and reset the clock to its initial state.
     */
    void stop();

    /**
     * @brief Advance the clock by deltaTime seconds.
     *
     * @param deltaTime Frame delta (must be > 0 to advance)
     * @return true if the clock completed this tick (Once mode only)
     */
    bool tick(float deltaTime);

    /**
     * @brief Get current playback progress in [0, 1].
     *
     * For PingPong mode this reflects the visual direction:
     * 0→1 on the forward pass, 1→0 on the reverse pass.
     */
    float getProgress() const { return m_progress; }

    /**
     * @brief Returns true if the delay window has elapsed and playback has begun.
     */
    bool hasStarted() const { return m_active && m_delayConsumed; }

    /**
     * @brief Returns true if the clock has completed (Once mode after reaching 1.0).
     */
    bool isComplete() const { return m_complete; }

    /**
     * @brief Returns true while the clock is paused.
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief Number of passes completed.
     *
     * In Loop mode this increments each full cycle.
     * In PingPong mode this increments each pass (both forward and reverse).
     */
    uint32_t getCycleIndex() const { return m_cycleIndex; }

    /**
     * @brief Returns true during the reverse pass of a PingPong clock.
     */
    bool isReversePass() const { return !m_forward; }

  private:
    LoopMode m_loopMode = LoopMode::Once;
    std::function<void()> m_onComplete;

    float m_duration = 1.0f;
    float m_delay = 0.0f;
    float m_speed = 1.0f;
    float m_elapsed = 0.0f;       ///< Time elapsed after the delay ends
    float m_delayElapsed = 0.0f;  ///< Time consumed during the delay window
    float m_progress = 0.0f;      ///< Current [0, 1] progress

    uint32_t m_cycleIndex = 0;

    bool m_active = false;
    bool m_paused = false;
    bool m_complete = false;
    bool m_forward = true;           ///< Direction for PingPong mode
    bool m_delayConsumed = false;    ///< True once the initial delay has elapsed
    bool m_completionFired = false;  ///< Guard: completion callback fires only once
};

}  // namespace vde
