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

bool AnimationHandle::isActive() const {
    if (!m_animator || m_id == INVALID_ANIMATION_ID)
        return false;
    return m_animator->isActive(m_id);
}

void AnimationHandle::cancel() {
    if (m_animator && m_id != INVALID_ANIMATION_ID) {
        m_animator->cancel(m_id);
        m_id = INVALID_ANIMATION_ID;
    }
}

void AnimationHandle::pause() {
    if (m_animator && m_id != INVALID_ANIMATION_ID)
        m_animator->pause(m_id);
}

void AnimationHandle::resume() {
    if (m_animator && m_id != INVALID_ANIMATION_ID)
        m_animator->resume(m_id);
}

void AnimationHandle::setSpeed(float speed) {
    if (m_animator && m_id != INVALID_ANIMATION_ID)
        m_animator->setSpeed(m_id, speed);
}

float AnimationHandle::getSpeed() const {
    if (!m_animator || m_id == INVALID_ANIMATION_ID)
        return 1.0f;
    return m_animator->getSpeed(m_id);
}

// ---------------------------------------------------------------------------
// Animator — internal helpers
// ---------------------------------------------------------------------------

AnimationId Animator::allocateId() {
    return m_nextId++;
}

Animator::Job* Animator::findJob(AnimationId id) {
    for (auto& job : m_jobs) {
        if (job.id == id)
            return &job;
    }
    return nullptr;
}

const Animator::Job* Animator::findJob(AnimationId id) const {
    for (const auto& job : m_jobs) {
        if (job.id == id)
            return &job;
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

    AnimationId id = job.id;
    m_jobs.push_back(std::move(job));

    return AnimationHandle(id, this);
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
        if (job.active)
            ++count;
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
        if (!m_ticking)
            compactJobs();
    }
}

void Animator::pause(AnimationId id) {
    Job* job = findJob(id);
    if (job)
        job->paused = true;
}

void Animator::resume(AnimationId id) {
    Job* job = findJob(id);
    if (job)
        job->paused = false;
}

void Animator::setSpeed(AnimationId id, float speed) {
    Job* job = findJob(id);
    if (job)
        job->speed = std::max(speed, 0.0001f);
}

float Animator::getSpeed(AnimationId id) const {
    const Job* job = findJob(id);
    return job ? job->speed : 1.0f;
}

// ---------------------------------------------------------------------------
// Animator::tickJob — advance one animation by dt
// ---------------------------------------------------------------------------

bool Animator::tickJob(Job& job, float dt) {
    if (!job.active || job.paused)
        return false;

    // Apply per-job speed multiplier.
    dt *= job.speed;

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

    // Fire onStart once.
    if (!job.started) {
        job.started = true;
        if (job.onStart) {
            job.onStart(makeContext(job, dt));
        }
    }

    job.elapsed += dt;

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
        // Count completed cycles.
        if (raw >= 1.0f) {
            uint32_t newCycles = static_cast<uint32_t>(std::floor(raw));
            job.cycleIndex += newCycles;
            // Wrap elapsed into the current cycle.
            job.elapsed = std::fmod(job.elapsed, job.duration);
            raw = job.elapsed / job.duration;
        }
        job.progress = raw;
    } else {
        // PingPong
        float raw = job.elapsed / job.duration;
        // Each pass (forward or reverse) is one duration.
        if (raw >= 1.0f) {
            job.cycleIndex += static_cast<uint32_t>(std::floor(raw));
            job.elapsed = std::fmod(job.elapsed, job.duration);
            raw = job.elapsed / job.duration;
            // Flip direction each pass.
            if (job.cycleIndex % 2 == 1) {
                job.forward = false;
            } else {
                job.forward = true;
            }
        }
        job.progress = job.forward ? raw : (1.0f - raw);
    }

    // Fire onUpdate.
    if (job.onUpdate) {
        job.onUpdate(makeContext(job, dt));
    }

    // Fire onComplete and deactivate for Once mode.
    if (completed) {
        job.active = false;
        if (job.onComplete) {
            job.onComplete(makeContext(job, dt));
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Animator::update — called by scheduler (Visual phase)
// ---------------------------------------------------------------------------

void Animator::update(float deltaTime) {
    if (deltaTime <= 0.0f || m_jobs.empty())
        return;

    float dt = deltaTime * m_globalSpeed;

    m_ticking = true;

    // Iterate by index so that animations scheduled from inside callbacks
    // (appended to m_jobs) are not visited this tick.
    size_t count = m_jobs.size();
    for (size_t i = 0; i < count; ++i) {
        tickJob(m_jobs[i], dt);
    }

    m_ticking = false;

    compactJobs();
}

void Animator::compactJobs() {
    std::erase_if(m_jobs, [](const Job& j) { return !j.active; });
}

}  // namespace vde
