# Security Review

## Findings

### Medium: Inconsistent tileset metadata can exhaust memory during palette creation

[src/api/TileMapImport.cpp](../src/api/TileMapImport.cpp#L315) accepts image metadata and passes the declared grid to `SpriteSheet::createGrid` at [src/api/TileMapImport.cpp](../src/api/TileMapImport.cpp#L578). The new palette then creates one tile entity plus four selection entities for every sprite at [games/level_builder/TilePalette.cpp](../games/level_builder/TilePalette.cpp#L170).

An attacker who supplies a crafted Tiled map and tileset image can declare, for example, a 1024x1024 tile grid over a 1024x1024 image while declaring 16-pixel map tiles. The branch removed the checks that previously rejected this inconsistency. Importing the map creates over one million sprite records, followed by millions of scene entities, likely causing severe startup delay, `bad_alloc`, or process termination. This is an availability issue at the Tiled asset to engine-resource boundary.

**Remediation direction:** Restore validation tying map tile dimensions, tileset dimensions, spacing, and image dimensions together, and enforce a practical maximum sprite count or virtualize the palette instead of instantiating every tile.

### Medium: Invalid tile IDs in extra overlay layers cause an uncaught process termination

[games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L93) accepts any integer in v2 overlay tile arrays. For layers beyond the imported map's native layer count, validation is deferred until [games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L496), where `loadLayerFromArray` throws for an out-of-range tile ID. The runtime rebuild loop at [games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L762) does not catch that exception.

An attacker who can place or modify `level_builder_ground.overlay.json` beside the executable can add an authorable layer containing an invalid tile ID. Overlay reload succeeds because extra layers are stored only in the session model; startup or F9 reload then creates the runtime map and terminates the application when validation throws. Impact is availability only, but the overlay is an automatically loaded file boundary.

**Remediation direction:** Validate every tile ID against the active tileset before committing parsed overlay state, including extra layers, and reject the overlay atomically rather than allowing runtime construction to perform deferred validation.

### Medium: Version-2 overlays have no layer or resource budget

The parser reserves and processes every serialized layer at [games/level_builder/TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L254), while [games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L761) creates a separate runtime tile map and scene entities for each one.

A crafted overlay can contain an excessive number of valid layers, each with a full map-sized tile array. The loader duplicates tile data while parsing and rebuilding, then performs repeated entity reordering. A sufficiently large but syntactically valid overlay can exhaust memory or stall the editor. Required capability is write access to the sidecar overlay file; impact is availability.

**Remediation direction:** Apply limits to overlay file size, layer count, total serialized tiles, layer-name length, and total runtime entities before allocating. Avoid rebuilding one runtime entity per unbounded serialized layer without a budget.

### Low: Removed map-pixel overflow validation leaves attacker-controlled signed overflow

[src/api/TileMapImport.cpp](../src/api/TileMapImport.cpp#L583) now multiplies two positive, attacker-controlled `int` values without the guard that was removed by the branch.

A Tiled file with dimensions such as `width = 50000` and `tilewidth = 50000` can trigger signed integer overflow with a relatively manageable tile array. The result feeds object-bound calculations and can produce undefined behavior or incorrect bounds, with possible importer termination when the caller does not catch the resulting failure. This is a defense-in-depth availability and input-validation issue, not demonstrated arbitrary memory corruption.

**Remediation direction:** Restore checked multiplication, preferably using a wider intermediate or explicit division checks, before constructing or processing the imported map.

## Focused Summary

Security risk level: **Medium**. The reviewed trust boundaries are Tiled JSON and tileset assets into native engine resources, the automatically loaded overlay JSON into runtime tile maps and scene entities, and smoke-test metadata into file cleanup and process execution. No confidentiality or integrity compromise was found, but the branch introduces multiple input-triggered availability failures. Findings 1 through 3 should block merge when project assets or overlay files may be untrusted; finding 4 is a lower-severity hardening issue.

## Review Scope

- Base: `origin/main`
- Merge base: `3d3f09d2bfd42da0eecc7923a24725857773b02e`
- Committed range: `origin/main...HEAD`
- Branch: `user/derek/tilemap-lvleditor`
- Reviewed tip: `62c77ac514cdc325d1f64f6d5f2fc8735ef2840b`
- Branch commits: 11
- Committed changes: 31 files, including the level-builder render path, tile import, scene ordering, palette, smoke scripts, and render-verification configuration
- Local worktree state: one unrelated unstaged edit to `.github/skills/branch-review/SKILL.md`; it was excluded from the review
- Validation: read-only static review; no builds, tests, linters, or other executable validation were run
- Exclusion: the pre-existing tileset image path join was inspected but not reported because the branch preserves that behavior
