# Tilemap Level Builder Implementation Plan

This document describes a phased plan for turning the existing tilemap demo into a multi-file game-focused level builder with joystick support and a controller-driven development mode.

The first goal is not full authoring parity with Tiled. The first goal is a cleanly structured game target that:

- reuses the imported tilemap demo as its runtime base,
- supports keyboard and joystick input through one input layer,
- toggles a Development mode with the controller Start button,
- introduces multiple Development submodes, starting with a collision-free Move Mode,
- adds a controller-first Select Tile Mode with a white outline and tile clipboard workflow,
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
- `DevModeController.*`: Development mode activation, active submode, selection state, tile clipboard, and future edit actions.
- `TileCursor.*`: white selection-outline rendering plus world-space selection status presentation.

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

## Current Status

- **Phase 1 is complete.** `games/level_builder/` now exists as a multi-file game target with scene, player, and tilemap-session responsibilities split apart.
- **Phase 2 is complete.** `InputActionMap` now drives keyboard and gamepad input, left-stick movement works in play mode, and Start / Enter toggles Development mode with visible HUD/debug state.
- **Phase 3 is complete.** Development mode now has a real submode controller, and `MoveMode` supports collision-free scene navigation while preserving scrolling.
- **Phase 4 is complete.** `SelectTileMode` now snaps to the nearest tile, shows a white selection outline, supports controller-driven tile navigation, and exposes an on-screen action legend.
- **The next active work is Phase 5.** The remaining milestones are about actual tile mutation, clipboard behavior, persistence, and end-to-end authoring verification.

---

## Phase Overview

| Phase | Status | Milestone | Primary outcome |
|-------|--------|-----------|-----------------|
| 1 | Complete | Milestone 1 | New game target runs the imported map with the existing playable baseline |
| 2 | Complete | Milestone 2 | Unified keyboard and joystick action layer, including Start-button mode toggle |
| 3 | Complete | Milestone 3 | Development submode framework with Move Mode as the default Development submode |
| 4 | Complete | Milestone 4 | Select Tile Mode with white outline, nearest-tile acquisition, and controller-driven navigation |
| 5 | Planned | Milestone 5 | Tile editing actions: next/previous tile cycling, copy/paste clipboard, and action legend/debug state |
| 6 | Planned | Milestone 6 | Persistence, smoke coverage, docs, polish, and final verification |

---

## Phase 1: Scaffold the Game and Preserve the Playable Baseline ✅ COMPLETE

**Goal:** Move the tilemap demo into a proper `games/` target without changing behavior.

**Status:** Complete.

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

## Phase 2: Unify Input and Add Joystick Support ✅ COMPLETE

**Goal:** Replace ad hoc key handling with a named action layer that supports keyboard and gamepad from the start.

**Status:** Complete.

### Tasks

1. **Create a game input class around named actions**
   - Implement `Input.h` using `InputActionMap` instead of only `KeyStateTracker`.
   - Route keyboard and gamepad input through one surface.

2. **Define the initial action set**
   - Play actions: `move_left`, `move_right`, `jump`, `reset`.
   - Mode action: `toggle_dev_mode`.
   - Defer submode-specific selection and tile-edit actions to later phases.

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

## Phase 3: Add Development Submodes and Move Mode ✅ COMPLETE

**Goal:** Turn Development mode into a real authoring state machine, with Move Mode as the default Development submode.

**Status:** Complete.

### Tasks

1. **Create a dedicated development-mode controller**
   - Add `DevModeController.*`.
   - Track whether Development mode is active.
   - Track the active Development submode.
   - First submodes: `MoveMode` and `SelectTileMode`.

2. **Define the submode lifecycle clearly**
   - Play mode: player simulation active, movement controls the player.
   - Development mode entry defaults to `MoveMode`.
   - Leaving Development mode returns to Play mode without losing the current player location.
   - Add explicit submode-switch actions so controller users can move between Development submodes predictably.

3. **Implement Move Mode behavior**
   - In `MoveMode`, the movement controls move the main character sprite freely in 2D.
   - Disable gravity and collision resolution while `MoveMode` is active.
   - Preserve camera scrolling/follow behavior so the scene remains navigable at map scale.

