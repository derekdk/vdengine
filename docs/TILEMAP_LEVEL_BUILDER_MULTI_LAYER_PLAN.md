# Tilemap Level Builder Multi-Layer Plan

This document describes a phased plan for extending `games/level_builder/` from a single editable ground layer into a multi-layer authoring workflow where the user can:

- add new layers,
- choose each layer's z-order,
- configure each layer's scrolling behavior,
- preserve those settings through save and load,
- keep gameplay collision scoped to the intended layers.

The plan starts from the current level-builder state rather than from the original tilemap-demo migration. The current game already has persistence, palette-driven painting, and undo or redo support for one editable layer. The remaining work is to generalize that workflow to multiple layers without breaking camera feel, collision, or save compatibility.

---

## Goals

The first cut of multi-layer support should make the following workflow possible without editing code:

1. Create a new tile layer at runtime.
2. Select which layer is active for painting.
3. Set the layer's z-order relative to other layers.
4. Configure how the layer scrolls relative to the camera.
5. Save and reload the resulting layer stack and metadata.

That first cut does not need to solve every future authoring problem. It only needs to establish a correct layer model that later tools can build on.

---

## Current Constraints

The existing codebase has three relevant properties:

1. `vde::TileMap` already supports multiple internal layers and per-layer depth.
2. `games/level_builder/TileMapSession` still assumes one editable layer at index 0 for editing, undo or redo, and persistence.
3. TileMap layers do not currently have independent scrolling rules; they share one entity transform.

That means the missing capability is not just "more layers." The missing capability is a layer model that owns:

- editable tiles per layer,
- persisted depth metadata,
- persisted scrolling metadata,
- collision participation rules,
- runtime rendering behavior that can actually honor different scroll settings.

---

## Recommended Architecture

Use one runtime `TileMap` entity per authorable layer in the level builder instead of extending one `TileMap` entity to carry all scroll behavior internally.

Reasoning:

- Per-layer z-order is already natural when each layer has its own entity transform and mesh.
- Per-layer scroll rules become a per-entity camera transform problem instead of an engine-wide TileMap renderer rewrite.
- Adding, selecting, hiding, or deleting a layer becomes a level-builder concern instead of a lower-level engine concern.
- Collision participation can be controlled explicitly per layer instead of treating every rendered layer as gameplay geometry.

This plan therefore treats the current single `TileMap` instance as a temporary implementation detail that should become a `LayerRuntime` stack owned by `TileMapSession` and mounted by `LevelBuilderScene`.

---

## Proposed Data Model

The first implementation should introduce an explicit layer definition and a small scroll-rule model.

Recommended first-cut shape:

```text
LayerDefinition
  id: stable string id
  name: display name
  tiles: row-major tile ids
  depthZ: render depth
  visible: render on/off
  collisionEnabled: contributes to collision extraction
  followFactorX: camera follow factor on X
  followFactorY: camera follow factor on Y
  scrollVelocityX: continuous scroll velocity on X
  scrollVelocityY: continuous scroll velocity on Y
  scrollOffsetX: saved offset seed on X
  scrollOffsetY: saved offset seed on Y
```

Recommended semantics for the first cut:

- `followFactor = (1, 1)` means world-locked gameplay behavior.
- `followFactor < 1` on either axis creates parallax.
- `scrollVelocity != 0` adds continuous drift on top of camera-relative motion.
- `collisionEnabled = false` by default for newly added decorative layers.

This is intentionally data-driven. A numeric scroll rule is easier to persist, test, and extend than a large enum tree of special cases.

---

## Persistence Direction

The current overlay format stores one `editable_layer` block. The multi-layer format should move to a versioned `layers` array.

Recommended direction:

- Keep the existing file path.
- Bump the overlay version.
- Add a `layers` array with one entry per layer definition.
- Keep backward compatibility by loading the old one-layer format as an implicit layer stack with one editable ground layer.

The overlay should persist:

