# June Engine Updates Plan

Date: 2026-06-19

## Objective

Implement the missing 2D workflow features highlighted in the engine evaluation so VDE supports a complete 2D gameplay pipeline with fewer per-project workarounds.

Source gaps:

- `TileMap` and tile-layer workflow
- Tiled map import path
- sprite-state wrapper (`AnimatedSpriteEntity`-style workflow)
- input action/binding system for named actions
- Camera2D feel helpers (deadzone, look-ahead, shake)

## Scope Lock (Day 1)

Use existing status/evaluation documents as canonical scope and acceptance framing:

- `docs/ENGINE_API_EVALUATION_2026-05-26.md`
- `docs/PROJECT_STATUS.md`
- `docs/REMAINING_ENGINE_DEFICIENCIES.md`

Definition for each feature to be considered delivered:

- public API shipped
- unit tests added
- at least one runnable example integration
- smoke coverage added
- docs updated

## Delivery Sequence

Order features by dependency and developer impact:

1. Input action mapping and bindings
2. Camera2D feel helpers
3. Animated sprite-state workflow
4. TileMap runtime workflow
5. Tiled import path

This sequence delivers immediate gameplay ergonomics first, then character workflow, then content pipeline.

## Phase A: Input Action/Binding System (Week 1-2)

### Status Snapshot (2026-06-19)

Initial vertical slice delivered:

- `InputActionMap` public API added for named actions with `pressed` / `held` / `released` states
- keyboard, gamepad button, and thresholded gamepad-axis bindings supported
- binding save/load implemented through `StorageManager`
- runnable `input_actions_demo` example added with smoke coverage

Remaining follow-up for this phase after the initial slice:

- built-in rebinding UI workflow
- richer buffered-input semantics
- broader migration of older examples to the new action-map layer

### Goal

Replace repeated per-example key handling with named, rebindable actions.

### Core Deliverables

- Named actions (for example: `Jump`, `Pause`, `Fire`, `MoveLeft`)
- Multiple bindings per action
- Action states: pressed, held, released
- Initial gamepad support with deadzone-aware axes
- Binding persistence via `StorageManager`

### Integration Points

- `include/vde/api/InputHandler.h`
- `include/vde/api/KeyStateTracker.h`
- `docs/API.md`

### Acceptance Criteria

- One existing example migrated from raw key events to action mapping
- Action bindings save/load correctly across restarts
- Keyboard and at least one gamepad path validated

## Phase B: Camera2D Feel Helpers (Week 2-3)

### Status Snapshot (2026-06-19)

Initial vertical slice delivered:

- `Camera2D` now exposes `followTarget()`, `setDeadzone()`, `setLookAhead()`, `shake()`, and `zoomTo()`
- normal scene execution now advances `GameCamera::update()` so camera interpolation and shake run without extra user wiring
- `GameCamera_test.cpp` covers the new Camera2D helper semantics
- runnable `camera_feel_demo` example added with smoke coverage

Remaining follow-up for this phase after the initial slice:

- camera bounds / confinement helpers
- more opinionated composition presets or per-axis tuning helpers
- broader migration of older 2D examples to the new follow helper API

### Goal

Add core camera-feel tools expected for modern 2D gameplay.

### Core Deliverables

- Follow deadzone rectangle
- Direction/velocity look-ahead
- Decaying screen shake with duration
- Smooth zoom target interpolation

### Integration Points

- `include/vde/api/GameCamera.h` (`Camera2D`)

### Acceptance Criteria

- Features are optional and preserve backward compatibility by default
- One demo scene validates each helper in isolation and combined usage

## Phase C: Animated Sprite-State Workflow (Week 3-4)

### Status Snapshot (2026-06-28)

Initial vertical slice delivered:

