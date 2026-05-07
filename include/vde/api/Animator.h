#pragma once

/**
 * @file Animator.h
 * @brief Scene-owned animation service.
 *
 * Animator manages multiple concurrent animations for one scene.
 * It is owned by Scene and ticked automatically in the Visual scheduler
 * phase, immediately after updateVisuals().
 *
 * ### Binding styles
 * Animations can target:
 * - entities (resolved via Scene::getEntity() each frame)
 * - weak_ptr objects (resolved via lock() each frame)
 * - resolver callables (escape hatch for cameras, lights, and other non-entity objects)
 *
 * Raw pointers are never stored.  A target is resolved at the moment it is
 * needed and is never kept across frames.
 *
 * ### Lifetime rules
 * - When a scene exits or is destroyed, all its animations are destroyed.
 * - onComplete is NOT fired for animations cancelled by scene teardown.
 * - Cancelling a handle from inside its own callback is safe.
 * - Scheduling a new animation from inside onComplete is safe.
 *
 * ### Usage
 * @code
 * // Inside a scene:
 * animations().schedule(
 *     AnimationBinding<MeshEntity>::entity(cubeId),
 *     { .duration = 1.0f, .easing = AnimationEasing::EaseOutCubic },
 *     {
 *         .onUpdate = [](MeshEntity& e, const AnimationContext& ctx) {
 *             e.setPosition(0.0f, ctx.easedProgress * 3.0f, 0.0f);
 *         },
 *     });
 * @endcode
 */

#include <vde/Easing.h>
#include <vde/api/GameTypes.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

class Scene;

// ---------------------------------------------------------------------------
// Playback options
// ---------------------------------------------------------------------------

/**
 * @brief Playback loop mode for an animation.
 */
enum class AnimationPlayback : uint8_t {
    Once,     ///< Play once and stop.
    Loop,     ///< Restart at the beginning after each cycle.
    PingPong  ///< Reverse direction after each pass (0→1 then 1→0 alternating).
};

/**
 * @brief Options controlling how an animation plays.
 */
struct AnimationOptions {
    float duration = 1.0f;  ///< Playback duration in seconds (> 0).
    float delay = 0.0f;     ///< Initial delay before playback starts.
    float speed = 1.0f;     ///< Speed multiplier (must be > 0).
    AnimationPlayback playback = AnimationPlayback::Once;
    AnimationEasing easing = AnimationEasing::EaseOutCubic;
    bool startPaused = false;                  ///< Start in paused state.
    std::function<float(float)> customEasing;  ///< Custom easing; used when easing == Custom.
};

// ---------------------------------------------------------------------------
// Per-frame context passed to callbacks
// ---------------------------------------------------------------------------

/**
 * @brief Per-frame snapshot passed to animation callbacks.
 *
 * @note `cycleIndex` semantics differ slightly by playback mode:
 *   - **Loop**: increments by 1 each time the animation wraps to the start
 *     (i.e., increments equal completed full cycles).
 *   - **PingPong**: increments by 1 each time a pass (forward *or* reverse)
 *     completes.  A full round trip (0→1→0) therefore increments cycleIndex
 *     by 2.  Use `reversePass` to distinguish which half of the cycle is active.
 */
struct AnimationContext {
    float deltaTime = 0.0f;       ///< Frame delta applied this tick.
    float elapsed = 0.0f;         ///< Total elapsed time since playback started (excl. delay).
    float duration = 0.0f;        ///< Total duration of the animation.
    float linearProgress = 0.0f;  ///< Raw [0, 1] progress before easing.
    float easedProgress = 0.0f;   ///< Progress after easing evaluation.
    uint32_t cycleIndex = 0;      ///< Number of completed cycles.
    bool reversePass = false;     ///< True during the reverse pass of a PingPong animation.
};

// ---------------------------------------------------------------------------
// Handle
// ---------------------------------------------------------------------------

