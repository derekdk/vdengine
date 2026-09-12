# Graphics Review

## Findings

### High: Runtime layer rebuild destroys GPU meshes without retirement

`LevelBuilderScene` removes each rendered `TileMap` during a layer rebuild and then clears the last owning pointers. The associated `TileMap` meshes can therefore be destroyed immediately even though the previous frame's command buffer may still reference their vertex and index buffers. Pressing `F9` after rendering, or adding a layer with `N`, can trigger Vulkan validation errors or device-dependent rendering failures.

**Remediation direction:** Defer destruction or retire each runtime mesh/entity until the relevant GPU fences have completed.

### Medium: Rebuilding layers corrupts sprite render order

Rebuilding the runtime layers moves only the replacement tilemaps to the front after `removeEntity` has swap-removed the old entries. The swap removal can move the final entities, including the four cursor segments created at the end of `onEnter`, into earlier positions. Because the sprite pipeline relies on draw order rather than depth, reloading or rebuilding layers can make cursor edges render underneath opaque tilemap meshes.

**Remediation direction:** Preserve the original entity order or replace runtime tilemaps without swap-removing unrelated entities.

### Medium: Parallax and scrolling desynchronize visible layers from collision geometry

`TileMapSession::runtimeLayerPosition` applies camera and scroll offsets to rendered runtime tilemaps, while collision rectangles are extracted from the unshifted backing map. The scroll-preset controls can apply non-gameplay presets to the collision-enabled ground layer. This produces visible tiles displaced from their collision surfaces, including invisible walls or the player falling through apparently solid tiles.

**Remediation direction:** Restrict parallax to non-collision layers or apply a consistent transform policy to collision data.

### Medium: Tile selection remains anchored to the base map instead of the active layer

`LevelBuilderScene::updateSelectTileUi` places the selection cursor using `TileMapSession::tileCenterWorld`, which uses the backing map position, while the active layer may be shifted by camera-relative or runtime scroll offsets. Selecting or painting a parallax or scrolling layer therefore shows the cursor away from the visible tile and can make the camera follow the wrong location.

**Remediation direction:** Compute selection coordinates using the active runtime layer transform, including camera and accumulated scroll offsets.

### Medium: Nonzero Tiled layer offsets are silently ignored

The branch removes the former `x/y` rejection from the tile-layer importer and proceeds with normal data import without applying those offsets to tile coordinates. The same guard is absent for object layers. A valid finite map with nonzero layer offsets now imports successfully but renders tile data at the origin; object-driven spawn positions are also displaced.

**Remediation direction:** Apply layer offsets during import or retain the previous explicit rejection for unsupported offsets.

### Low: The palette can cover the action legend at reachable camera positions

The new palette anchors its opaque panel to the camera's top-right, while the action legend remains fixed at world coordinates. In Development Select Tile mode, moving the free camera toward the map's lower-left area can bring those regions into overlap. The palette is added after the HUD and sprites have no depth test, so the panel can obscure control lines.

**Remediation direction:** Use a shared screen-space layout or reserve a non-overlapping HUD region.

### Low: The render-verification gate was weakened for the new UI surface

The branch raises the FLIP threshold from `0.06` to `0.08` while adding the palette and other overlay changes. Regressions in that interval can now pass, including meaningful UI occlusion or placement errors.

**Remediation direction:** Retain the documented UI threshold unless cross-device measurements justify the increase, or add focused checks for the new overlay.

## Focused Summary

Graphics correctness risk is high. The affected stages are runtime `TileMap` mesh lifetime and draw ordering, camera-dependent layer transforms, collision and selection scene coordinates, Tiled-to-UV/tile placement import, and the new sprite-based editor overlay. The mesh lifetime failure is validation-layer and device sensitive; the transform, cursor, importer, and UI issues are deterministic under the stated scenes. Visual and Vulkan validation evidence should block merge until the runtime rebuild and transform contracts are corrected. The relaxed render threshold adds residual risk because the new palette is less strongly protected by automated visual comparison.

## Review Scope

- Base: `origin/main`
- Merge base: `3d3f09d2bfd42da0eecc7923a24725857773b02e`
- Committed range: `origin/main...HEAD`
- Branch: `user/derek/tilemap-lvleditor`
- Reviewed tip: `62c77ac514cdc325d1f64f6d5f2fc8735ef2840b`
- Branch commits: 11
- Committed changes: 31 files, including the level-builder render path, tile import, scene ordering, palette, smoke scripts, and render-verification configuration
- Local worktree state: one unrelated unstaged edit to `.github/skills/branch-review/SKILL.md`; it was excluded from the review
- Validation: read-only static review; builds, tests, smoke tests, render verification, and Vulkan validation layers were not run