- layer identity and order,
- layer name,
- tile payload,
- depth,
- visibility,
- collision participation,
- scroll metadata.

Undo or redo history should remain session-only and should not be serialized.

---

## Phase Overview

| Phase | Status | Milestone | Primary outcome |
|-------|--------|-----------|-----------------|
| 1 | Complete | Milestone A | Multi-layer session model and overlay schema exist |
| 2 | Complete | Milestone B | Runtime rendering uses one TileMap entity per authorable layer |
| 3 | Complete | Milestone C | User can add, select, hide, and depth-order layers |
| 4 | Complete | Milestone D | User can configure per-layer scrolling rules |
| 5 | Complete | Milestone E | Editing, collision, and history work correctly across layers |
| 6 | Planned | Milestone F | Save/load, smoke coverage, docs, and polish are complete |

---

## Phase 1: Multi-Layer Session Model and Overlay Schema

**Goal:** Replace the single editable-layer assumption with an explicit layer stack model in `TileMapSession`.

### Tasks

1. Introduce a `LayerDefinition` model inside `TileMapSession` or in a neighboring header.
2. Replace `kEditableLayerIndex`-style assumptions with an active-layer or layer-id based API.
3. Generalize tile read, tile write, and tile-cycle helpers to take a target layer.
4. Update undo or redo records to include layer identity as well as tile coordinate and tile ids.
5. Replace the one-layer overlay schema with a versioned `layers` array.
6. Add backward-compatible loading for the current one-layer overlay format.

### Acceptance Criteria

- `TileMapSession` can represent more than one editable layer in memory.
- Save and load preserve multiple layers and their metadata.
- Existing one-layer overlay files still load correctly.

### Milestone A

The level builder owns a real multi-layer data model instead of a hard-coded editable layer 0.

---

## Phase 2: Runtime Layer Stack Rendering

**Goal:** Move the level builder from one mounted `TileMap` entity to one runtime `TileMap` entity per authorable layer.

### Tasks

1. Add a `LayerRuntime` concept that pairs a `LayerDefinition` with a live `TileMap` entity.
2. Build one `TileMap` entity per layer from the session model.
3. Keep all runtime layers on the same tileset and tile dimensions for the first cut.
4. Drive each runtime layer's depth from the stored `depthZ` metadata.
5. Add sync helpers so tile edits update only the active runtime layer instead of rebuilding the whole stack when unnecessary.

### Acceptance Criteria

- Two or more tile layers can render at different z positions in the same scene.
- Layer order is stable and deterministic after edits, save/load, and reset.
- Layer creation does not require re-importing the base map from disk.

### Milestone B

The level builder renders a stack of independently configurable tilemap layers.

---

## Phase 3: Layer Authoring Workflow

**Goal:** Give the user an in-game path to create and manage layers.

### Tasks

1. Add actions for:
   - add layer,
   - next layer,
   - previous layer,
   - layer depth up,
   - layer depth down,
   - toggle layer visibility.
2. Decide whether active-layer selection belongs in `DevModeController` or the scene shell; keep persistence state in `TileMapSession` and transient selection state out of the overlay.
3. Auto-name new layers in the first cut rather than blocking on text-entry UI.
4. Surface active layer name, index, depth, visibility, and collision participation in the HUD and debug UI.

### Acceptance Criteria

- The user can add a new layer without editing code.
- The user can switch the active layer and see which layer is selected.
- The user can change z-order in-game and immediately see the result.

### Milestone C

The level builder has a controller- and keyboard-friendly layer-management path.

---

## Phase 4: Per-Layer Scrolling Rules

**Goal:** Make each layer's movement relative to the camera independently configurable.

### Tasks

1. Implement a small scroll-rule runtime that updates each layer entity every frame from:
   - camera position,
   - saved base offset,
   - follow factor,
   - scroll velocity.
