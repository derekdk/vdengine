# Performance Review

## Findings

### Medium: Per-layer runtime maps multiply render submissions and mesh resources

`LevelBuilderScene` now creates a separate `TileMap` entity for every layer. The baseline rendered one combined tile map. The new path requires one mesh render and draw submission per non-empty visible layer, with independent mesh rebuilds when culling bounds or dirty state changes. Cost grows as `O(layerCount)`; the shipped two-layer map already doubles the tilemap render path, and larger authored maps scale linearly.

**Remediation direction:** Batch layers that share the default transform and split only layers that need independent parallax or scrolling. Measure draw calls and mesh rebuilds with 2, 8, and 32 layers.

### Medium: Single-tile edits copy and validate the entire layer

After paste, undo, or redo, the scene synchronizes the affected runtime layer through `loadLayerFromArray`, which validates and copies every tile even when one coordinate changed. A 128x24 layer copies 3,072 integers, about 12 KiB, per action; a 1024x1024 layer copies about 4 MiB per action. The operation also dirties the runtime mesh.

**Remediation direction:** Use `setTile` for single-cell edits and separate metadata synchronization from full tile-array replacement. Benchmark paint and undo operations at small and large map sizes.

### Medium: Session state retains multiple complete tile-grid snapshots

Each `LayerDefinition` owns its tile vector. The session keeps copies in `m_layers`, `m_savedLayers`, `m_savedLayerTiles`, and `m_importedLayers`; runtime maps add another copy per layer, while the original `TileMap` retains its arrays. For an 8-layer 1024x1024 map, each complete stack is 32 MiB, so the four session-side copies alone are roughly 128 MiB before runtime and transient save allocations. Dirty checks also create temporary full-vector copies through vector comparison.

**Remediation direction:** Keep saved metadata separate from saved tile data, compare existing vectors without creating temporary copies, and retain only the baselines needed for reload and undo. Verify with a resident-memory measurement on a large map.

### Medium: The palette scales as five scene entities per sprite and relayouts every frame

The palette creates one tile sprite plus four selection-frame sprites for every tileset sprite. Its layout loop updates every tile and frame transform each frame, and the camera-dependent update calls it even when the palette is hidden. A 256-sprite sheet therefore creates about 1,280 scene entities, performs roughly 1,280 transform updates per frame, and issues about 256 sprite draws when visible.

**Remediation direction:** Skip layout while hidden, cache layout until the viewport or tileset changes, and use a batched or virtualized palette. Compare CPU time and draw calls for 5, 128, and 512 sprites.

### Medium: Adding one layer rebuilds every existing runtime layer

Adding a layer calls `rebuildLayerRuntimes`, which clears and recreates all runtime maps. Each recreation performs a full layer copy and allocates a new mesh/resource path. Adding layers sequentially therefore performs `O(layerCount^2 * width * height)` copying and repeated mesh/resource allocation, causing avoidable editor stalls on large maps. Reload uses the same rebuild path.

**Remediation direction:** Preserve existing runtimes and append or update only the affected layer. Measure repeated layer creation on a large map.

### Low: Move mode rebuilds HUD strings and action lists every frame

Move mode reconstructs an 11-entry `std::vector<std::string>` and formats a new layer-status string every frame. This causes recurring allocation, comparison, and setter work even though `TextEntity::setText` avoids rebuilding its texture when the value is unchanged.

**Remediation direction:** Cache the last layer and mode state and update HUD entities only when that state changes. This is non-blocking unless profiling shows it consumes a meaningful part of the frame budget.

## Focused Summary

The branch has medium performance risk across CPU, GPU, and memory domains. GPU work grows with layer count because the renderer now owns one mesh and render submission per layer. CPU edit cost grows with full-map copies, dirty scans, and runtime reconstruction. Memory grows through several complete tile-grid snapshots. Save/reload I/O grows with the serialized layer stack, which is expected for multi-layer persistence.

The shipped workload is small at 128x24 with two tile layers and five sprites, so the immediate regression is modest. The implementation does not scale well to larger maps or tilesets. A small layer-count benchmark and a large-map edit/memory benchmark should be run before merge. The HUD allocation issue is not merge-blocking.

## Review Scope

- Base: `origin/main`
- Merge base: `f1b513fea0f1bd35883b6074c7240d8c887e7233`
- Committed range: `f1b513fea0f1bd35883b6074c7240d8c887e7233..HEAD`
- Branch: `user/derek/tilemap-lvleditor`
- Worktree: clean, with no staged, unstaged, or untracked files
- Validation: read-only static review; no builds, tests, linters, or profiler runs were performed
