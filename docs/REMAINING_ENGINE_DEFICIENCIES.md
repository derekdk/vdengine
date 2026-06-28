# Remaining Engine Deficiencies — Ranked by Importance

> **Created:** 2026-04-16
>
> Supplemental to `TOP_FIVE_ENGINE_FEATURES_PLAN.md`. This document catalogs every
> identified gap preventing VDE from being a full-featured 2D game engine, ranked by
> impact. The top five (SpriteSheet, Animation, Particles, Collision Layers, Game UI)
> are tracked in the primary plan and excluded here.

---

## Ranking Criteria

Each item is scored on four axes (1–5 scale):

| Axis | Meaning |
|------|---------|
| **Frequency** | How often a game developer would need this feature |
| **Workaround Cost** | How painful it is to work around the gap (5 = very painful) |
| **Scope of Impact** | How many game genres and use cases benefit |
| **Engine Credibility** | How much this gap undermines perception of engine maturity |

**Priority Score** = average of the four axes, rounded to one decimal.

---

## Tier 1 — High Priority (Score 4.0+)

### 6. TileMap / Tiled Background System
**Score: 4.5** — Frequency 5 · Workaround 5 · Scope 4 · Credibility 4

Every 2D sidescroller, platformer, RPG, and strategy game needs a tilemap. VDE has
no tile grid renderer, no collision generation from tile data, and no format support.
The workaround is manually placing individual sprite entities, which does not scale and
has no culling.

**What's needed:**
- `TileMap` entity with grid dimensions and spritesheet binding
- View-frustum culling (only render visible tiles)
- Collision shape generation from tile types (solid, one-way platform, slope)
- Instanced or batched rendering for performance
- CSV loader (minimum); Tiled `.tmx` JSON support (ideal)
- `RepeatingBackground` entity for infinite scrolling backgrounds with parallax factor

