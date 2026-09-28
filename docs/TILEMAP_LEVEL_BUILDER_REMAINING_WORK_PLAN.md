# Tilemap Level Builder Remaining Work Plan

This plan covers what is left for `games/level_builder/` after the completed
[TILEMAP_LEVEL_BUILDER_IMPLEMENTATION_PLAN.md](TILEMAP_LEVEL_BUILDER_IMPLEMENTATION_PLAN.md)
(Phases 1–6 plus undo/redo and palette slices) and
[TILEMAP_LEVEL_BUILDER_MULTI_LAYER_PLAN.md](TILEMAP_LEVEL_BUILDER_MULTI_LAYER_PLAN.md)
(Phases 1–6).

It is based on a code review of the current implementation, not only on the earlier plans. Items
marked **defect** are existing behavior that is wrong or inconsistent today. Items marked **gap** are
missing capabilities.

---

## Implemented Baseline

| Area | What exists | Evidence |
|------|-------------|----------|
| Game target | Multi-file game under `games/level_builder/`, registered in `games/CMakeLists.txt` | [../games/level_builder/](../games/level_builder/) |
| Input | `InputActionMap`-backed per-mode binding presets (Play, Dev Move, Dev Select Tile), keyboard and gamepad, held-state replay on mode swaps | [../games/level_builder/Input.h](../games/level_builder/Input.h) |
| Development mode | Start / Enter toggle; `MoveMode` (free move, no collision) and `SelectTileMode` submodes; LB / RB cycle | [../games/level_builder/DevModeController.h](../games/level_builder/DevModeController.h) |
| Tile selection | Nearest-tile acquisition, white outline cursor, repeat-timed stick stepping, parallax-aware cursor placement | [../games/level_builder/TileCursor.h](../games/level_builder/TileCursor.h), [../games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L833) |
| Painting | Palette/clipboard brush: next/previous palette tile, copy from map, paint single tile on the active layer | [../games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L308) |
| Palette UI | Floating 8-column palette, capped at 256 tiles, highlighted current brush | [../games/level_builder/TilePalette.cpp](../games/level_builder/TilePalette.cpp#L10) |
| History | Per-tile undo/redo with layer identity, branch truncation, dirty-state tracking against saved snapshot | [../games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L892) |
| Layers | Add layer, select previous/next, toggle visibility, adjust depth, cycle four scroll presets | [../games/level_builder/TileMapSession.h](../games/level_builder/TileMapSession.h) |
| Runtime rendering | One runtime `TileMap` entity per authorable layer with camera-follow factors and scroll velocity | [../games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L751) |
| Collision | Collision cache built only from `collisionEnabled` layers; collision layers forced to the Gameplay scroll preset | [../games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L1160) |
| Persistence | Version 2 `layers` overlay with v1 migration, size/layer/tile caps, staged (all-or-nothing) reload | [../games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L974) |
| Unit tests | `TileMapSession`, `LevelBuilderInput`, `DevModeController`, `PlayerController` | [../tests/TileMapSession_test.cpp](../tests/TileMapSession_test.cpp), [../tests/LevelBuilderInput_test.cpp](../tests/LevelBuilderInput_test.cpp), [../tests/DevModeController_test.cpp](../tests/DevModeController_test.cpp), [../tests/PlayerController_test.cpp](../tests/PlayerController_test.cpp) |
| Smoke / render | Keyboard-driven smoke flow; one Select Tile Mode golden image | [../smoketests/scripts/smoke_level_builder.vdescript](../smoketests/scripts/smoke_level_builder.vdescript), [../smoketests/scripts/verify_level_builder_select_tile.vdescript](../smoketests/scripts/verify_level_builder_select_tile.vdescript) |

---

## Review Findings

### Correctness defects

Defects 1–4 and UX gaps 1–2 below were fixed in Phase 0; they are kept here as the record of what
the review found.

1. **Layer count can exceed what reload accepts.** `addLayer()` has no cap, but reload rejects
   overlays with more than 64 layers. Saving 65+ layers writes a file that can never be loaded.
   See [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L727).
2. **Collision is never produced for added layers.** Added layers exist only in `m_layers`. The
   backing `m_tileMap` keeps the imported layer count. `writeLayerTile()`, the collision-rebuild
   check in `applyEditableTileId()`, and `rebuildCollisionCache()` all ignore indices beyond it.
   A hand-edited overlay with `collision_enabled: true` on an added layer renders but does not
   collide. See [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L1160).
3. **Saves are not atomic.** `writeTextFile()` truncates the target in place. A crash or write
   failure mid-save corrupts the only overlay copy. See
   [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L48).
4. **Layer IDs are index-derived.** New layers get `"layer_" + index`. This is safe only because
   layers cannot be deleted or reordered today; any delete/reorder feature will produce duplicate
   IDs.
5. **Layer operations bypass history.** Add layer, visibility, depth, and scroll preset changes are
   not undoable. Gamepad `A` in Move Mode adds a layer, so repeated presses create layers that can
   only be removed by reloading.

### UX gaps

1. **No unsaved-changes guard.** `F9` / `R3` reload and `Esc` quit
   ([GameBase.h](../games/GameBase.h#L68)) silently discard dirty edits.
2. **Persistence and validation errors are invisible without ImGui.** Messages such as
   "Collision-enabled layers must use the Gameplay scroll preset." go to `lastPersistenceStatus`.
   The HUD only shows `DIRTY` / `CLEAN` and the file name.
3. **No erase brush.** `cycledEditableTileId()` wraps modulo the sprite count and never yields
   `kEmptyTile`. Erasing is only possible by copying an already-empty cell.
   See [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L852).
4. **Palette navigation is linear only.** Next/previous steps one tile at a time through up to 256
   entries; no 2D palette cursor, paging, or jump. Tilesets larger than 256 tiles are truncated.
5. **Controller budget in Move Mode is exhausted.** A, B, X, Y, LT, RT, LB, RB, L3, R3, right-stick X,
   Start, Back, and D-pad/left stick are all bound. The on-screen legend is a fixed 11 lines and
   Move Mode already uses all 11. New layer actions cannot be added without a new submode or
   paged legend.
6. **Collision participation is not editable in-game.** It can only be changed by editing the
   overlay file.
7. **Scroll tuning is preset-only.** Follow factors, velocity, and base offset cannot be tuned
   numerically in-game; non-preset values show as "Custom".
8. **HUD text is world-space.** The mode, layer-status, persistence, and action-legend text
   entities are placed at fixed world positions, so they scroll off-screen as the camera follows
   the player (visible in the Select Tile Mode golden image). Only the tile palette and the
   Phase 0 status line are anchored to the camera's visible rect. Fixing this changes the golden
   image, so pair it with a golden refresh.

### Feature gaps (roadmap)

1. Region tools: rectangle fill, flood fill, marquee copy/stamp.
2. Layer management: delete, reorder, rename, duplicate, collision toggle.
3. Object-layer authoring: imported objects are counted and used for spawn lookup, then discarded.
   They are not rendered, edited, or persisted.
4. Map scope: the source map is hard-coded to `assets/tiled/tilemap_demo.tmj`; no map picker,
   new-map, or resize flow.
5. Export: no Tiled `.tmj` export; the engine exposes import only
   ([TileMapImport.h](../include/vde/api/TileMapImport.h)).
6. Per-tile collision metadata comes only from the imported tileset; no in-game editor for tile
   collision kinds.

### Verification gaps

1. The smoke script is crash-only. Scripted input `assert` supports scene-level fields, but the level
   builder exposes no game-state queries, so edits, undo, and save/load outcomes are unverified at
   runtime.
2. Smoke does not exercise visibility toggle, previous palette, copy, depth down, reverse preset
   cycling, reload failure, or any gamepad-only binding.
3. Render verification covers Select Tile Mode only; no golden for multi-layer depth or parallax.
4. `TilePalette` and `TileCursor` have no unit tests.

### Documentation debt

1. The Summary section of [TILEMAP_LEVEL_BUILDER_IMPLEMENTATION_PLAN.md](TILEMAP_LEVEL_BUILDER_IMPLEMENTATION_PLAN.md)
   still lists Phases 3–6 as remaining.
2. In-game text still says "ground layer" after the multi-layer migration:
   `getGameplaySummary()`, `getControls()` ("editable ground-layer overlay"), the "Using imported
   ground layer." status, and the debug UI "Editable Layer" label. See
   [LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L416).
3. [PROJECT_STATUS.md](PROJECT_STATUS.md) lists the level builder only in the runnable-surface counts;
   it is not described in the tile-based content pipeline row.
4. The overlay file name and format ID (`level_builder_ground.overlay.json`,
   `vde.level_builder.ground_overlay`) reflect the old single-layer scope. Keep them for
   compatibility unless a v3 schema change is made anyway.

---

## Phase Overview

| Phase | Status | Primary outcome |
|-------|--------|-----------------|
| 0 | Complete | Hardening: fix defects, close doc debt, add state assertions for smoke |
| 1 | Not started | Layer Mode submode and complete layer management with history |
| 2 | Not started | Brush and region tools with compound undo |
| 3 | Not started | Object and gameplay-marker authoring, overlay v3 |
| 4 | Not started | Tiled export for the supported subset |
| 5 | Not started | Map scope: picker, new map, resize |
| 6 | Not started | Final verification, docs, and tool-vs-game decision |

Phases 0–2 are the recommended near-term scope. Phases 3–5 each need a design decision before
implementation.

---

## Phase 0: Hardening and Cleanup ✅ COMPLETE

**Goal:** Make the existing workflow safe before adding surface area.

### Outcome

- `addLayer()` enforces the 64-layer and total-tile overlay limits and reports the reason.
- Saves write `<overlay>.tmp` and rename it over the previous file; a failed save keeps the last
  good overlay.
- New layer IDs come from a counter seeded past the highest `layer_<n>` ID; reload rejects
  duplicate IDs.
- Added layers are mirrored into the session-owned `TileMap`, so collision-enabled added layers
  now contribute collision (enabling it in-game is Phase 1 work).
- A camera-anchored status line shows save, reload, layer-creation, undo/redo, and scroll-preset
  results, plus startup overlay load failures.
- Reload (`F9` / `R3`) and quit (`Esc`) need a second press within three seconds when there are
  unsaved changes. Closing the window directly is not guarded.
- "Ground layer" wording is gone from summaries, controls, status strings, and the debug UI;
  the original plan's Summary and [PROJECT_STATUS.md](PROJECT_STATUS.md) are updated.
- Scripted input gained `assert scene "name" state.<key> <op> <value>`, backed by a new
  `Scene::getScriptStateValue()` virtual. The level builder smoke script now asserts layer count,
  active layer, dirty state, undo/redo depth, painted tile IDs, and both guards.

### Tasks

1. **Cap layer creation.** Enforce the same 64-layer and total-tile limits in `addLayer()` as in
   reload, return a failure, and surface the reason in the HUD. Add a unit test that saving at the
   cap reloads successfully.
2. **Atomic save.** Write to `<overlay>.tmp`, flush, then rename over the target. Keep the previous
   file on failure. Unit-test that a failed write leaves the prior overlay intact.
3. **Stable layer IDs.** Generate IDs from a monotonic counter seeded from the maximum existing
   numeric suffix after load. Validate ID uniqueness on reload.
4. **Collision for added layers.** Either extend `m_tileMap` with a real layer for each added
   layer (keeping collision extraction centralized), or extract collision from `LayerDefinition`
   tiles directly. Remove the `index < m_tileMap->getLayerCount()` special cases. Add a unit test
   for collision on an added layer.
5. **HUD status line.** Show `lastPersistenceStatus()` (or a short transient message) in the HUD so
   validation and save/load failures are visible without F1.
6. **Unsaved-changes guard.** When dirty, require a second press within a short window for reload
   and quit, with a HUD prompt. Decide whether `Esc` handling belongs in `BaseGame` or the scene.
7. **Text cleanup.** Replace "ground layer" wording in summaries, controls, status strings, and the
   debug UI. Fix the stale Summary in the original implementation plan and update
   [PROJECT_STATUS.md](PROJECT_STATUS.md).
8. **Smoke state assertions.** Print deterministic state lines, such as layer count, active layer,
   dirty flag, and undo depth, after key actions. Then either assert on them through a
   game-state hook for `assert`, or have `smoke-test.ps1` match expected log lines. Pick the
   smallest mechanism that fits the existing scripted-input design.

### Acceptance Criteria

- No reachable in-game sequence produces an overlay that fails to reload.
- An interrupted save never destroys the previous overlay.
- Validation and persistence errors are visible in the HUD.
- Smoke detects a regression in paint, undo, or save/load, not only crashes.

---

## Phase 1: Layer Mode and Complete Layer Management

**Goal:** Give layer authoring its own submode, and make every layer operation reversible.

### Tasks

1. **Add `LayerMode` as a third Development submode.** Move add/select/visibility/depth/scroll
   actions out of `MoveMode`. This frees Move Mode controller buttons and gives Layer Mode its
   own 11-line legend and binding preset in [Input.h](../games/level_builder/Input.h).
2. **New layer operations** in `TileMapSession`:
   - delete layer (refuse to delete the last layer, or the last collision layer),
   - move layer up/down in stack order (distinct from `depthZ`),
   - duplicate layer,
   - toggle collision participation, enforcing the Gameplay scroll rule; turning collision on
     for a scrolled layer must either reset it to Gameplay or be rejected with a HUD message.
3. **Layer rename.** Controller-first text entry is expensive; first cut is auto-names plus
   rename through the ImGui debug panel. Document that limitation.
4. **Unified history.** Generalize `TileEditRecord` into an edit-command variant covering tile
   edits and layer metadata/structure changes. Undo/redo become available in every Development
   submode, not only Select Tile Mode.
5. **Numeric scroll tuning.** Add fine adjustment for follow factor X/Y and velocity X in Layer
   Mode (for example, D-pad with a modifier), and show values in the HUD. Presets remain the
   quick path.
6. **Active-layer indication while painting.** Dim non-active layers optionally in Select Tile
   Mode so the paint target is obvious.
7. **Camera-anchored HUD.** Anchor the mode, layer-status, persistence, and action-legend text to
   the camera's visible rect (UX gap 8), and refresh the Select Tile Mode golden image.

### Acceptance Criteria

- All layer operations are reachable from keyboard and gamepad without editing files.
- Every layer operation can be undone and redone, and dirty state stays correct after undo back to
  the saved state.
- Deleting or reordering layers never produces duplicate IDs or invalid overlays.

---

## Phase 2: Brush and Region Tools

**Goal:** Make painting practical for level-sized edits.

### Tasks

1. **Erase brush.** Add an explicit empty-tile palette entry or erase action; include
   `kEmptyTile` in palette cycling.
2. **Rectangle selection.** Hold a modifier to anchor a marquee from the current selection; render
   it with the existing cursor style extended to a rectangle.
3. **Region operations:** fill rectangle with brush, erase rectangle, copy rectangle into a
   multi-tile stamp, paste stamp at the cursor.
4. **Flood fill** on the active layer, bounded by map size.
5. **Compound history.** Record a region operation as one undo step. Cap history depth or memory to
   bound growth.
6. **Batch collision rebuild.** Rebuild collision once per compound operation, not per tile.
   Measure rebuild cost on the demo map; consider dirty-region extraction only if needed.
7. **Palette navigation.** 2D palette cursor with page support for tilesets over 256 tiles, and
   a quick "pick from map" action that also moves the palette highlight.

### Acceptance Criteria

- A rectangle fill, flood fill, and stamp paste each undo in one step.
- Region edits on collision layers update collision correctly with a single rebuild.
- Erasing works without first finding an empty cell.

---

## Phase 3: Object and Gameplay-Marker Authoring

**Goal:** Stop discarding imported objects; make spawn and gameplay markers editable.

### Decision required

Choose where objects live:

- Option A: an `objects` array in overlay v3 that replaces imported objects wholesale.
- Option B: overlay v3 stores object deltas keyed by imported object ID plus added objects.

Option A is simpler and matches the existing whole-layer snapshot model; it is recommended.

### Tasks

1. Keep `ImportedTileObject` records in `TileMapSession` instead of only a count.
2. Render objects in Development mode as outlined markers with type labels; hide them in Play mode.
3. Add `ObjectMode` submode: select nearest object, move by tile or sub-tile step, add from a small
   type list (spawn, marker, trigger rect), delete, and resize rectangles.
4. Drive the player spawn from the edited spawn object so Reset honors authored changes.
5. Bump overlay to version 3 with an `objects` array; keep v1 and v2 loading. Add limits on object
   count and property sizes consistent with existing caps.
6. Include object edits in the unified history.

### Acceptance Criteria

- Moving the spawn object and saving changes where Reset places the player after reload.
- v1 and v2 overlays still load unchanged.
- Object edits undo and redo alongside tile and layer edits.

---

## Phase 4: Tiled Export

**Goal:** Round-trip authored content back to Tiled for the supported subset.

### Decision required

Decide whether export belongs in the engine (`vde::TileMapExport` next to `TileMapImport`) or in the
level builder only. Engine placement is recommended if a second consumer is expected, because the
import subset is already defined there.

### Tasks

1. Write finite orthogonal `.tmj` with inline tile data and the single embedded tileset reference,
   matching the subset `TileMapImport` accepts.
2. Map layer metadata: name, visibility, and custom properties for depth, collision, and scroll
   fields; use Tiled `parallaxx` / `parallaxy` for follow factors where semantics match.
3. Export object layers from Phase 3.
4. Round-trip unit tests: import → export → import yields identical tiles, layer metadata, and
   objects.
5. Add an export action in Development mode that writes a separate file; never overwrite the checked-in source map.

### Acceptance Criteria

- An exported map opens in Tiled and re-imports through `TileMapImport` without loss for the
  supported subset.
- Unsupported data is reported, not silently dropped.

---

## Phase 5: Map Scope

**Goal:** Allow authoring beyond the single checked-in demo map.

### Tasks

1. Accept the source map path from the command line or `vde.toml` instead of the hard-coded
   `kImportedMapPath`; derive the overlay name per map.
2. New-map flow: blank map with configurable dimensions using the current tileset.
3. Map resize (grow/shrink with anchor), applied to all layers and objects, recorded as one history
   step or explicitly clearing history.
4. Keep path handling restricted to the asset directory and validate sizes against the existing
   caps.

### Acceptance Criteria

- Two different maps can be authored and saved without overlay collisions.
- Resize preserves existing content within the kept region.

---

## Phase 6: Final Verification, Documentation, and Placement

### Tasks

1. Extend smoke coverage to each new submode, including one gamepad-driven script if scripted
   input supports gamepad events; otherwise document the gap.
2. Add render goldens for multi-layer depth/parallax and for region-selection visuals.
3. Update [../games/level_builder/README.md](../games/level_builder/README.md) controls, overlay
   schema (v3), and limitations.
4. Update [PROJECT_STATUS.md](PROJECT_STATUS.md) tile-pipeline rows.
5. Decide whether the builder moves to `tools/` (editor-first) or stays in `games/` (play-first),
   and whether `examples/tilemap_demo/` is retired or kept as the minimal API sample.
6. Run full verification (build, unit tests, smoke, render verify, lint) and a final review.

---

## Deferred / Out of Scope

- Mouse-driven editing UI beyond the ImGui debug panel.
- Multiple tilesets, external TSX, infinite maps, and flipped/rotated GIDs; these are engine import
  limits and belong to the tile content pipeline work in
  [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md).
- Engine-level per-layer scroll rules inside `vde::TileMap`; revisit only if another game needs it.
- Slopes and authored collision shapes beyond solid/one-way.

---

## Risks and Decision Points

1. **History model generalization** (Phase 1) touches every edit path. Land it before region and
   object tools, or those will need rework.
2. **Controller mapping growth.** Each new submode must keep its legend within the HUD line budget
   and stay in sync with `Input.h`. Consider generating legend text from the binding presets.
3. **Overlay schema churn.** Batch object and any naming changes into one v3 bump rather than
   several.
4. **Collision rebuild cost** grows with region edits and multiple collision layers; measure before
   optimizing.
5. **Tool vs game placement** becomes more pressing as editor features outgrow the play loop.

---

## Recommended Order

1. Phase 0 in full; the defects affect data integrity.
2. Phase 1 unified history, then Layer Mode and layer operations.
3. Phase 2 brush and region tools.
4. Decide object storage, then Phase 3.
5. Decide export placement, then Phase 4.
6. Phase 5 as needed, then Phase 6.