/**
 * @brief Opaque handle to a running animation.
 *
 * Use the handle to pause, resume, cancel, or adjust the speed of a
 * running animation.  Once an animation is cancelled or naturally
 * completes (Once mode), the handle becomes invalid.
 */
using AnimationId = uint64_t;

/**
 * @brief Sentinel value for an invalid animation ID.
 */
constexpr AnimationId INVALID_ANIMATION_ID = 0;

class Animator;

struct AnimatorHandleControl {
    Animator* animator = nullptr;
};

/**
 * @brief Lightweight handle to a single running animation.
 *
 * Copying an AnimationHandle is safe; all copies refer to the same animation.
 * The handle becomes invalid after cancel() or after the animation finishes
 * naturally (Once mode) or the owning Animator is destroyed.
 */
class AnimationHandle {
  public:
    AnimationHandle() = default;

    /**
     * @brief Returns true if this handle still refers to a live animation.
     */
    bool isValid() const;

    /**
     * @brief Returns true if the animation is still running (not cancelled or complete).
     */
    bool isActive() const;

    /**
     * @brief Cancel the animation.  onComplete will not be called.
     */
    void cancel();

    /**
     * @brief Pause the animation.  Progress stops advancing until resume().
     */
    void pause();

    /**
     * @brief Resume a paused animation.
     */
    void resume();

    /**
     * @brief Set the playback speed multiplier (must be > 0).
     */
    void setSpeed(float speed);

    /**
     * @brief Get the current playback speed multiplier.
     */
    float getSpeed() const;

    AnimationId getId() const { return m_id; }

  private:
    friend class Animator;

    AnimationHandle(AnimationId id, std::weak_ptr<AnimatorHandleControl> handleControl)
        : m_id(id), m_handleControl(std::move(handleControl)) {}

    Animator* resolveAnimator() const;

    AnimationId m_id = INVALID_ANIMATION_ID;
    std::weak_ptr<AnimatorHandleControl> m_handleControl;
};

// ---------------------------------------------------------------------------
// Target binding
// ---------------------------------------------------------------------------

/**
 * @brief Binding kind (entity, weak_ptr, or resolver callable).
 */
enum class AnimationBindingKind : uint8_t { None, Entity, Weak, Resolver };

/**
 * @brief Safe binding from an animation to its target object.
 *
 * Three factory functions create the three supported binding styles.
 * The animation resolves the target each frame through the binding;
 * if resolution fails, the animation is cancelled silently.
 *
 * @tparam T The concrete type of the animation target.
 */
template <typename T>
class AnimationBinding {
  public:
    /**
     * @brief Bind to a scene entity by ID.  Resolved via Scene::getEntity().
     */
    static AnimationBinding<T> entity(EntityId id) {
        AnimationBinding<T> b;
        b.m_kind = AnimationBindingKind::Entity;
        b.m_entityId = id;
        return b;
    }

    /**
     * @brief Bind to a shared object via weak_ptr.  Resolved via lock().
     */
    static AnimationBinding<T> weak(std::weak_ptr<T> obj) {
        AnimationBinding<T> b;
        b.m_kind = AnimationBindingKind::Weak;
        b.m_weak = std::move(obj);
        return b;
    }

    /**
     * @brief Bind to any object via a resolver callable.
     *
     * The resolver is called each frame and must return a raw pointer or nullptr.
     * Use this for cameras, lights, and other scene-owned non-entity objects.
     */
    static AnimationBinding<T> resolver(std::function<T*()> resolve) {
        AnimationBinding<T> b;
        b.m_kind = AnimationBindingKind::Resolver;
        b.m_resolver = std::move(resolve);
        return b;
    }

    /**
     * @brief Resolve the target.  Returns nullptr if the target is no longer available.
     */
    T* resolve(Scene& scene) const;

