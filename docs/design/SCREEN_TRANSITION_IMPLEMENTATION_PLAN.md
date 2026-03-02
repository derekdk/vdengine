# Screen Transition Effects — Implementation Plan

> **Feature Reference:** [OPEN_SUGGESTIONS.md — #5 Scene Transition Effects](../OPEN_SUGGESTIONS.md#5-scene-transition-effects)  
> **Created:** 2026-03-02  
> **Last Updated:** 2026-03-02  
> **Status:** Phase 1 Complete

---

## Progress Summary

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | [Core Types & State Machine](#phase-1-core-types--state-machine) | **Done** |
| 2 | [FADE_BLACK Overlay Rendering](#phase-2-fade_black-overlay-rendering) | Not Started |
| 3 | [Advanced Transitions (CROSSFADE, SLIDE)](#phase-3-advanced-transitions) | Not Started |
| 4 | [Transition Example & Documentation](#phase-4-transition-example--documentation) | Not Started |

---

## Design Decisions

| Decision | Resolution | Rationale |
|---|---|---|
| API surface | Overloaded `setActiveScene()` with transition params | Minimal API change; default param keeps backwards compatibility |
| Transition timing model | Two-phase: fade-out (alpha 0→1) then fade-in (alpha 1→0) | Simple, predictable, works for all transition types |
| State ownership | `TransitionState` struct owned by `Game` | Centralizes transition logic; accessible from render path |
| Overlay alpha model | `overlayAlpha` float (0.0–1.0) exposed as public state | Renderers and game code can use it for custom overlay rendering |
| Scene switch timing | At midpoint (end of fade-out phase) | Screen is fully occluded, hiding the instant scene swap |
| Duration semantics | Total duration (split evenly between fade-out and fade-in) | Intuitive for callers: `0.5f` = half-second total transition |
| Transition rejection | Ignored if a transition is already active | Prevents conflicting state; caller can check `isTransitioning()` |
| Phase 1 visual scope | State machine only; no built-in overlay rendering | Keeps Phase 1 simple and testable; games can render overlays using `getTransitionOverlayAlpha()` |

---

## Scope

**In scope (Phase 1 — this PR):**
- `TransitionType` enum: `NONE`, `FADE_BLACK`, `CROSSFADE`, `SLIDE_LEFT`, `SLIDE_RIGHT`
- `TransitionPhase` enum: `NONE`, `FADING_OUT`, `FADING_IN`
- `TransitionState` struct with state machine logic
- Overloaded `Game::setActiveScene(name, transition, duration)`
- `Game::isTransitioning()`, `getTransitionOverlayAlpha()`, `getTransitionState()`
- Integration into the game loop (`updateTransition()` called each frame)
- Scene switch triggered at transition midpoint via `processPendingSceneChange()`
- Unit tests for the transition state machine
- Implementation plan document

**In scope (Phase 2 — future PR):**
- Built-in full-screen overlay rendering for `FADE_BLACK`
- Full-screen quad pipeline or sprite-based overlay
- Automatic overlay draw during render phase when transition is active

**In scope (Phase 3 — future PR):**
- `CROSSFADE` implementation (requires offscreen render targets)
- `SLIDE_LEFT` / `SLIDE_RIGHT` (requires viewport animation)

**In scope (Phase 4 — future PR):**
- Example demonstrating screen transitions
- API documentation updates
- Smoke test scripts for transition examples

---

## Phase 1: Core Types & State Machine

**Goal:** Establish the transition data model, state machine, and game loop integration.
Games can use `getTransitionOverlayAlpha()` to implement custom visual overlays.

### Files Created

| File | Purpose |
|------|---------|
| `include/vde/api/ScreenTransition.h` | `TransitionType`, `TransitionPhase`, `TransitionState` |
| `src/api/ScreenTransition.cpp` | State machine implementation (`start`, `update`, `reset`) |
| `tests/ScreenTransition_test.cpp` | Unit tests for state machine |

### Files Modified

| File | Changes |
|------|---------|
| `include/vde/api/Game.h` | Added `#include "ScreenTransition.h"`, overloaded `setActiveScene()`, transition query methods, `m_transition` member, `updateTransition()` private method |
| `src/api/Game.cpp` | Implemented overloaded `setActiveScene()`, `updateTransition()`, integrated into game loop |
| `CMakeLists.txt` | Added new source and header to `VDE_SOURCES` and `VDE_PUBLIC_HEADERS` |
| `tests/CMakeLists.txt` | Added `ScreenTransition_test.cpp` to `VDE_TEST_SOURCES` |
| `docs/OPEN_SUGGESTIONS.md` | Updated status from "Not Started" to "In Progress" |

### State Machine

```
                  setActiveScene(name, FADE_BLACK, duration)
                                    │
                                    ▼
                          ┌─────────────────┐
                          │   FADING_OUT     │
                          │ alpha: 0.0 → 1.0│
                          │ elapsed < half   │
                          └────────┬─────────┘
                                   │ elapsed >= halfDuration
                                   │ (midpoint reached)
                                   ▼
                        ┌──────────────────────┐
                        │  Scene Switch         │
                        │  processPending-      │
                        │  SceneChange()        │
                        └──────────┬────────────┘
                                   │
                                   ▼
                          ┌─────────────────┐
                          │   FADING_IN      │
                          │ alpha: 1.0 → 0.0│
                          │ elapsed < half   │
                          └────────┬─────────┘
                                   │ elapsed >= halfDuration
                                   │ (transition complete)
                                   ▼
                          ┌─────────────────┐
                          │     NONE         │
                          │ alpha: 0.0       │
                          │ (idle)           │
                          └─────────────────┘
```

### API Usage (Phase 1)

```cpp
// Instant switch (existing behavior, unchanged)
game.setActiveScene("menu");

// Transition with fade-to-black effect (0.5s total)
game.setActiveScene("gameplay", vde::TransitionType::FADE_BLACK, 0.5f);

// Check transition state
if (game.isTransitioning()) {
    float alpha = game.getTransitionOverlayAlpha();
    // Use alpha to draw a custom overlay (0.0 = transparent, 1.0 = black)
}
```

### Test Cases

| Test | Validates |
|------|-----------|
| `DefaultStateIsInactive` | Default construction yields inactive state |
| `ResetClearsAllFields` | `reset()` returns all fields to default |
| `StartBeginsInFadingOutPhase` | `start()` sets phase, type, target, timing |
| `StartWithZeroDurationClampsHalfDuration` | Avoids division by zero |
| `UpdateIncreasesOverlayAlphaDuringFadeOut` | Alpha increases linearly |
| `UpdateReachesMidpointAtEndOfFadeOut` | Returns true at midpoint |
| `FadeInDecreasesOverlayAlpha` | Alpha decreases after midpoint |
| `FadeInCompletesAndResetsState` | State resets at end |
| `OnCompleteCallbackInvokedAtEnd` | Callback fires on completion |
| `UpdateWithNoActiveTransitionDoesNothing` | No-op when idle |
| `FullTransitionSequence` | End-to-end multi-frame transition |
| `LargeDeltaTimeCompletesTransitionInOneFrame` | Handles large dt |

### Completion Criteria

- [x] `TransitionType` and `TransitionPhase` enums defined
- [x] `TransitionState` struct with `start()`, `update()`, `reset()`
- [x] Overloaded `Game::setActiveScene(name, transition, duration)` added
- [x] `Game::isTransitioning()` and `getTransitionOverlayAlpha()` added
- [x] `updateTransition()` called in game loop
- [x] Scene switch occurs at transition midpoint
- [x] Unit tests cover state machine logic
- [x] CMakeLists.txt updated for new files

---

## Phase 2: FADE_BLACK Overlay Rendering

**Goal:** Add built-in full-screen overlay rendering so that `FADE_BLACK` works
visually without any game-side code.

### Approach

Two options (to be decided during implementation):

**Option A: Sprite-based overlay**
- Create a full-screen `SpriteEntity` with the default white texture
- Set color to black with alpha = `getTransitionOverlayAlpha()`
- Render after all scene content in the render callback
- Pro: Uses existing sprite pipeline; no new Vulkan code
- Con: Requires a dedicated "transition scene" or entity management

**Option B: Dedicated full-screen quad pipeline**
- Simple vertex shader (full-screen triangle, no vertex buffer)
- Fragment shader outputs solid color with configurable alpha
- Rendered as a post-scene overlay in `renderSingleViewport()` / `renderMultiViewport()`
- Pro: Clean, minimal overhead, no entity management
- Con: New Vulkan pipeline to create and manage

**Recommendation:** Option A is simpler for Phase 2. Option B is cleaner
long-term and can be refactored in Phase 3 when CROSSFADE needs offscreen
targets anyway.

### Tasks

- [ ] Create overlay rendering mechanism (Option A or B)
- [ ] Hook into `renderSingleViewport()` to draw overlay after scene content
- [ ] Hook into `renderMultiViewport()` for multi-scene transitions
- [ ] Test visual rendering with an example

---

## Phase 3: Advanced Transitions

**Goal:** Implement CROSSFADE and SLIDE transitions.

### CROSSFADE

Requires rendering both old and new scenes to offscreen framebuffers,
then blending them with the transition alpha as the blend factor.

- [ ] Create offscreen render target infrastructure
- [ ] Render old scene to offscreen target before switch
- [ ] Render new scene to second offscreen target after switch
- [ ] Blend using a full-screen quad with two texture inputs
- [ ] Clean up offscreen targets when transition completes

### SLIDE_LEFT / SLIDE_RIGHT

Requires rendering both scenes with animated viewport offsets.

- [ ] Render old scene with viewport sliding out
- [ ] Render new scene with viewport sliding in
- [ ] Coordinate viewport positions using transition progress

---

## Phase 4: Transition Example & Documentation

**Goal:** Create a demo example and update documentation.

### Tasks

- [ ] Create `examples/transition_demo/` with multiple scenes and transitions
- [ ] Add smoke test script for transition demo
- [ ] Update `API-DOC.md` with screen transition section
- [ ] Update `docs/GETTING_STARTED.md` with transition usage

---

## Implementation Order

```
Phase 1 — Core (this PR)
├── ScreenTransition.h  (types + state machine)
├── ScreenTransition.cpp (implementation)
├── Game.h / Game.cpp   (API + integration)
├── ScreenTransition_test.cpp (unit tests)
└── Implementation plan document

Phase 2 — Visual (separate PR)
├── Overlay rendering (sprite-based or dedicated pipeline)
└── Visual verification

Phase 3 — Advanced (separate PR)
├── CROSSFADE (offscreen render targets)
└── SLIDE_LEFT / SLIDE_RIGHT (viewport animation)

Phase 4 — Polish (separate PR)
├── Transition demo example
└── Documentation updates
```

---

## Effort Estimates

| Phase | Complexity | Estimated Effort |
|-------|------------|------------------|
| Phase 1: Core types & state machine | Low–Medium | 2–3 hours |
| Phase 2: FADE_BLACK overlay | Medium | 3–4 hours |
| Phase 3: CROSSFADE + SLIDE | High | 6–8 hours |
| Phase 4: Example + docs | Low | 2–3 hours |
| **Total** | | **~13–18 hours** |

---

## Dependencies

- No new third-party dependencies
- Uses existing engine infrastructure (Scene, Game, Scheduler)
- Phase 2 uses existing sprite pipeline or adds a minimal new pipeline
- Phase 3 requires new offscreen rendering infrastructure
