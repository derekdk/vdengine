#pragma once

/**
 * @file AnimatorImpl.h
 * @brief Template implementations for Animator that require Scene to be complete.
 *
 * This file is automatically included at the bottom of Scene.h.
 * Do NOT include it directly — include Scene.h (or GameAPI.h) instead.
 */

#include <vde/Tween.h>
#include <vde/api/Animator.h>
#include <vde/api/Entity.h>
#include <vde/api/Scene.h>

namespace vde {

// ---------------------------------------------------------------------------
// AnimationBinding<T>::resolve
// ---------------------------------------------------------------------------

template <typename T>
T* AnimationBinding<T>::resolve(Scene& scene) const {
    switch (m_kind) {
    case AnimationBindingKind::Entity: {
        Entity* e = scene.getEntity(m_entityId);
        return dynamic_cast<T*>(e);
    }
    case AnimationBindingKind::Weak: {
        if (!m_weak)
            return nullptr;
        auto locked = m_weak->lock();
        return locked ? locked.get() : nullptr;
    }
    case AnimationBindingKind::Resolver:
        if (m_resolver)
            return m_resolver();
        return nullptr;
    case AnimationBindingKind::None:
    default:
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Animator::schedule<T> — bound target overload
// ---------------------------------------------------------------------------

template <typename T>
AnimationHandle Animator::schedule(Scene& scene, const AnimationBinding<T>& binding,
                                   const AnimationOptions& options,
                                   BoundAnimationCallbacks<T> callbacks) {
    // Wrap bound callbacks into unbound ones that resolve the target each frame.
    // Cancellation on missing target: if resolution fails, the callback is simply skipped.
    // For Weak bindings, the shared_ptr lock is held for the entire callback invocation
    // so that the target cannot be destroyed mid-callback even if the last external
    // owner releases the shared_ptr during the callback.
    AnimationCallbacks unbound;

    if (callbacks.onStart) {
        unbound.onStart = [&scene, binding,
                           cb = std::move(callbacks.onStart)](const AnimationContext& ctx) mutable {
            auto lock = binding.lockWeak();  // keeps Weak target alive; empty for non-Weak
            T* target = lock ? lock.get() : binding.resolve(scene);
            if (target)
                cb(*target, ctx);
        };
    }

    if (callbacks.onUpdate) {
        unbound.onUpdate = [&scene, binding, cb = std::move(callbacks.onUpdate)](
                               const AnimationContext& ctx) mutable {
            auto lock = binding.lockWeak();  // keeps Weak target alive; empty for non-Weak
            T* target = lock ? lock.get() : binding.resolve(scene);
            if (target)
                cb(*target, ctx);
        };
    }

    if (callbacks.onComplete) {
        unbound.onComplete = [&scene, binding, cb = std::move(callbacks.onComplete)](
                                 const AnimationContext& ctx) mutable {
            auto lock = binding.lockWeak();  // keeps Weak target alive; empty for non-Weak
            T* target = lock ? lock.get() : binding.resolve(scene);
            if (target)
                cb(*target, ctx);
        };
    }

    return schedule(options, std::move(unbound));
}

// ---------------------------------------------------------------------------
// Animator::tween<T, Value>
// ---------------------------------------------------------------------------

template <typename T, typename Value>
AnimationHandle Animator::tween(Scene& scene, const AnimationBinding<T>& binding, const Value& from,
                                const Value& to, const AnimationOptions& options,
                                std::function<void(T&, const Value&)> setter) {
    BoundAnimationCallbacks<T> callbacks;
    callbacks.onUpdate = [from, to, s = std::move(setter)](T& target, const AnimationContext& ctx) {
        s(target, tweenValue<Value>(from, to, ctx.easedProgress));
    };
    return schedule<T>(scene, binding, options, std::move(callbacks));
}

}  // namespace vde
