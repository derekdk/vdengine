# Tilemap Level Builder Implementation Plan

This document describes a phased plan for turning the existing tilemap demo into a multi-file game-focused level builder with joystick support and a controller-driven development mode.

The first goal is not full authoring parity with Tiled. The first goal is a cleanly structured game target that:

- reuses the imported tilemap demo as its runtime base,
- supports keyboard and joystick input through one input layer,
- toggles a Development mode with the controller Start button,
- lets the user select any tile with the joystick,
- leaves clear extension points for future tile editing and map authoring features.

---

## Background

The current implementation in `examples/tilemap_demo/main.cpp` proves that VDE can:

- import a checked-in `.tmj` tilemap,
- render layered tiles with parallax backgrounds,
- extract collision rectangles,
- run a playable side-view scene.

What it does not provide is a maintainable game structure for level-authoring workflows. Input, scene bootstrapping, collision resolution, and map logic are all in one file. For the requested direction, the next step is to move that behavior into a proper `games/` target with files split by concern.

---

## Proposed Target

Create a new game target under `games/level_builder/`.

Recommended initial file layout:

```text
games/
  level_builder/
    CMakeLists.txt
    main.cpp
    README.md
    vde.toml
    Input.h
    LevelBuilderScene.h
    LevelBuilderScene.cpp
    TileMapSession.h
    TileMapSession.cpp
    PlayerController.h
    PlayerController.cpp
    DevModeController.h
    DevModeController.cpp
    TileCursor.h
    TileCursor.cpp
```

Concern split:

- `main.cpp`: game entry point and scene wiring.
- `Input.h`: named actions for keyboard and gamepad.
- `LevelBuilderScene.*`: top-level orchestration, mode switching, HUD, camera routing.
- `TileMapSession.*`: tilemap import, collision cache, tile coordinate helpers, future editing surface.
- `PlayerController.*`: play-mode movement and collision behavior migrated from the tilemap demo.
- `DevModeController.*`: Development mode state, selection, layer targeting, future edit actions.
- `TileCursor.*`: world-space selection visuals and status presentation.

---

## Non-Goals for the First Cut

These should stay out of the first milestone unless they are nearly free:

- full Tiled `.tmj` export,
- undo/redo,
- brush palettes,
- multi-tile paint tools,
- mouse-driven UI,
- standalone tool-mode conversion.

The engine currently exposes import support for Tiled maps, but not an obvious matching export pipeline. That makes persistence a later milestone, not part of the bootstrap phase.

---

## Phase Overview

| Phase | Milestone | Primary outcome |
|-------|-----------|-----------------|
| 1 | Milestone 1 | New game target runs the imported map with the existing playable baseline |
| 2 | Milestone 2 | Unified keyboard and joystick action layer, including Start-button mode toggle |
| 3 | Milestone 3 | Development mode with joystick-driven tile selection and visible cursor |
| 4 | Milestone 4 | First in-memory map operations routed through an editor/session layer |
| 5 | Milestone 5 | Persistence strategy and authoring workflow defined and implemented |
| 6 | Milestone 6 | Smoke coverage, docs, polish, and verification |

---

## Phase 1: Scaffold the Game and Preserve the Playable Baseline

**Goal:** Move the tilemap demo into a proper `games/` target without changing behavior.

### Tasks

1. **Create the new game target**
   - Add `games/level_builder/` using the repo's game structure.
   - Wire the target into `games/CMakeLists.txt` and its per-game `CMakeLists.txt`.

2. **Create the entry point**
   - Add `main.cpp` using `vde::games::BaseGame<...>`.
   - Keep window size and launch behavior aligned with the existing tilemap demo unless a different default is justified.

3. **Move scene orchestration out of the example**
   - Create `LevelBuilderScene.h` and `LevelBuilderScene.cpp`.
   - Port map loading, background setup, camera setup, and reset flow out of `examples/tilemap_demo/main.cpp`.

4. **Move player physics out of the scene monolith**
   - Create `PlayerController.*`.
   - Lift horizontal movement, jump, gravity, and collision resolution into that controller.
   - Preserve current spawn, jump, and collision behavior.

5. **Wrap map ownership in a session object**
   - Create `TileMapSession.*`.
   - Move tilemap import, collision extraction, imported object lookup, and map-level helpers into it.

### Acceptance Criteria

