/**
 * @file Animator.cpp
 * @brief Animator implementation — scene-owned animation service.
 */

#include <vde/api/Animator.h>

#include <algorithm>
#include <cmath>

namespace vde {

// ---------------------------------------------------------------------------
// AnimationHandle
// ---------------------------------------------------------------------------

Animator* AnimationHandle::resolveAnimator() const {
    if (m_id == INVALID_ANIMATION_ID) {
        return nullptr;
    }

    auto handleControl = m_handleControl.lock();
    if (!handleControl) {
        return nullptr;
    }

    return handleControl->animator;
}

bool AnimationHandle::isValid() const {
    return isActive();
}

bool AnimationHandle::isActive() const {
    Animator* animator = resolveAnimator();
    if (!animator) {
        return false;
    }
    return animator->isActive(m_id);
}

void AnimationHandle::cancel() {
    Animator* animator = resolveAnimator();
    if (animator && m_id != INVALID_ANIMATION_ID) {
        animator->cancel(m_id);
        m_id = INVALID_ANIMATION_ID;
        m_handleControl.reset();
    }
}

void AnimationHandle::pause() {
    Animator* animator = resolveAnimator();
    if (animator && m_id != INVALID_ANIMATION_ID) {
        animator->pause(m_id);
    }
}

void AnimationHandle::resume() {
    Animator* animator = resolveAnimator();
    if (animator && m_id != INVALID_ANIMATION_ID) {
        animator->resume(m_id);
    }
}

void AnimationHandle::setSpeed(float speed) {
    Animator* animator = resolveAnimator();
    if (animator && m_id != INVALID_ANIMATION_ID) {
        animator->setSpeed(m_id, speed);
    }
}

float AnimationHandle::getSpeed() const {
    Animator* animator = resolveAnimator();
    if (!animator) {
        return 1.0f;
    }
    return animator->getSpeed(m_id);
}

// ---------------------------------------------------------------------------
// Animator — internal helpers
// ---------------------------------------------------------------------------

Animator::Animator() {
    resetHandleControl();
}

Animator::Animator(Animator&& other) noexcept
    : m_nextId(other.m_nextId), m_globalSpeed(other.m_globalSpeed), m_ticking(other.m_ticking),
      m_handleControl(std::move(other.m_handleControl)), m_jobs(std::move(other.m_jobs)) {
    if (!m_handleControl) {
        resetHandleControl();
    } else {
        m_handleControl->animator = this;
    }

    other.m_nextId = 1;
    other.m_globalSpeed = 1.0f;
    other.m_ticking = false;
    other.m_jobs.clear();
    other.resetHandleControl();
}

Animator& Animator::operator=(Animator&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    m_nextId = other.m_nextId;
    m_globalSpeed = other.m_globalSpeed;
    m_ticking = other.m_ticking;
    m_handleControl = std::move(other.m_handleControl);
    m_jobs = std::move(other.m_jobs);

    if (!m_handleControl) {
        resetHandleControl();
    } else {
        m_handleControl->animator = this;
    }

    other.m_nextId = 1;
    other.m_globalSpeed = 1.0f;
    other.m_ticking = false;
    other.m_jobs.clear();
    other.resetHandleControl();

    return *this;
}

void Animator::resetHandleControl() {
    m_handleControl = std::make_shared<AnimatorHandleControl>();
    m_handleControl->animator = this;
}

AnimationId Animator::allocateId() {
    return m_nextId++;
}

Animator::Job* Animator::findJob(AnimationId id) {
    for (auto& job : m_jobs) {
        if (job.id == id) {
            return &job;
        }
    }
    return nullptr;
}

const Animator::Job* Animator::findJob(AnimationId id) const {
    for (const auto& job : m_jobs) {
        if (job.id == id) {
            return &job;
        }
    }
    return nullptr;
}

AnimationContext Animator::makeContext(const Job& job, float deltaTime) {
    AnimationContext ctx;
    ctx.deltaTime = deltaTime;
    ctx.elapsed = job.elapsed;
    ctx.duration = job.duration;
    ctx.linearProgress = job.progress;
    ctx.easedProgress = applyEasing(job, job.progress);
    ctx.cycleIndex = job.cycleIndex;
    ctx.reversePass = !job.forward;
    return ctx;
}

float Animator::applyEasing(const Job& job, float linear) {
    if (job.easing == AnimationEasing::Custom && job.customEasing) {
        return job.customEasing(linear);
    }
    return evaluateEasing(job.easing, linear);
}

// ---------------------------------------------------------------------------
// Animator::schedule (unbound)
// ---------------------------------------------------------------------------

AnimationHandle Animator::schedule(const AnimationOptions& options, AnimationCallbacks callbacks) {
    Job job;
    job.id = allocateId();
    job.duration = std::max(options.duration, 0.0001f);
    job.delay = std::max(options.delay, 0.0f);
    job.speed = std::max(options.speed, 0.0001f);
    job.playback = options.playback;
    job.easing = options.easing;
    job.customEasing = options.customEasing;
    job.paused = options.startPaused;
    job.active = true;
    job.onStart = std::move(callbacks.onStart);
    job.onUpdate = std::move(callbacks.onUpdate);
    job.onComplete = std::move(callbacks.onComplete);

    m_jobs.push_back(std::move(job));

    return {m_jobs.back().id, m_handleControl};
}

// ---------------------------------------------------------------------------
// Animator — bulk controls
// ---------------------------------------------------------------------------

void Animator::cancelAll() {
    for (auto& job : m_jobs) {
        job.active = false;
    }
    if (!m_ticking) {
        m_jobs.clear();
    }
}

void Animator::pauseAll() {
    for (auto& job : m_jobs) {
        job.paused = true;
    }
}

void Animator::resumeAll() {
    for (auto& job : m_jobs) {
        job.paused = false;
    }
}

void Animator::setGlobalSpeed(float speed) {
    m_globalSpeed = std::max(speed, 0.0001f);
}

size_t Animator::activeCount() const {
    size_t count = 0;
    for (const auto& job : m_jobs) {
        if (job.active) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Animator — per-handle controls
// ---------------------------------------------------------------------------

bool Animator::isActive(AnimationId id) const {
    const Job* job = findJob(id);
    return job && job->active;
}

void Animator::cancel(AnimationId id) {
    Job* job = findJob(id);
    if (job) {
        job->active = false;
        if (!m_ticking) {
            compactJobs();
        }
    }
}

void Animator::pause(AnimationId id) {
    Job* job = findJob(id);
    if (job) {
        job->paused = true;
    }
}

void Animator::resume(AnimationId id) {
    Job* job = findJob(id);
    if (job) {
        job->paused = false;
    }
}

void Animator::setSpeed(AnimationId id, float speed) {
    Job* job = findJob(id);
    if (job) {
        job->speed = std::max(speed, 0.0001f);
    }
}

float Animator::getSpeed(AnimationId id) const {
    const Job* job = findJob(id);
    return job ? job->speed : 1.0f;
}

// ---------------------------------------------------------------------------
// Animator::tickJob — advance one animation by dt
// ---------------------------------------------------------------------------

bool Animator::tickJob(Job& job, float dt) {
    if (!job.active || job.paused) {
        return false;
    }

    // Consume the initial delay window.
    if (!job.delayDone) {
        job.delayElapsed += dt;
        if (job.delayElapsed < job.delay) {
            return false;
        }
        // The overshoot after the delay runs into the first active tick.
        dt = job.delayElapsed - job.delay;
        job.delayDone = true;
        job.delayElapsed = job.delay;
    }

    // Apply speed multipliers only to active playback (delay is wall-clock).
    float playbackDt = dt * m_globalSpeed * job.speed;

    // Fire onStart once.
    if (!job.started) {
        job.started = true;
        if (job.onStart) {
            job.onStart(makeContext(job, playbackDt));
            if (!job.active) {
                return false;
            }
        }
    }

    job.elapsed += playbackDt;

    // Compute progress based on loop mode.
    bool completed = false;

    if (job.playback == AnimationPlayback::Once) {
        float raw = job.elapsed / job.duration;
        if (raw >= 1.0f) {
            raw = 1.0f;
            completed = true;
        }
        job.progress = raw;
    } else if (job.playback == AnimationPlayback::Loop) {
        float raw = job.elapsed / job.duration;
        job.cycleIndex = static_cast<uint32_t>(std::floor(raw));
        job.progress = raw - static_cast<float>(job.cycleIndex);
    } else {
        // PingPong
        float raw = job.elapsed / job.duration;
        job.cycleIndex = static_cast<uint32_t>(std::floor(raw));
        float cycleProgress = raw - static_cast<float>(job.cycleIndex);
        job.forward = (job.cycleIndex % 2u) == 0u;
        job.progress = job.forward ? cycleProgress : (1.0f - cycleProgress);
    }

    // Fire onUpdate.
    if (job.onUpdate) {
        job.onUpdate(makeContext(job, playbackDt));
        if (!job.active) {
            return false;
        }
    }

    // Fire onComplete and deactivate for Once mode.
    if (completed) {
        job.active = false;
        if (job.onComplete) {
            job.onComplete(makeContext(job, playbackDt));
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Animator::update — called by scheduler (Visual phase)
// ---------------------------------------------------------------------------

void Animator::update(float deltaTime) {
    if (deltaTime <= 0.0f || m_jobs.empty()) {
        return;
    }

    m_ticking = true;

    // Iterate by index so that animations scheduled from inside callbacks
    // (appended to m_jobs) are not visited this tick.
    size_t count = m_jobs.size();
    for (size_t i = 0; i < count; ++i) {
        tickJob(m_jobs.at(i), deltaTime);
    }

    m_ticking = false;

    compactJobs();
}

void Animator::compactJobs() {
    std::erase_if(m_jobs, [](const Job& j) { return !j.active; });
}

}  // namespace vde