4. **Keep play-mode and Development-mode responsibilities separate**
   - Play mode continues to use the existing physics/collision path.
   - `MoveMode` uses direct positioning with no collision constraints.
   - Avoid folding free-move rules back into the play-mode controller.

5. **Surface submode state in the HUD and debug UI**
   - Expand the Development mode HUD/debug state to show the current submode.
   - Make it obvious to the user that `MoveMode` is the default Development submode.

6. **Choose and document controller mappings for submode switching**
   - Pick explicit joypad inputs for “next submode” and “previous submode” or an equivalent scheme.
   - Keep these bindings visible in the debug UI during early implementation.

### Acceptance Criteria

- Development mode can be entered and exited reliably.
- Entering Development mode lands in `MoveMode` by default.
- In `MoveMode`, the main character sprite can be moved anywhere in the scene.
- No collisions or gravity interfere with `MoveMode` navigation.
- Camera scrolling remains preserved while the player is moved freely.

### Milestone 3

Development mode has a real submode framework, and `MoveMode` is usable as the default no-collision scene-navigation submode.

---

## Phase 4: Add Select Tile Mode and Tile Navigation ✅ COMPLETE

**Goal:** Add a controller-first tile-selection workflow centered on the player’s current location.

**Status:** Complete.

### Tasks

1. **Create Select Tile Mode as a Development submode**
   - Add `SelectTileMode` to the Development submode controller.
   - Ensure switching into `SelectTileMode` does not discard the current player position.

2. **Acquire the initial selection from the player’s location**
   - When entering `SelectTileMode`, find the tile closest to the main character sprite.
   - Snap the initial selection to that closest tile.

3. **Add tile coordinate helpers to the session layer**
   - Convert between world space and tile coordinates.
   - Clamp selection to valid map bounds.
   - Expose tile-center positions for cursor visuals.

4. **Render a white selection outline**
   - Add `TileCursor.*` or equivalent selection-outline rendering.
   - Use a white outline as the default selection visual.
   - Keep the outline clearly visible against the imported map art.

5. **Route movement controls into tile navigation while Select Tile Mode is active**
   - The joystick or movement controls should move the selection tile-by-tile in the requested direction.
   - D-pad input should step one tile per press.
   - Left-stick input should use threshold-based stepping with repeat timing instead of analog drift.

6. **Display the available joypad actions on screen**
   - Add a HUD panel or world-space overlay listing the joypad actions available in `SelectTileMode`.
   - This list should become the canonical in-game explanation of the tile-edit controls.

### Acceptance Criteria

- Switching into `SelectTileMode` produces a white outline on the tile closest to the main character.
- The joystick or movement controls move the selection by tile in the expected direction.
- The selected tile remains visible and clamped to map bounds.
- An on-screen joypad-action list explains the controls available in `SelectTileMode`.

### Milestone 4

`SelectTileMode` is navigable and understandable before the first tile-mutation actions are wired.

---

## Phase 5: Add Tile Actions and Clipboard Workflow

**Goal:** Make `SelectTileMode` capable of useful controller-driven tile edits.

### Tasks

1. **Keep tile mutations behind the session layer**
   - Expose tile-edit operations on `TileMapSession`.
   - Do not let scene code mutate `TileMap` directly.

2. **Implement “next tile” and “previous tile” actions**
   - Add joypad actions that change the selected tile to the next tile in the tileset ordering.
   - Add matching actions that change the selected tile to the previous tile.
   - Decide whether these actions mutate immediately or use a preview/apply model, then document that choice.

3. **Implement copy and paste tile actions**
   - Add a joypad action that copies the tile currently under selection into a clipboard value.
   - Add a second joypad action that pastes the copied tile onto the current selection.
   - Treat the clipboard as explicit Development state so it survives submode switches during the same session.

4. **Expose clipboard and edit state in the debug UI**
   - Show the currently copied tile in the debug panel.
   - If no tile has been copied yet, show that state explicitly.

5. **Keep the on-screen action legend synchronized with live behavior**
   - The joypad-action list shown in `SelectTileMode` must reflect the actual current bindings for next, previous, copy, and paste.

6. **Refresh dependent data after edits**
   - Rebuild collision caches when edited tiles affect solid or one-way collision data.
   - Keep visual and collision state synchronized.

### Acceptance Criteria