- The new game launches and renders the imported tilemap.
- The player can still move, jump, collide, and reset as in the current demo.
- The old example remains available until the new game is stable enough to replace it, if replacement is desired later.

### Milestone 1

`games/level_builder` exists and behaves like the current tilemap demo, but with code split into scene, input, player, and map/session files.

---

## Phase 2: Unify Input and Add Joystick Support

**Goal:** Replace ad hoc key handling with a named action layer that supports keyboard and gamepad from the start.

### Tasks

1. **Create a game input class around named actions**
   - Implement `Input.h` using `InputActionMap` instead of only `KeyStateTracker`.
   - Route keyboard and gamepad input through one surface.

2. **Define the initial action set**
   - Play actions: `move_left`, `move_right`, `jump`, `reset`.
   - Mode action: `toggle_dev_mode`.
   - Cursor actions: `cursor_left`, `cursor_right`, `cursor_up`, `cursor_down`.
   - Reserve future actions now: `apply_tile`, `clear_tile`, `prev_layer`, `next_layer`.

3. **Bind keyboard and gamepad controls**
   - Keyboard: preserve A/D or arrows, jump keys, reset key.
   - Gamepad movement: left stick and D-pad.
   - Gamepad mode toggle: `GAMEPAD_BUTTON_START`.

4. **Decide mode-toggle parity**
   - Add a keyboard equivalent for Start, such as Enter or Tab, so Development mode is testable without a controller.

5. **Advance input state once per frame**
   - Make frame-based action transitions explicit so mode toggles and tile cursor stepping are deterministic.

### Acceptance Criteria

- The game can be played with either keyboard or gamepad.
- Pressing Start toggles Development mode on and off.
- Input code no longer depends on scattered raw button checks outside the input layer.

### Milestone 2

The new game has a single input abstraction that supports current movement and future editor actions, with Start-button mode toggling implemented.

---

## Phase 3: Add Development Mode and Joystick Tile Selection

**Goal:** Make Development mode usable as a controller-first tile selection workflow.

### Tasks

1. **Create a dedicated development-mode controller**
   - Add `DevModeController.*`.
   - Track whether Development mode is active.
   - Track selected layer, selected tile column, selected tile row, and cursor repeat timers.

2. **Define mode behavior clearly**
   - Play mode: player simulation active, movement controls the player.
   - Development mode: player simulation paused or ignored, cursor input moves tile selection.
   - Preserve the player's location when switching modes.

3. **Add tile coordinate helpers to the session layer**
   - Convert between world space and tile coordinates.
   - Clamp selection to valid map bounds.
   - Expose tile center positions for cursor visuals.

4. **Implement joystick-friendly cursor stepping**
   - D-pad: one tile per press.
   - Left stick: threshold-based movement with initial press plus repeat delay.
   - Avoid frame-rate-dependent movement.

5. **Create visible selection feedback**
   - Add `TileCursor.*`.
   - Render a clear highlight over the selected tile.
   - Show at least column, row, and active layer in a HUD or world-space label.

6. **Route camera behavior for authoring**
   - Decide whether the camera follows the cursor, remains free, or snaps to keep selection visible.
   - Prefer a simple follow/keep-visible rule first.

### Acceptance Criteria

- Development mode can be entered and exited reliably.
- The joystick can select any tile on the map.
- The currently selected tile is always visible and clearly highlighted.
- Cursor movement is discrete and controllable, not analog drift.

### Milestone 3

The project is usable as a controller-driven tile selection tool: Start toggles modes, and the user can move a highlighted cursor to any tile.

---

## Phase 4: Add First Editing Primitives

**Goal:** Turn the selector into the start of a real level-editing workflow.

### Tasks

1. **Keep all tile mutations behind the session layer**
   - Expose methods on `TileMapSession` for operations like `setTile`, `clearTile`, and later region fill.
   - Do not let scene code mutate `TileMap` directly.

2. **Implement the first safe operations**
   - Apply a currently selected tile ID to the current cell.
   - Clear the current cell.
   - Switch active layer.

3. **Refresh dependent data after edits**
   - Rebuild collision caches when solid or one-way tiles change.
   - Keep visual and collision state synchronized.

4. **Add a minimal editing state model**
   - Track active paint tile ID.
   - Track active layer.
   - Expose the current values in the HUD.

5. **Keep the API reversible for future work**
   - Shape the session/controller APIs so undo/redo can be added later without rewriting the scene.

### Acceptance Criteria

