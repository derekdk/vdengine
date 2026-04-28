#pragma once

/**
 * @file TimedEvents.h
 * @brief Scene-owned timed-event service for delayed and repeating callbacks.
 *
 * TimedEvents drives one-shot delayed callbacks and fixed-interval
 * repeating callbacks.  It is owned by Scene and ticked automatically
 * during the Timed scheduler phase.
 *
 * Lifetime:
 * - Events are cancelled automatically when the owning TimedEvents is destroyed.
 * - Cancellation from within a callback is safe.
 *
 * Performance:
 * - No heap allocation occurs per-frame after an event is created.
 */

#include <cstdint>
#include <functional>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Opaque handle to a scheduled timed event.
 *
 * Use this handle to cancel an event before it fires.
 * INVALID_TIMED_EVENT_HANDLE means the handle is not valid.
 */
using TimedEventHandle = uint64_t;

/**
 * @brief Sentinel value for an invalid or cancelled timed event handle.
 */
constexpr TimedEventHandle INVALID_TIMED_EVENT_HANDLE = 0;

/**
 * @brief Scene-owned timed-event service.
 *
 * Provides one-shot delayed callbacks and fixed-interval repeating callbacks.
 * Owned by each Scene and ticked automatically in the Timed scheduler phase.
 *
 * @example
 * @code
 * // In your scene's onEnter():
 * auto& t = getTimedEvents();
 *
 * // One-shot: fire once after 1 second
 * t.after(1.0f, [this]() { spawnEnemy(); });
 *
 * // Repeating: fire every 0.5 seconds
 * m_blinkHandle = t.every(0.5f, [this]() { toggleBlink(); });
 *
 * // Cancel later:
 * t.cancel(m_blinkHandle);
 * @endcode
 */
class TimedEvents {
  public:
    TimedEvents() = default;
    ~TimedEvents() = default;

    // Non-copyable; each Scene owns exactly one instance.
    TimedEvents(const TimedEvents&) = delete;
    TimedEvents& operator=(const TimedEvents&) = delete;

    TimedEvents(TimedEvents&&) = default;
    TimedEvents& operator=(TimedEvents&&) = default;

    /**
     * @brief Schedule a one-shot callback to fire after a delay.
     *
     * The callback fires in the first Timed-phase tick where cumulative
     * scene time has exceeded @p delay.
     *
     * @param delay    Seconds to wait before firing (>= 0; clamped if negative)
     * @param callback Callable to invoke when the delay elapses
     * @return Handle that can be passed to cancel() to suppress the callback
     */
    TimedEventHandle after(float delay, std::function<void()> callback);

    /**
     * @brief Schedule a repeating callback to fire at a fixed interval.
     *
     * The callback fires every @p interval seconds.  A single large
     * deltaTime fires the callback multiple times without skipping any.
     *
     * @param interval Seconds between firings (clamped to a small positive minimum)
     * @param callback Callable to invoke on each interval
     * @return Handle that can be passed to cancel()
     */
    TimedEventHandle every(float interval, std::function<void()> callback);

    /**
     * @brief Cancel a scheduled event.
     *
     * Safe to call from within the event's own callback.
     * Calling with INVALID_TIMED_EVENT_HANDLE or an already-cancelled
     * handle is a no-op.
     *
     * @param handle Handle returned by after() or every()
     */
    void cancel(TimedEventHandle handle);

    /**
     * @brief Cancel all scheduled events.
     *
     * After this call, no pending callbacks will fire.
     * Safe to call from within a callback.
     */
    void cancelAll();

    /**
     * @brief Pause all event timers.
     *
     * While paused, tick() does not advance any accumulators.
     */
    void pause();

    /**
     * @brief Resume all event timers after a pause.
     */
    void resume();

    /**
     * @brief Returns true while event timers are paused.
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief Set a speed multiplier that scales how fast all timers advance.
     *
     * @param speed Must be > 0 (clamped if not). Default is 1.0.
     */
    void setSpeed(float speed);

    /**
     * @brief Get the current speed multiplier.
     */
    float getSpeed() const { return m_speed; }

    /**
     * @brief Advance all timers by deltaTime seconds.
     *
     * Called automatically by the engine in the Timed scheduler phase.
     * @param deltaTime Frame delta in seconds
     */
    void tick(float deltaTime);

  private:
    struct EventEntry {
        TimedEventHandle handle = INVALID_TIMED_EVENT_HANDLE;
        float accumulator = 0.0f;  ///< Time elapsed since creation
        float target = 0.0f;       ///< Delay (one-shot) or interval (repeating)
        bool repeating = false;
        bool active = true;  ///< False when cancelled or completed
        std::function<void()> callback;
    };

    std::vector<EventEntry> m_events;
    TimedEventHandle m_nextHandle = 1;
    bool m_paused = false;
    float m_speed = 1.0f;
    bool m_ticking = false;  ///< True while tick() is iterating; guards cancel() re-entrance
};

}  // namespace vde
