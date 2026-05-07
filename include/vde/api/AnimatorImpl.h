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
        auto locked = m_weak.lock();
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
    auto idHolder = std::make_shared<AnimationId>(INVALID_ANIMATION_ID);
    auto handleControl = m_handleControl;

    auto cancelOnMissingTarget = [idHolder, handleControl]() {
        if (*idHolder == INVALID_ANIMATION_ID || !handleControl || !handleControl->animator)
            return;
        handleControl->animator->cancel(*idHolder);
    };

    auto resolveBoundTarget = [&scene, binding,
                               cancelOnMissingTarget]() -> std::pair<std::shared_ptr<T>, T*> {
        std::shared_ptr<T> lockedTarget;
        T* target = nullptr;

        if (binding.kind() == AnimationBindingKind::Weak) {
            lockedTarget = binding.lockWeak();
            target = lockedTarget.get();
        } else {
            target = binding.resolve(scene);
        }

        if (!target) {
            cancelOnMissingTarget();
            return {{}, nullptr};
        }

        return {std::move(lockedTarget), target};
    };

    if (callbacks.onStart) {
        unbound.onStart = [&scene, binding, cb = std::move(callbacks.onStart),
                           resolveBoundTarget](const AnimationContext& ctx) mutable {
            auto [lock, target] = resolveBoundTarget();
            if (target)
                cb(*target, ctx);
        };
    }

    if (callbacks.onUpdate) {
        unbound.onUpdate = [&scene, binding, cb = std::move(callbacks.onUpdate),
                            resolveBoundTarget](const AnimationContext& ctx) mutable {
            auto [lock, target] = resolveBoundTarget();
            if (target)
                cb(*target, ctx);
        };
    }

    if (callbacks.onComplete) {
        unbound.onComplete = [&scene, binding, cb = std::move(callbacks.onComplete),
                              resolveBoundTarget](const AnimationContext& ctx) mutable {
            auto [lock, target] = resolveBoundTarget();
            if (target)
                cb(*target, ctx);
        };
    }

    AnimationHandle handle = schedule(options, std::move(unbound));
    *idHolder = handle.getId();
    return handle;
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
