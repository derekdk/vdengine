# Overall Issues Summary

This document consolidates every finding from the four reviews in this folder and ranks the recommended work order across the branch.

## Tracking

The `Status` column is the source of truth for remediation progress. Use these values consistently:

| Status | Meaning |
| :--- | :--- |
| Open | No fix has been started or recorded. |
| In progress | Work is underway, but the issue is not fully addressed. |
| Fixed | The implementation is complete; validation is still pending or incomplete. |
| Verified | The implementation is complete and the listed validation has passed. |
| Won't fix | The issue was reviewed and intentionally left unresolved; record the rationale in the evidence column. |

When changing a status, add the relevant commit, pull request, test, benchmark, smoke test, render check, or validation result to `Fix / verification evidence`.

## Prioritization

Rank 1 is the first issue to address. The ranking combines consequence, likelihood during normal editor use, exposure to untrusted Tiled or overlay files, and the breadth of the fix. The original review severity is preserved in the table and is not replaced by the cross-review priority.

- **P0 - Merge blocker:** Can terminate the process, invalidate GPU lifetime, or create a broad correctness failure. Fix before normal rollout. Security items in this band assume project assets or sidecar overlays may be untrusted.
- **P1 - High priority:** Deterministic user-visible correctness problems, architectural drift, or scalability failures that should follow immediately after the blockers.
- **P2 - Hardening:** Lower-probability input safety or regression-protection work.
- **P3 - Non-blocking:** Documentation, polish, or small allocation improvements.

## Ranked Issues

| Rank | Status | Priority | Issue | Original severity | Source | Recommended next move | Fix / verification evidence |
| ---: | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | Open | P0 | **Runtime layer rebuild destroys GPU meshes without retirement.** Rebuilding after rendering can destroy vertex and index buffers while an earlier command buffer still references them, causing Vulkan validation failures or device-dependent rendering failures. | High | [Graphics review](graphics-review.md) | Retire old runtime meshes and entities only after the relevant GPU fences complete. Verify F9 and layer creation after rendering with validation enabled. | - |
| 2 | Open | P0 | **Layer state has competing mutable owners and caller-managed synchronization.** `TileMapSession`, its mutable base `TileMap`, and `LevelBuilderScene` can diverge, bypassing undo, dirty tracking, persistence, or runtime updates. | Medium | [Code organization review](code-organization-review.md) | Choose one authoritative model and one explicit runtime lifecycle. Make `tileMap()` read-only or route all mutation through the session, then make synchronization an owned boundary. | - |
| 3 | Open | P0 | **Rebuilding layers corrupts sprite render order.** Swap-removing old tilemaps can move cursor entities ahead of opaque tilemaps, causing cursor edges to render underneath the map after reload or layer creation. | Medium | [Graphics review](graphics-review.md) | Replace runtime tilemaps without swap-removing unrelated entities, or preserve and restore the complete entity order. Add a reload/layer-add render check. | - |
| 4 | Open | P0 | **Invalid tile IDs in extra overlay layers cause uncaught process termination.** A malformed automatically loaded overlay can survive parsing and then throw during runtime-map construction on startup or F9 reload. | Medium | [Security review](security-review.md) | Validate every overlay tile ID before committing parsed state, reject the overlay atomically, and prevent runtime construction from being the first validation boundary. | - |
| 5 | Open | P0 | **Inconsistent tileset metadata can exhaust memory during palette creation.** Crafted image and grid metadata can produce more than one million sprite records and millions of palette entities, leading to severe startup stalls, `bad_alloc`, or process termination. | Medium | [Security review](security-review.md) | Restore consistency checks between map tiles, tileset dimensions, spacing, and image dimensions. Enforce a practical sprite-count limit or virtualize the palette. | - |
| 6 | Open | P0 | **Version-2 overlays have no layer or resource budget.** A syntactically valid sidecar can contain excessive full-size layers, causing large parse/rebuild allocations, editor stalls, or memory exhaustion. | Medium | [Security review](security-review.md) | Bound file size, layer count, total serialized tiles, layer-name length, and total runtime entities before allocating or rebuilding. | - |
| 7 | Open | P1 | **Parallax and scrolling desynchronize visible layers from collision geometry.** Rendered layers receive camera and scroll offsets while collision rectangles remain tied to the unshifted backing map; collision-enabled ground can also receive non-gameplay scroll presets. | Medium | [Graphics review](graphics-review.md) | Define one transform contract. Restrict parallax to non-collision layers or apply matching transforms to collision data, then test visible solids against player collision. | - |
| 8 | Open | P1 | **Tile selection remains anchored to the base map instead of the active layer.** The cursor and camera-follow position ignore active-layer camera-relative and scroll offsets. | Medium | [Graphics review](graphics-review.md) | Calculate selection coordinates from the active runtime layer transform, including camera and accumulated scroll offsets. Test selection and painting on shifted layers. | - |
| 9 | Open | P1 | **Nonzero Tiled layer offsets are silently ignored.** Tile and object layer offsets are accepted but not applied, so imported tiles render at the wrong origin and object-driven spawn positions are displaced. | Medium | [Graphics review](graphics-review.md) | Apply offsets during import, or restore explicit rejection until offset support is implemented. Add fixtures for nonzero tile and object layer offsets. | - |
| 10 | Open | P1 | **Adding one layer rebuilds every existing runtime layer.** Sequential layer creation performs quadratic copying and repeated mesh/resource allocation, creating avoidable large-map editor stalls; reload uses the same path. | Medium | [Performance review](performance-review.md) | Preserve existing runtimes and append or update only the affected layer. Measure repeated layer creation on a large map after the lifecycle fix. | - |
| 11 | Open | P1 | **Session state retains multiple complete tile-grid snapshots.** `m_layers`, saved state, imported state, runtime maps, and the original `TileMap` duplicate large grids, while dirty checks can create further temporary copies. | Medium | [Performance review](performance-review.md) | Separate saved metadata from tile baselines, compare existing vectors without copying, and retain only the baselines required for reload and undo. Measure resident memory on a large map. | - |
| 12 | Open | P1 | **Per-layer runtime maps multiply render submissions and mesh resources.** Each visible non-empty layer now costs an independent mesh render and draw submission, with work growing linearly by layer count. | Medium | [Performance review](performance-review.md) | Batch layers sharing the default transform and split only layers needing independent parallax or scrolling. Measure draw calls and mesh rebuilds at 2, 8, and 32 layers. | - |
| 13 | Open | P1 | **Single-tile edits copy and validate the entire layer.** Paste, undo, and redo use full-array replacement even for one changed coordinate, making edit cost proportional to the complete layer size. | Medium | [Performance review](performance-review.md) | Use `setTile` for single-cell changes and reserve full-array replacement for actual bulk edits. Benchmark paint and undo at small and large map sizes. | - |
| 14 | Open | P1 | **The palette scales as five scene entities per sprite and relayouts every frame.** Hidden palettes still run camera-dependent layout work, and a 256-sprite sheet creates about 1,280 entities. | Medium | [Performance review](performance-review.md) | Skip hidden layout, cache layout until viewport or tileset changes, and use batching or virtualization. Measure CPU time and draw calls at 5, 128, and 512 sprites. | - |
| 15 | Open | P2 | **Removed map-pixel overflow validation leaves attacker-controlled signed overflow.** Large positive Tiled dimensions can overflow the map-pixel multiplication and produce undefined behavior or incorrect bounds. | Low | [Security review](security-review.md) | Restore checked multiplication with a wider intermediate or explicit division checks before processing dimensions. | - |
| 16 | Open | P2 | **The render-verification gate was weakened for the new UI surface.** Raising the FLIP threshold from `0.06` to `0.08` can allow meaningful palette or overlay regressions to pass. | Low | [Graphics review](graphics-review.md) | Retain the documented threshold unless cross-device data justifies the change, or add focused assertions for palette placement and occlusion. | - |
| 17 | Open | P2 | **The palette can cover the action legend at reachable camera positions.** The palette is camera-anchored while the legend remains at fixed world coordinates, allowing overlap in Select Tile mode. | Low | [Graphics review](graphics-review.md) | Put both elements in a shared screen-space layout or reserve a non-overlapping HUD region, then verify at the stated camera positions. | - |
| 18 | Open | P3 | **Move mode rebuilds HUD strings and action lists every frame.** An 11-entry vector and a layer-status string are reconstructed each frame despite unchanged state. | Low | [Performance review](performance-review.md) | Cache the last layer and mode state and update HUD entities only when that state changes. Confirm with profiling before optimizing further. | - |
| 19 | Open | P3 | **The public Scene ordering API is missing from the API reference.** `Scene::moveEntityToBack()` is not documented in the entity-management reference or reflected in canonical project status. | Low | [Code organization review](code-organization-review.md) | Add the method to `docs/API.md` and update `docs/PROJECT_STATUS.md` with the public behavior evidence. | - |