- A selected tile can be modified in memory through Development mode.
- Layer selection works.
- Collision rebuilds stay correct after edits that affect terrain.

### Milestone 4

Development mode is no longer view-only. It can change the runtime tilemap through a dedicated editing/session layer.

---

## Phase 5: Persistence and Authoring Workflow

**Goal:** Decide how authored changes survive beyond one run and implement the smallest viable persistence path.

### Tasks

1. **Choose a persistence strategy explicitly**
   - Option A: save a VDE-native patch or overlay format.
   - Option B: save a full VDE-authored map format.
   - Option C: add Tiled JSON export for the supported subset.

2. **Implement the lowest-risk first version**
   - Prefer the strategy with the smallest correctness surface and the clearest testability.
   - If Tiled export is expensive, start with a VDE-native save format and add conversion later.

3. **Add session commands around persistence**
   - New map/load map/save map if those are in scope.
   - At minimum, support save of the current working map.

4. **Document compatibility limits**
   - Be explicit about which Tiled constructs round-trip cleanly and which do not.

### Acceptance Criteria

- Edited maps can be saved and restored through a documented path.
- The save format and compatibility rules are explicit.

### Milestone 5

The level builder supports a real authoring loop instead of only runtime experimentation.

---

## Phase 6: Smoke Coverage, Documentation, and Polish

**Goal:** Make the feature stable enough for repeated iteration and future expansion.

### Tasks

1. **Add smoke coverage**
   - Create a smoke script for launching the new game, toggling Development mode, moving selection, and performing at least one edit if editing is available.

2. **Document controls and workflow**
   - Add a game README with play controls, Development mode controls, and current authoring limitations.

3. **Polish gamepad UX**
   - Tune stick dead zones and cursor repeat timing.
   - Ensure no accidental mode toggles or cursor drift.

4. **Verify the full path**
   - Build.
   - Run tests.
   - Run smoke coverage for the changed game.

5. **Identify the next roadmap slice**
   - Candidate follow-ups: undo/redo, tile palette selection, region tools, object-layer editing, export, or migration into a first-class tool.

### Acceptance Criteria

- The new game is documented and scriptable.
- The controller workflow is stable enough for repeated level-layout iteration.
- The feature set is ready for the next editing milestone without another structural rewrite.

### Milestone 6

The level builder is verified, documented, and ready for iterative expansion.

---

## Recommended Execution Order Inside Each Phase

For each phase, use the same local execution loop:

1. Create or move the smallest file slice needed.
2. Wire it into the running game.
3. Validate with the narrowest relevant build/test/smoke check.
4. Only then open the next slice.

That order matters most in Phases 1 through 4, where structure and runtime behavior are changing together.

---

## Milestone Deliverables

| Milestone | Deliverable |
|-----------|-------------|
| 1 | Multi-file `games/level_builder` target preserving tilemap demo gameplay |
| 2 | Unified input system with controller support and Start-button Development mode toggle |
| 3 | Controller-driven tile selection with visible cursor and map-aware clamping |
| 4 | In-memory tile editing routed through a session/editor layer |
| 5 | Save/load path for authored content |
| 6 | Smoke-tested, documented, iteration-ready level builder |

---

## Risks and Decision Points

1. **Game vs tool boundary**
   - If the project becomes editor-first rather than play-first, it may eventually belong under `tools/` instead of `games/`.
   - The proposed structure keeps that migration possible by isolating editing concerns from the scene shell.

2. **Persistence format choice**
   - This is the most expensive wrong decision in the plan.
   - It should be made only after the in-memory editing model is stable.

3. **Camera behavior in Development mode**
   - This should stay simple at first.
   - Free camera, follow cursor, and hybrid modes can be added later if needed.

4. **Collision rebuild cost**
   - If edits become frequent, collision extraction may need optimization or dirty-region updates later.
   - That is not a reason to complicate the first editing milestone.

---

## Summary

The correct first move is to stop extending `examples/tilemap_demo/main.cpp` directly and instead create a new multi-file game target that treats the tilemap demo as imported gameplay logic. Once that structure is in place, joystick support and Development mode become straightforward incremental additions rather than another round of monolithic edits.

The key milestone sequence is:

1. port the demo into a real game structure,
2. unify input and add Start-button mode toggling,
3. build controller-driven tile selection,
4. add in-memory editing,
5. add persistence,
6. verify and document the workflow.