**Status:** Proposed API exists in `OPEN_SUGGESTIONS.md` (#3). Not started.

---

### 7. Advanced Camera2D Constraints and Composition
**Score: 2.6** — Frequency 3 · Workaround 2 · Scope 3 · Credibility 3

VDE now ships the core camera-feel layer on `Camera2D`, including:

- `followTarget()` for per-frame target following
- `setDeadzone()` for jitter-resistant follow windows
- `setLookAhead()` for velocity-based lead
- `shake()` for time-limited decaying shake
- `zoomTo()` for smooth zoom interpolation

The remaining gap is the next layer above that core API:

- **Bounds / confinement**: Constrain camera travel to a level rectangle or track
- **Camera rails / authored paths**: Follow gameplay while staying on a designed route
- **Higher-level composition presets**: Separate horizontal/vertical tuning, easing presets, or anchor-based framing helpers

**What's still needed:**
```
Camera2D::setBounds(float minX, float minY, float maxX, float maxY)
Camera2D::setBoundsEnabled(bool enabled)
Camera2D::setFollowAxes(bool horizontal, bool vertical)
```

**Status:** Core feel helpers shipped and demonstrated in `camera_feel_demo`; advanced constraints/composition are still open.

---

### 8. Advanced Input Semantics / Rebinding UX
**Score: 3.1** — Frequency 4 · Workaround 2 · Scope 4 · Credibility 3

VDE now has a shipped input action layer via `InputActionMap`, including:

- Named actions ("Jump", "Attack", "MoveLeft") bound to one or more physical inputs
- Keyboard, gamepad button, and thresholded gamepad-axis bindings
- Action states: pressed, held, released
- Serialization to/from `StorageManager` for saved bindings

The remaining gap is the advanced workflow around that layer:

- Rebindable runtime UI for settings menus
- Per-action analog/deadzone tuning workflows beyond thresholded axes
- Input buffering for fighting game inputs (e.g., queue a punch during hitstun)
- Higher-level intent helpers for analog movement/look actions

**What's still needed:**
- Built-in rebinding/prompt flow for changing bindings at runtime
- Optional buffered-input window API on top of action states
- Additional docs/examples for migrating older raw-input examples

**Status:** Partially addressed. Core action mapping shipped; advanced semantics remain.

---

### 9. Spatial Partitioning for Physics Broadphase
**Score: 4.0** — Frequency 3 · Workaround 4 · Scope 4 · Credibility 5

VDE's physics broadphase is O(n²) all-pairs. This works for small entity counts but
breaks down at 100+ dynamic bodies. Any game with large numbers of projectiles, enemies,
or destructible objects will hit performance walls.

**What's needed:**
- Uniform grid or quadtree spatial partition
- Automatic re-bucketing on body movement
- Broadphase pair pruning before narrow-phase AABB/circle tests
- Configurable cell size / tree depth

**Status:** Noted in `OPEN_SUGGESTIONS.md` (#8) as part of circle collider. Not started.

---

## Tier 2 — Medium Priority (Score 3.0–3.9)

### 10. Audio Effects & Spatial Audio
**Score: 3.8** — Frequency 4 · Workaround 3 · Scope 4 · Credibility 4

VDE audio is functional but basic. Games expect:

- **Reverb/echo** for environment (cave, arena, outdoor)
- **Pitch shift** for slow-motion effects
- **Low-pass filter** for muffled/underwater audio
- **Spatial falloff** with distance attenuation curves
- **Audio bus routing** (separate bus for UI sounds vs world sounds)

miniaudio supports DSP node graphs, so this is achievable within the existing backend.

**Status:** Not tracked. New suggestion.

---

### 11. Scene Serialization & Level Loading
**Score: 3.8** — Frequency 4 · Workaround 4 · Scope 4 · Credibility 3

VDE scenes are constructed entirely in C++ code. There is no data-driven scene format.
This means:

- Level designers must be C++ programmers
- Every level change requires recompilation
- No save/load of scene state (only key-value StorageManager)

**What's needed:**
- JSON or binary scene format describing entities, transforms, physics bodies, and properties
- `Scene::saveToFile()` / `Scene::loadFromFile()` serialization
- Support for custom entity properties via a property registration system
- Editor integration (resource_editor could export scenes)

**Status:** Not tracked. New suggestion.

---

### 12. Finite State Machine Utility
**Score: 3.5** — Frequency 4 · Workaround 3 · Scope 4 · Credibility 3

Fighting games, AI, animation controllers, and menu systems all use state machines.
VDE has none. Every example that needs state tracking uses ad-hoc enums and switch
statements.

**What's needed:**
- `StateMachine<StateEnum>` template with `onEnter`/`onUpdate`/`onExit` per state
- Transition conditions (immediate, delayed, conditional)
- Optional: hierarchical states (substates)
- Not a heavy framework — a lightweight utility class

**Status:** Not tracked. New suggestion.

---

### 13. Post-Processing Pipeline
**Score: 3.5** — Frequency 3 · Workaround 4 · Scope 4 · Credibility 3

VDE has `OffscreenRenderTarget` but no post-processing chain. Common effects are missing:

- **Bloom** for emissive/bright areas
- **Chromatic aberration** for impact effects
- **Vignette** for cinematic framing
- **Color grading / LUT** for art direction
- **Hit-pause / frame-freeze** effect (render last frame for N frames)

**What's needed:**
- `PostProcessChain` that renders the scene to an offscreen target, then applies
  a sequence of fullscreen fragment shaders
- Built-in effects as shader presets
- Per-effect enable/disable and parameter control
- Integration with scene rendering pipeline

**Status:** Not tracked. New suggestion.

---

### 14. Per-Scene Input Blocking
**Score: 3.3** — Frequency 4 · Workaround 2 · Scope 4 · Credibility 3

When a pause menu or dialog is pushed as an overlay scene, the underlying gameplay
scene still receives input events. `setFocusedScene()` exists but there is no
`setBlocksInput()` propagation control.

**What's needed:**
- `Scene::setBlocksInput(bool)` — when true, scenes below in the stack don't receive input
- Default behavior should be backward-compatible (no blocking)

**Status:** Noted in `OPEN_SUGGESTIONS.md` (#7). Partial.

---

### 15. Scripting Language Integration
**Score: 3.3** — Frequency 3 · Workaround 3 · Scope 4 · Credibility 3

All game logic is C++. This means:

- Iteration speed is slow (recompile for every tweak)
- Non-programmers cannot modify game behavior
- Modding is impossible

**What's needed:**
- Lua binding (lightweight, proven in games) for entity behavior, scene logic, and events
- `ScriptComponent` that attaches a Lua file to an entity
- Access to core API from Lua: entity transforms, physics, audio, input
- Hot-reload of scripts during development

**Status:** Not tracked. New suggestion. Low ROI for engine's current audience (C++ developers).

---

### 16. Tween / Easing Library
**Score: 3.3** — Frequency 4 · Workaround 2 · Scope 4 · Credibility 3

UI animations, camera movements, entity interpolation, and juice effects all need easing
functions. VDE has `Cooldown` (linear progress) but no easing curves.

**What's needed:**
- `Tween<T>` template that interpolates a value from A to B over duration with an easing curve
- Standard easing functions: linear, ease-in/out quad/cubic/elastic/bounce
- Chainable: `tween.then(...)` for sequences
- Callback on completion
- Works with `float`, `glm::vec2`, `glm::vec3`, `Color`

**Status:** Not tracked. New suggestion.

---

### 17. Debug Drawing / Gizmo System
**Score: 3.0** — Frequency 3 · Workaround 3 · Scope 3 · Credibility 3

Debugging physics, hitboxes, and spatial queries requires visualizing wireframe shapes.
VDE has no immediate-mode debug draw.

**What's needed:**
- `DebugDraw::line(a, b, color)`
- `DebugDraw::rect(pos, size, color)`
- `DebugDraw::circle(pos, radius, color)`
- `DebugDraw::text(pos, string, color)` (screen-space label)
- Rendered as an overlay, cleared each frame
- Toggle with a single flag (`game.setDebugDraw(true)`)
- Physics bodies optionally render their collision shapes

**Status:** Not tracked. New suggestion.

---

### 18. Scene Re-entry Optimization
**Score: 3.0** — Frequency 3 · Workaround 2 · Scope 3 · Credibility 4

`onEnter()`/`onExit()` fire on every scene switch. Scenes with heavy setup use a manual
`m_initialized` flag. The engine should provide `onFirstEnter()` / `onReEnter()` hooks.

**Status:** Noted in `OPEN_SUGGESTIONS.md` (#6). Not started.

---

## Tier 3 — Lower Priority (Score 2.0–2.9)

### 19. Networking / Multiplayer
**Score: 2.8** — Frequency 2 · Workaround 3 · Scope 3 · Credibility 3

VDE is entirely single-player. For a fighting game, rollback netcode is the gold standard.
This is a massive undertaking and probably out of scope for a lightweight engine, but basic
peer-to-peer or client-server primitives would expand the engine's reach significantly.

**What's needed (minimum viable):**
- UDP socket abstraction
- Lobby / connection management
- State synchronization primitives
- Input serialization for rollback

**Status:** Not tracked. Major scope — likely a separate project or plugin.

---

### 20. Viewport Decorations
**Score: 2.5** — Frequency 2 · Workaround 2 · Scope 2 · Credibility 4

No mechanism for viewport borders or focus indicators between scenes in split-screen.
Current workaround uses border sprites.

**Status:** Noted in `OPEN_SUGGESTIONS.md` (#10). Not started.

---

### 21. CMake `find_package` Support
**Score: 2.5** — Frequency 2 · Workaround 2 · Scope 3 · Credibility 3

`find_package(vde)` is not functional. Consumers must use `add_subdirectory()`.

**What's needed:**
- Generate and install `vdeConfig.cmake`
- Export transitive dependencies
- Version the package

**Status:** Noted in `OPEN_SUGGESTIONS.md` (#12). Not started.

---

### 22. 3D Physics / Rigid Body Dynamics
**Score: 2.5** — Frequency 2 · Workaround 2 · Scope 2 · Credibility 4

VDE's physics is 2D-only. 3D games currently have no physics support. For the engine's
stated focus on 2D this is lower priority, but it limits 3D use cases.

**What's needed:**
- 3D broadphase (AABB tree or spatial hash)
- 3D narrow-phase (sphere, box, capsule)
- 3D impulse resolution and constraint solver
- Or: integrate Jolt/Bullet as a backend

**Status:** Not tracked. Major scope.

---

### 23. Skeletal / Bone Animation
**Score: 2.3** — Frequency 2 · Workaround 3 · Scope 2 · Credibility 2

VDE has no skeletal animation. This is mainly needed for 3D, but some 2D games use
bone-based animation (Spine, DragonBones). Lower priority given the engine's sprite focus.

**Status:** Not tracked. Would likely integrate Spine runtime or similar.

---

### 24. Rich Text Rendering
**Score: 2.3** — Frequency 3 · Workaround 2 · Scope 2 · Credibility 2

`TextEntity` renders single-style text. Games often need inline color changes, size
changes, or icons within text (damage numbers, chat, dialog).

**What's needed:**
- Markup syntax for inline style changes: `"Deal <color=red>50</color> damage!"`
- Icon insertion within text flow
- Text effects (shake, wave, typewriter reveal)

**Status:** Not tracked. New suggestion.

---

### 25. Asset Hot-Reload
**Score: 2.0** — Frequency 2 · Workaround 1 · Scope 3 · Credibility 3

`ShaderCache` supports hot-reload, but textures, meshes, audio, and fonts do not.
Changes to art assets require restarting the application.

**What's needed:**
- File watcher on asset directories
- `ResourceManager` invalidation and reload path
- Notification to entities using reloaded resources

**Status:** Not tracked. ShaderCache provides the pattern to follow.

---

## Summary Table

| Rank | Feature | Score | Tier | Status |
|------|---------|-------|------|--------|
| 1 | SpriteSheet & Sprite Flipping | — | **Top 5** | Planned |
| 2 | Sprite Animation System | — | **Top 5** | Planned |
| 3 | 2D Particle System | — | **Top 5** | Planned |
| 4 | Collision Layers & Filtering | — | **Top 5** | Planned |
| 5 | Game UI System (HUD) | — | **Top 5** | Planned |
| 6 | TileMap / Tiled Background | 4.5 | High | `OPEN_SUGGESTIONS.md` #3 |
| 7 | Camera2D Deadzone & Shake | 4.3 | High | `OPEN_SUGGESTIONS.md` #9 |
| 8 | Input Action Mapping | 4.0 | High | New |
| 9 | Spatial Partitioning | 4.0 | High | `OPEN_SUGGESTIONS.md` #8 |
| 10 | Audio Effects & Spatial | 3.8 | Medium | New |
| 11 | Scene Serialization | 3.8 | Medium | New |
| 12 | Finite State Machine | 3.5 | Medium | New |
| 13 | Post-Processing Pipeline | 3.5 | Medium | New |
| 14 | Per-Scene Input Blocking | 3.3 | Medium | `OPEN_SUGGESTIONS.md` #7 |
| 15 | Scripting Language | 3.3 | Medium | New |
| 16 | Tween / Easing Library | 3.3 | Medium | New |
| 17 | Debug Drawing / Gizmos | 3.0 | Medium | New |
| 18 | Scene Re-entry Optimization | 3.0 | Medium | `OPEN_SUGGESTIONS.md` #6 |
| 19 | Networking / Multiplayer | 2.8 | Lower | New |
| 20 | Viewport Decorations | 2.5 | Lower | `OPEN_SUGGESTIONS.md` #10 |
| 21 | CMake `find_package` | 2.5 | Lower | `OPEN_SUGGESTIONS.md` #12 |
| 22 | 3D Physics | 2.5 | Lower | New |
| 23 | Skeletal Animation | 2.3 | Lower | New |
| 24 | Rich Text Rendering | 2.3 | Lower | New |
| 25 | Asset Hot-Reload | 2.0 | Lower | New |

---

## Recommended Implementation Waves

### Wave 1 (Top 5 Plan)
Features 1–5 as defined in `TOP_FIVE_ENGINE_FEATURES_PLAN.md`.

### Wave 2 (2D Game Completeness)
Features 6–9: TileMap, Camera2D enhancements, Input Mapping, Spatial Partitioning.
After this wave, VDE can ship a complete 2D platformer or fighter.

### Wave 3 (Polish & Juice)
Features 10, 12, 13, 16, 17: Audio effects, State Machines, Post-Processing, Tweens, Debug Draw.
After this wave, games built with VDE feel professional.

### Wave 4 (Ecosystem)
Features 11, 14, 15, 18, 21, 25: Serialization, Input Blocking, Scripting, Scene Re-entry, CMake packaging, Hot-Reload.
After this wave, VDE supports a production workflow.

### Wave 5 (Expansion)
Features 19, 20, 22–24: Networking, Viewport Decorations, 3D Physics, Skeletal Animation, Rich Text.
Niche or high-effort features that expand the engine beyond its core 2D focus.