## Recommended Work Batches

The numerical ranking is the issue order; these batches show the most efficient implementation grouping.

1. **Stabilize runtime rebuilds and ownership:** ranks 1-3, then rank 10. Establish the authoritative session/runtime boundary first, implement fence-safe retirement and order-preserving replacement, and remove whole-stack rebuilds where possible.
2. **Harden import and overlay boundaries:** ranks 4-6 and 15. Validate dimensions, tile IDs, offsets, checked arithmetic, and resource budgets before parsed state is committed or runtime entities are created.
3. **Repair coordinate contracts:** ranks 7-9. Make rendering, collision, selection, and imported layer/object positions use consistent transforms.
4. **Measure and reduce scaling costs:** ranks 11-14. Benchmark memory, edit latency, draw calls, mesh rebuilds, and palette layout before and after the targeted changes.
5. **Close regression and documentation gaps:** ranks 16-19. Restore visual-gate sensitivity, fix HUD overlap and recurring allocations, and document the public ordering API.

## Coverage Check

| Review | Findings included |
| :--- | ---: |
| [Graphics review](graphics-review.md) | 7 |
| [Performance review](performance-review.md) | 6 |
| [Security review](security-review.md) | 4 |
| [Code organization review](code-organization-review.md) | 2 |
| **Total** | **19** |

## Validation Note

This is a consolidation of the existing static reviews. All four source reviews state that builds, tests, linters, profiling, smoke tests, render verification, and Vulkan validation were not run. The rankings should therefore be treated as a work-order proposal until the P0 items are reproduced and verified with executable checks.