    /**
     * @brief For Weak bindings, lock the underlying weak_ptr and return the shared_ptr.
     *
     * The caller should hold the returned shared_ptr for the duration of any
     * callback that uses the raw target pointer, preventing the target from
     * being destroyed mid-callback.
     *
     * Returns an empty shared_ptr for Entity, Resolver, and None bindings.
     */
    std::shared_ptr<T> lockWeak() const {
        if (m_kind != AnimationBindingKind::Weak)
            return {};
        return m_weak.lock();
    }

    AnimationBindingKind kind() const { return m_kind; }

  private:
    AnimationBindingKind m_kind = AnimationBindingKind::None;
    EntityId m_entityId = INVALID_ENTITY_ID;
    std::weak_ptr<T> m_weak;
    std::function<T*()> m_resolver;
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

/**
 * @brief Unbound callbacks (no target type).
 *
 * Use with the unbound overload of Animator::schedule().
 */
struct AnimationCallbacks {
    std::function<void(const AnimationContext&)> onStart;     ///< Fires once after the delay.
    std::function<void(const AnimationContext&)> onUpdate;    ///< Fires every active frame.
    std::function<void(const AnimationContext&)> onComplete;  ///< Fires once on natural end.
};

/**
 * @brief Bound callbacks that receive a reference to the resolved target.
 *
 * @tparam T The concrete type of the animation target.
 */
template <typename T>
struct BoundAnimationCallbacks {
    std::function<void(T&, const AnimationContext&)> onStart;
    std::function<void(T&, const AnimationContext&)> onUpdate;
    std::function<void(T&, const AnimationContext&)> onComplete;
};

// ---------------------------------------------------------------------------
// Animator
// ---------------------------------------------------------------------------

/**
 * @brief Scene-owned animation service.
 *
 * Owns and ticks active animations for one scene.
 * Accessed via Scene::animations().
 */
class Animator {
  public:
    Animator();
    ~Animator() = default;

    // Non-copyable; each Scene owns exactly one Animator.
    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;

    Animator(Animator&& other) noexcept;
    Animator& operator=(Animator&& other) noexcept;

    // -----------------------------------------------------------------------
    // Scheduling — unbound (no target)
    // -----------------------------------------------------------------------

    /**
     * @brief Schedule an animation with no bound target.
     *
     * Use this when the callback manages its own target references.
     *
     * @param options   Playback options.
     * @param callbacks Callback set (onStart / onUpdate / onComplete).
     * @return Handle to the running animation.
     */
    AnimationHandle schedule(const AnimationOptions& options, AnimationCallbacks callbacks);

    // -----------------------------------------------------------------------
    // Scheduling — bound to a target
    // -----------------------------------------------------------------------

    /**
     * @brief Schedule a target-bound animation.
     *
     * The animation resolves @p binding each frame.  If the target cannot be
     * resolved, the animation is cancelled silently.
     *
     * @tparam T     Concrete type of the animation target.
     * @param scene     The owning scene (needed for entity resolution).
     * @param binding   How to find the target each frame.
     * @param options   Playback options.
     * @param callbacks Target-aware callback set.
     * @return Handle to the running animation.
     */
    template <typename T>
    AnimationHandle schedule(Scene& scene, const AnimationBinding<T>& binding,
                             const AnimationOptions& options, BoundAnimationCallbacks<T> callbacks);

    // -----------------------------------------------------------------------
    // Scheduling — convenience tween
    // -----------------------------------------------------------------------

    /**
     * @brief Schedule a typed value tween.
     *
     * Interpolates from @p from to @p to over the animation duration and
     * calls @p setter each frame with the current interpolated value.
     *
     * @tparam T     Concrete type of the animation target.
     * @tparam Value Tweenable value type (float, Color, Position, etc.).
     * @param scene   The owning scene.
     * @param binding How to find the target each frame.
     * @param from    Start value.
     * @param to      End value.
     * @param options Playback options.
     * @param setter  Called each frame: void(T& target, const Value& currentValue).
     * @return Handle to the running animation.
     */
    template <typename T, typename Value>
    AnimationHandle tween(Scene& scene, const AnimationBinding<T>& binding, const Value& from,
                          const Value& to, const AnimationOptions& options,
                          std::function<void(T&, const Value&)> setter);