2. Preserve world-locked behavior for gameplay layers by default.
3. Add authoring controls for changing the active layer's follow factor and scroll velocity.
4. Add a few first-cut presets in UI text for clarity:
   - gameplay layer,
   - mild parallax,
   - strong parallax,
   - drifting decorative layer.
5. Persist the scroll metadata in the overlay.

### Acceptance Criteria

- A newly added layer can be configured to scroll differently from the base gameplay layer.
- Scroll settings survive save and load.
- Layer depth and scroll behavior do not fight each other visually.

### Milestone D

Each layer can have its own scrolling behavior instead of sharing one world transform.

---

## Phase 5: Multi-Layer Editing, Collision, and History

**Goal:** Keep editing and gameplay semantics correct once multiple layers exist.

### Tasks

1. Make palette-driven paint operations target the active layer only.
2. Ensure undo or redo records restore the correct layer and tile coordinate.
3. Scope collision extraction to layers with `collisionEnabled = true`.
4. Keep decorative or parallax-only layers from changing player collision unless explicitly enabled.
5. Decide whether new layers start empty, cloned from the active layer, or cloned from a template; use the smallest predictable behavior for the first cut.

### Acceptance Criteria

- Painting affects only the active layer.
- Undo or redo works across edits on multiple layers.
- Decorative layers do not create accidental collision.

### Milestone E

The multi-layer authoring workflow is safe for gameplay iteration, not just rendering experiments.

---

## Phase 6: Verification, Migration, and Documentation

**Goal:** Verify the new architecture end to end and make it usable for repeated iteration.

### Tasks

1. Add focused unit tests for:
   - multi-layer save and load,
   - old overlay migration,
   - per-layer undo or redo,
   - collision filtering by layer.
2. Extend smoke coverage to exercise:
   - add layer,
   - select active layer,
   - adjust depth,
   - configure scroll behavior,
   - paint,
   - undo or redo,
   - save and reload.
3. Update the level-builder README with the new layer workflow and controls.
4. Document the overlay schema changes and compatibility behavior.
5. Verify the full path with build, unit tests, smoke tests, render verification if needed, lint, and review.

### Acceptance Criteria

- The full multi-layer workflow is documented and smoke-tested.
- Existing single-layer overlays still load into the new session model.
- Verification passes without breaking the current level-builder gameplay baseline.

### Milestone F

The level builder supports add-layer, z-order, and per-layer scrolling through a verified multi-layer authoring loop.

---

## Risks and Decision Points

1. **Finite tilemap parallax edges**
   - A finite layer with strong parallax can expose empty space near map edges.
   - First cut should document this and avoid promising seamless infinite backgrounds for authorable tile layers.

2. **One TileMap with many layers vs many TileMap entities**
   - The single-entity approach keeps fewer scene entities but pushes scroll behavior into the engine renderer.
   - The per-layer entity approach keeps the change local to the level builder and is the recommended first implementation.

3. **Overlay migration**
   - The one-layer format is already in use.
   - Multi-layer persistence should version the schema rather than silently changing field meaning.

4. **Controller-first layer authoring complexity**
   - Layer creation, selection, depth changes, and scroll-rule tuning can overwhelm the current action legend if everything lands at once.
   - The UI should expose only the current layer-editing bindings for the active submode and keep parameter tuning incremental.

---

## Recommended Execution Order

Use the following order even if the final feature set is broader:

1. Land the multi-layer session model and overlay schema.
2. Split runtime rendering into one `TileMap` entity per layer.
3. Add layer-creation and active-layer selection controls.
4. Add z-order editing.
5. Add scroll-rule editing.
6. Then widen collision, history, smoke coverage, and docs.

That order matters because scroll rules are not meaningful until layer runtime ownership is explicit.

---

## Follow-On Work After This Plan

Once this plan is complete, the next natural slices are:

- region tools for multi-tile paint and fill,
- object-layer authoring,
- export or round-trip back to Tiled metadata when the supported subset is stable,
- optional engine-level TileMap enhancements if multiple games need per-layer scroll rules outside the level builder.