- `SpriteAnimation` public clip model added for frame sequences with per-frame durations and looping control
- `AnimatedSpriteEntity` public wrapper added with named states, `play()` / `pause()` / `resume()` / `stop()`, playback speed, and frame-event hooks
- focused unit coverage added for looping, one-shot clamping, pause/resume, speed scaling, and frame-event dispatch
- `sidescroller` migrated to a SpriteSheet-backed `idle` / `run` / `jump` / `attack` workflow with no manual UV math in gameplay code
- smoke coverage added for the migrated `sidescroller` example
- transition-condition helpers and optional blend callbacks added for more advanced state graphs
- Aseprite-style JSON import helper added for atlas-backed clip import
- additional legacy example migration completed in `sprite_demo` using imported atlas metadata plus `AnimatedSpriteEntity`

Remaining follow-up for this phase after the initial slice:

- broader migration pass for other SpriteSheet-driven demos that still set frame UVs manually for character-like workflows
- richer data import support beyond the current Aseprite-style JSON subset
- higher-level authoring/editor workflow for animation states if the engine grows a dedicated content pipeline

### Goal

Provide a sprite-focused state animation wrapper so users stop writing manual frame/UV logic.

### Core Deliverables

- `SpriteAnimation` data model
- `AnimatedSpriteEntity` wrapper
- Named animation states and transitions
- Playback controls: play, pause, stop, speed
- Optional frame events for effects/gameplay hooks

### Reference Plan

- `docs/TOP_FIVE_ENGINE_FEATURES_PLAN.md` (animation section)

### Acceptance Criteria

- Character demo with `idle/run/jump/attack` states
- No manual UV math or frame timer logic in the game loop for that demo

## Phase D: TileMap Runtime Workflow (Week 4-6)

### Goal

Enable scalable tile-based level construction and rendering workflows.

### Core Deliverables

- Tile grid and tile-layer API
- Tileset/atlas binding
- Visible-region culling
- Basic collision extraction (minimum: solid + one-way)
- Repeating/parallax background helper

### Reference Inputs

- `docs/REMAINING_ENGINE_DEFICIENCIES.md`
- `docs/OPEN_SUGGESTIONS.md`

### Acceptance Criteria

- One medium-size map scene runs with stable performance using culling
- Collision extraction supports at least solid and one-way platforms

## Phase E: Tiled Import Path (Week 6-7)

### Goal

Ship a practical data import path from Tiled into the runtime TileMap workflow.

### Core Deliverables

- Importer for a clearly documented subset (start with orthogonal finite maps)
- Layer and tileset translation into runtime structures
- Optional object-layer extraction for collision metadata
- Explicit diagnostics for unsupported features

### Acceptance Criteria

- Checked-in Tiled sample imports into a playable scene
- Unsupported constructs fail with actionable messages (no silent fallbacks)

## Verification Gates (Per Phase)

Every phase must pass:

1. Build
2. Unit tests
3. Smoke tests
4. Documentation updates

Use existing scripts and workflows:

- `scripts/build.ps1`
- `scripts/test.ps1`
- `scripts/smoke-test.ps1`
- `scripts/verify.ps1`
- `scripts/render-verify.ps1` (for visually sensitive features)

## Risks and Mitigations

### Risk: TileMap and import scope grows too quickly

Mitigation: ship in thin vertical slices with a documented supported subset first.

### Risk: Camera changes alter existing behavior

Mitigation: default all new camera helpers to disabled/off; preserve existing follow semantics.

### Risk: Animation API overlap/confusion

Mitigation: clearly position `AnimatedSpriteEntity` as the common-case wrapper while retaining generic animator support for advanced use.

## Program-Level Definition of Done

The highlighted gaps from `docs/ENGINE_API_EVALUATION_2026-05-26.md` are resolved or explicitly staged with remaining scope documented in `docs/PROJECT_STATUS.md`, and at least one end-to-end 2D workflow example demonstrates:

- action bindings
- camera feel helpers
- animated sprite states
- tilemap runtime usage
- imported map content