    // -----------------------------------------------------------------------
    // Bulk controls
    // -----------------------------------------------------------------------

    /**
     * @brief Cancel all running animations.  onComplete is not fired.
     */
    void cancelAll();

    /**
     * @brief Pause all running animations.
     */
    void pauseAll();

    /**
     * @brief Resume all paused animations.
     */
    void resumeAll();

    /**
     * @brief Set a global speed multiplier applied on top of per-animation speed.
     */
    void setGlobalSpeed(float speed);

    /**
     * @brief Get the global speed multiplier.
     */
    float getGlobalSpeed() const { return m_globalSpeed; }

    /**
     * @brief Returns the number of currently active animations.
     */
    size_t activeCount() const;

    // -----------------------------------------------------------------------
    // Per-handle controls (called by AnimationHandle)
    // -----------------------------------------------------------------------

    bool isActive(AnimationId id) const;
    void cancel(AnimationId id);
    void pause(AnimationId id);
    void resume(AnimationId id);
    void setSpeed(AnimationId id, float speed);
    float getSpeed(AnimationId id) const;

    // -----------------------------------------------------------------------
    // Engine tick (called by Game::rebuildSchedulerGraph via scene.animations task)
    // -----------------------------------------------------------------------

    /**
     * @brief Advance all active animations by @p deltaTime.
     *
     * Called automatically by the scheduler in the Visual phase, immediately
     * after updateVisuals().  Do not call this manually.
     *
     * @param deltaTime Frame delta in seconds.
     */
    void update(float deltaTime);

  private:
    // -----------------------------------------------------------------------
    // Internal job record
    // -----------------------------------------------------------------------

    struct Job {
        AnimationId id = INVALID_ANIMATION_ID;

        // Playback state
        float elapsed = 0.0f;       ///< Total time elapsed since delay ended.
        float delayElapsed = 0.0f;  ///< Time consumed in the delay window.
        float progress = 0.0f;      ///< Linear [0, 1] progress.
        uint32_t cycleIndex = 0;
        bool forward = true;  ///< PingPong direction flag.
        bool delayDone = false;
        bool started = false;  ///< True once onStart has fired.
        bool active = true;
        bool paused = false;

        // Options (stored for playback)
        float duration = 1.0f;
        float delay = 0.0f;
        float speed = 1.0f;
        AnimationPlayback playback = AnimationPlayback::Once;
        AnimationEasing easing = AnimationEasing::EaseOutCubic;
        std::function<float(float)> customEasing;

        // Callbacks (unbound path)
        std::function<void(const AnimationContext&)> onStart;
        std::function<void(const AnimationContext&)> onUpdate;
        std::function<void(const AnimationContext&)> onComplete;
    };

    AnimationId m_nextId = 1;
    float m_globalSpeed = 1.0f;
    bool m_ticking = false;

    std::shared_ptr<AnimatorHandleControl> m_handleControl;
    std::vector<Job> m_jobs;

    AnimationId allocateId();
    Job* findJob(AnimationId id);
    const Job* findJob(AnimationId id) const;

    void resetHandleControl();

    /// Build an AnimationContext snapshot from a job's current state.
    static AnimationContext makeContext(const Job& job, float deltaTime);

    /// Evaluate the eased progress for a job.
    static float applyEasing(const Job& job, float linear);

    /// Tick a single job.  Returns true if the job completed this tick.
    bool tickJob(Job& job, float dt);

    void compactJobs();
};

}  // namespace vde

// Template implementations that need Scene to be fully defined live in AnimatorImpl.h.
// Scene.h includes AnimatorImpl.h at its bottom, so any translation unit that includes
// Scene.h (directly or via GameAPI.h) automatically gets the implementations.
// Do NOT include AnimatorImpl.h directly.