- Joypad actions can change the selected tile to the next or previous tile.
- A selected tile can be copied and pasted onto another selection.
- The debug UI shows the currently copied tile.
- The on-screen joypad-action list matches the actual editing actions available to the player.

### Milestone 5

`SelectTileMode` supports controller-driven next/previous/copy/paste editing with visible clipboard/debug state.

---

## Phase 6: Persistence, Smoke Coverage, Documentation, and Polish

**Goal:** Persist authored changes and verify the full controller-first authoring workflow end to end.

### Tasks

1. **Choose a persistence strategy explicitly**
   - Option A: save a VDE-native patch or overlay format.
   - Option B: save a full VDE-authored map format.
   - Option C: add Tiled JSON export for the supported subset.

2. **Implement the smallest viable save/load path**
   - At minimum, support saving and restoring the current working map or overlay.
   - Prefer the strategy with the smallest correctness surface and clearest testability.

3. **Extend smoke coverage across the full workflow**
   - Launch the game.
   - Toggle Development mode.
   - Enter `MoveMode` and move the player freely.
   - Switch to `SelectTileMode`.
   - Move the selection.
   - Perform at least one next/previous tile change.
   - Perform at least one copy/paste action.
   - Save/load if persistence exists in this phase.

4. **Document controls and workflow**
   - Update the game README with play-mode controls, Development mode controls, submode switching, and `SelectTileMode` joypad actions.
   - Document clipboard semantics and persistence limitations.

5. **Polish gamepad UX**
   - Tune stick dead zones and selection repeat timing.
   - Ensure no accidental mode toggles or cursor drift.
   - Validate that the white selection outline remains readable while scrolling.

6. **Verify the full path**
   - Build.
   - Run tests.
   - Run smoke coverage for the changed game.
   - Run lint.
   - Re-review the final diff.

7. **Identify the next roadmap slice**
   - Candidate follow-ups: undo/redo, tile palette selection, region tools, object-layer editing, export, or migration into a first-class tool.

### Acceptance Criteria

- Edited maps or overlays can be saved and restored through a documented path.
- Smoke coverage exercises Development mode entry, submode switching, selection movement, tile mutation, and copy/paste.
- The new game is documented and scriptable.
- The controller workflow is stable enough for repeated level-layout iteration.

### Milestone 6

The level builder supports a controller-first authoring loop that is persisted, smoke-tested, documented, and ready for iterative expansion.

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
| 3 | Development submode framework with default no-collision `MoveMode` |
| 4 | `SelectTileMode` with white outline, nearest-tile acquisition, and controller navigation |
| 5 | Controller-driven next/previous/copy/paste tile editing with visible clipboard/debug state |
| 6 | Save/load path plus smoke-tested, documented, iteration-ready level builder |

---

## Risks and Decision Points

1. **Game vs tool boundary**
   - If the project becomes editor-first rather than play-first, it may eventually belong under `tools/` instead of `games/`.
   - The proposed structure keeps that migration possible by isolating editing concerns from the scene shell.

2. **Persistence format choice**
   - This is the most expensive wrong decision in the plan.
   - It should be made only after the in-memory editing model is stable.

3. **Camera behavior across Development submodes**
   - `MoveMode` should preserve the current scrolling/follow feel.
   - `SelectTileMode` may eventually need different camera rules, but the first version should stay simple.

4. **Submode input mapping and action-legend drift**
   - The controller mapping for submode switching and tile actions must stay synchronized with the on-screen joypad-action list.
   - Drift between actual bindings and displayed help will make the authoring workflow unusable.

5. **Collision rebuild cost**
   - If edits become frequent, collision extraction may need optimization or dirty-region updates later.
   - That is not a reason to complicate the first editing milestone.

---

## Summary

The correct first move was to stop extending `examples/tilemap_demo/main.cpp` directly and instead create a new multi-file game target that treats the tilemap demo as imported gameplay logic. That foundation now exists, and the unified input layer plus Development mode toggle are already in place.

The first two phases are now complete:

1. port the demo into a real game structure,
2. unify input and add Start-button mode toggling.

The remaining milestone sequence is:

1. add Development submodes with default no-collision `MoveMode`,
2. add `SelectTileMode` with a white outline on the nearest tile,
3. add next/previous/copy/paste tile actions plus visible joypad-action and clipboard state,
4. add persistence,
5. verify, document, and polish the workflow.