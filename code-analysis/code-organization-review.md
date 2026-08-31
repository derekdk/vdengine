# Code Organization Review

## Findings

### Medium: Layer state has competing mutable owners and caller-managed synchronization

[games/level_builder/TileMapSession.h](../games/level_builder/TileMapSession.h#L79-L96) adds a new `m_layers` model while [TileMapSession.h](../games/level_builder/TileMapSession.h#L50) continues to expose a mutable `shared_ptr<TileMap>`. The session now maintains parallel layer/tile state in [TileMapSession.h](../games/level_builder/TileMapSession.h#L125-L141), manually synchronizes both representations in [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L712-L727), and creates runtime clones in [TileMapSession.cpp](../games/level_builder/TileMapSession.cpp#L458-L500). The live runtime list and its scroll state are separately owned by [LevelBuilderScene.h](../games/level_builder/LevelBuilderScene.h#L34-L38).

A caller can mutate `session.tileMap()` directly with `setTile()` or `addLayer()` and bypass undo history, dirty tracking, persistence, and the `m_layers` data used for rendering. Separately, session mutations such as layer creation, reload, visibility, and depth changes require paired calls from [LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L758-L788); a new mutation path that misses that pairing leaves the runtime layer stack stale. This differs from the branch's stated architecture, which says the runtime stack should be owned by `TileMapSession` and mounted by `LevelBuilderScene` in [docs/TILEMAP_LEVEL_BUILDER_MULTI_LAYER_PLAN.md](../docs/TILEMAP_LEVEL_BUILDER_MULTI_LAYER_PLAN.md#L47-L58).

**Remediation direction:** Consolidate the authoritative layer model and runtime ownership, or make the session a pure model with a dedicated scene-side runtime adapter. In either case, narrow `tileMap()` to read-only access or route all mutations through the session, and make runtime synchronization an explicit ownership/lifecycle boundary rather than a convention every caller must remember.

### Low: The new public Scene ordering API is missing from the API reference

The branch adds `Scene::moveEntityToBack()` in [include/vde/api/Scene.h](../include/vde/api/Scene.h#L394-L399) and uses it to establish level-layer ordering in [games/level_builder/LevelBuilderScene.cpp](../games/level_builder/LevelBuilderScene.cpp#L758-L775). The focused entity-management table in [docs/API.md](../docs/API.md#L575-L585) does not list the method, and [docs/PROJECT_STATUS.md](../docs/PROJECT_STATUS.md#L94-L96) was not updated despite requiring canonical status updates for public behavior changes.

Contributors looking for scene ordering support will not discover the generic API and may reimplement ordering locally. Add the method to the entity-management reference and update the canonical status evidence for the new Scene behavior.

## Focused Summary

The branch is mostly coherent in module placement: level-builder authoring remains game-local, generic Scene scheduling/order behavior lives in the engine, and tests/CMake/smoke metadata are colocated with their behavior. The highest-risk organizational theme is the absence of a single owner for authorable layer state and runtime layer entities. That creates a concrete divergence path between the mutable base `TileMap`, the session model, and rendered runtime clones.

The ownership issue should block merge until the synchronization/lifetime boundary is made explicit. The API documentation gap is non-blocking.

## Review Scope

- Base: `origin/main`
- Merge base: `3d3f09d2bfd42da0eecc7923a24725857773b02e`
- Committed range: `origin/main...HEAD`
- Branch commits: 11
- Committed changes: 31 files, 2,795 insertions, 348 deletions
- Local worktree: one unstaged edit to [.github/skills/branch-review/SKILL.md](../.github/skills/branch-review/SKILL.md), reviewed separately and not included in the product findings; no staged or relevant untracked files

Generated build output and binary render assets were excluded from structural assessment. Per the shared review workflow, no builds, tests, linters, or other executable validation were run.
