# VDE Project Status

Last updated: 2026-07-01

This is the canonical status document for VDE. It answers three questions clearly:

- what is implemented
- what is partial
- what is still planned

When this document disagrees with older planning or status notes, trust this file and the current public code under [../include/vde/](../include/vde/) and [../include/vde/api/](../include/vde/api/).

## Summary

VDE is already a small game engine and prototyping framework, not just a thin Vulkan wrapper. The project is strongest in code-driven runtime features, rendering foundations, audio, text, transitions, examples, tests, and verification tooling.

The main gaps are workflow-completeness items rather than missing primitives in isolation: tile and scene authoring/import, input rebinding/buffering ergonomics, physics filtering and scaling, advanced Camera2D constraints, and a game-facing UI/data pipeline.

## Status Meanings

- **Implemented**: public API, build target, tool, or workflow exists today.
- **Partial**: usable today, but an expected workflow, ergonomics layer, or scaling feature is still missing.
- **Planned**: described in design or planning docs, but not exposed as public code yet.

## Implemented Today

| Area | Status | Evidence | Notes |
|------|--------|----------|-------|
| Engine layering | Implemented | [ARCHITECTURE.md](ARCHITECTURE.md), [../include/vde/Core.h](../include/vde/Core.h), [../include/vde/api/GameAPI.h](../include/vde/api/GameAPI.h) | Clear split between low-level Vulkan helpers and the game-facing API. |
| Core game framework | Implemented | [../include/vde/api/Game.h](../include/vde/api/Game.h), [../include/vde/api/Scene.h](../include/vde/api/Scene.h), [../include/vde/api/SceneGroup.h](../include/vde/api/SceneGroup.h), [../include/vde/api/GameSettings.h](../include/vde/api/GameSettings.h) | Scene management, lifecycle, multi-scene groups, per-scene viewports, and game settings are all public. |
| Entity and rendering foundation | Implemented | [../include/vde/api/Entity.h](../include/vde/api/Entity.h), [../include/vde/api/Mesh.h](../include/vde/api/Mesh.h), [../include/vde/api/Material.h](../include/vde/api/Material.h), [../include/vde/api/LightBox.h](../include/vde/api/LightBox.h) | Mesh and sprite entities, materials, transforms, colors, and lighting are part of the public gameplay layer. |
| 2D sprite foundations | Implemented | [../include/vde/api/Entity.h](../include/vde/api/Entity.h), [../include/vde/api/SpriteSheet.h](../include/vde/api/SpriteSheet.h), [../include/vde/api/GameCamera.h](../include/vde/api/GameCamera.h) | Sprite UV regions, anchor points, flipping, SpriteSheet atlas helpers, and Camera2D are public and used by examples. |
| TileMap runtime workflow | Implemented | [../include/vde/api/TileMap.h](../include/vde/api/TileMap.h), [../examples/tilemap_demo/](../examples/tilemap_demo/), [../tests/TileMap_test.cpp](../tests/TileMap_test.cpp) | Layered tile grids, SpriteSheet binding, camera-visible culling, merged solid/one-way collision extraction, and repeating parallax backgrounds are now part of the public 2D workflow. |
| Text stack | Implemented | [../include/vde/api/TextEntity.h](../include/vde/api/TextEntity.h), [../include/vde/api/BitmapFont.h](../include/vde/api/BitmapFont.h), [../include/vde/api/TrueTypeFont.h](../include/vde/api/TrueTypeFont.h), [../include/vde/api/EmojiFont.h](../include/vde/api/EmojiFont.h), [../include/vde/api/TextRenderer.h](../include/vde/api/TextRenderer.h) | Bitmap, TrueType, and emoji rendering are all public and tested. |
| Generic animation and tweening | Implemented | [../include/vde/api/Animator.h](../include/vde/api/Animator.h), [../include/vde/api/AnimatorImpl.h](../include/vde/api/AnimatorImpl.h), [../include/vde/Tween.h](../include/vde/Tween.h), [../examples/animation_demo/](../examples/animation_demo/) | Scene-owned animation, easing, loop/ping-pong playback, and animated property updates are implemented. |
| Scene transitions | Implemented | [../include/vde/api/Transition.h](../include/vde/api/Transition.h), [../include/vde/api/FadeTransition.h](../include/vde/api/FadeTransition.h), [../include/vde/api/WipeTransition.h](../include/vde/api/WipeTransition.h), [../include/vde/api/CircleRevealTransition.h](../include/vde/api/CircleRevealTransition.h), [../include/vde/api/BlockFallTransition.h](../include/vde/api/BlockFallTransition.h), [../include/vde/api/TransitionManager.h](../include/vde/api/TransitionManager.h) | Built-in shader-driven transitions are public and no longer merely planned. |
| Physics foundation | Implemented | [../include/vde/api/PhysicsScene.h](../include/vde/api/PhysicsScene.h), [../include/vde/api/PhysicsEntity.h](../include/vde/api/PhysicsEntity.h), [../include/vde/api/PhysicsTypes.h](../include/vde/api/PhysicsTypes.h) | Fixed-step 2D physics, body definitions, collision callbacks, raycasts, and query APIs are implemented. |
| Audio | Implemented | [../include/vde/api/AudioManager.h](../include/vde/api/AudioManager.h), [../include/vde/api/AudioSource.h](../include/vde/api/AudioSource.h), [../include/vde/api/AudioClip.h](../include/vde/api/AudioClip.h) | Cross-platform audio playback and scene-level audio workflows are public. |
| Input handling and automation | Implemented | [../include/vde/api/InputHandler.h](../include/vde/api/InputHandler.h), [../include/vde/api/InputActionMap.h](../include/vde/api/InputActionMap.h), [../include/vde/api/KeyStateTracker.h](../include/vde/api/KeyStateTracker.h), [../include/vde/api/InputScript.h](../include/vde/api/InputScript.h), [../include/vde/api/InputScriptExecutor.h](../include/vde/api/InputScriptExecutor.h), [../examples/input_actions_demo/](../examples/input_actions_demo/) | Raw input handling, lightweight key tracking, full named action mapping with persistence, and scripted input playback are part of the API. |
| Resources and persistence | Implemented | [../include/vde/api/ResourceManager.h](../include/vde/api/ResourceManager.h), [../include/vde/api/StorageManager.h](../include/vde/api/StorageManager.h) | Resource caching, cross-scene persistence, and SQLite-backed key-value storage are public. |
| Scheduling and utilities | Implemented | [../include/vde/api/Scheduler.h](../include/vde/api/Scheduler.h), [../include/vde/api/ThreadPool.h](../include/vde/api/ThreadPool.h), [../include/vde/api/TimedEvents.h](../include/vde/api/TimedEvents.h), [../include/vde/api/Timing.h](../include/vde/api/Timing.h), [../include/vde/api/Random.h](../include/vde/api/Random.h), [../include/vde/PlaybackClock.h](../include/vde/PlaybackClock.h) | Timing, scheduling, random utilities, and threaded execution are exposed and covered by tests. |
| Low-level Vulkan layer | Implemented | [../include/vde/Window.h](../include/vde/Window.h), [../include/vde/VulkanContext.h](../include/vde/VulkanContext.h), [../include/vde/Texture.h](../include/vde/Texture.h), [../include/vde/ShaderCache.h](../include/vde/ShaderCache.h), [../include/vde/BufferUtils.h](../include/vde/BufferUtils.h), [../include/vde/DescriptorManager.h](../include/vde/DescriptorManager.h), [../include/vde/OffscreenRenderTarget.h](../include/vde/OffscreenRenderTarget.h), [../include/vde/HexGeometry.h](../include/vde/HexGeometry.h), [../include/vde/HexPrismMesh.h](../include/vde/HexPrismMesh.h) | The low-level rendering surface remains available for advanced direct control. |
| Build, verification, and scaffolding workflows | Implemented | [../scripts/](../scripts/), [../scripts/build.ps1](../scripts/build.ps1), [../scripts/test.ps1](../scripts/test.ps1), [../scripts/smoke-test.ps1](../scripts/smoke-test.ps1), [../scripts/render-verify.ps1](../scripts/render-verify.ps1), [../scripts/verify.ps1](../scripts/verify.ps1), [../scripts/lint.ps1](../scripts/lint.ps1), [../scripts/new-example.ps1](../scripts/new-example.ps1), [../scripts/new-game.ps1](../scripts/new-game.ps1), [../scripts/new-tool.ps1](../scripts/new-tool.ps1) | Verification discipline is one of VDE's strongest areas. |

## Partial Today

| Area | What exists now | What is still missing | Evidence |
|------|-----------------|-----------------------|----------|
| Camera2D ergonomics | Positioning, zoom, rotation, viewport sizing, visible-rect calculation, orthographic application, `followTarget()`, deadzone, look-ahead, shake, and smooth zoom are now public APIs | No built-in camera bounds/rails workflow or higher-level composition presets yet | [../include/vde/api/GameCamera.h](../include/vde/api/GameCamera.h), [../examples/camera_feel_demo/](../examples/camera_feel_demo/), [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md) |
| Tile-based content pipeline | `TileMap` and `RepeatingBackground` now cover runtime tile rendering, layered grids, culling, and extracted solid/one-way collision spans | No checked-in Tiled import path, CSV/data loader workflow, or richer collision metadata like slopes yet | [../include/vde/api/TileMap.h](../include/vde/api/TileMap.h), [../examples/tilemap_demo/](../examples/tilemap_demo/), [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md) |
| 2D animation workflow | SpriteSheet atlas support, the generic scene-owned Animator service, `SpriteAnimation` clips, `AnimatedSpriteEntity` with named playback states and frame events, transition-condition helpers, and Aseprite-style JSON clip import | No authored transition editor, blend-graph tooling, or broader multi-tool import pipeline yet | [../include/vde/api/SpriteSheet.h](../include/vde/api/SpriteSheet.h), [../include/vde/api/SpriteAnimation.h](../include/vde/api/SpriteAnimation.h), [../include/vde/api/AnimatedSpriteEntity.h](../include/vde/api/AnimatedSpriteEntity.h), [../include/vde/api/SpriteAnimationImport.h](../include/vde/api/SpriteAnimationImport.h), [../examples/sidescroller/](../examples/sidescroller/), [../examples/sprite_demo/](../examples/sprite_demo/) |
| Input abstraction | Raw input events, `KeyStateTracker` named held/one-shot bindings, and `InputActionMap` pressed/held/released actions with saved bindings | No built-in rebinding UI, buffered/fighting-game input semantics, or higher-level analog intent helpers beyond thresholded axis bindings | [../include/vde/api/InputHandler.h](../include/vde/api/InputHandler.h), [../include/vde/api/InputActionMap.h](../include/vde/api/InputActionMap.h), [../include/vde/api/KeyStateTracker.h](../include/vde/api/KeyStateTracker.h), [../examples/input_actions_demo/](../examples/input_actions_demo/), [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md) |
| Physics selectivity and scale | Body simulation, callbacks, raycasts, and AABB query support | No collision layers/filter masks and no public broadphase partitioning workflow for larger scenes | [../include/vde/api/PhysicsScene.h](../include/vde/api/PhysicsScene.h), [TOP_FIVE_ENGINE_FEATURES_PLAN.md](TOP_FIVE_ENGINE_FEATURES_PLAN.md), [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md) |
| Content pipeline | Resource caching, storage, launcher/editor tools, and many runnable examples | Scenes are still primarily authored in C++; there is no general scene serialization or level-loading pipeline | [../tools/](../tools/), [../include/vde/api/ResourceManager.h](../include/vde/api/ResourceManager.h), [../include/vde/api/StorageManager.h](../include/vde/api/StorageManager.h), [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) |
| External packaging | Install rules for the library and public headers exist | The root CMake explicitly notes that exported package config support is not provided yet | [../CMakeLists.txt](../CMakeLists.txt), [OPEN_SUGGESTIONS.md](OPEN_SUGGESTIONS.md) |

## Planned or Not Yet Implemented

### High-priority workflow gaps

| Area | Why it matters | Current source of truth |
|------|----------------|-------------------------|
| Tiled import and authored tile metadata | Needed to move from code-built tilemaps to practical external level-authoring workflows | [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [OPEN_SUGGESTIONS.md](OPEN_SUGGESTIONS.md) |
| Collision layers and filtering | Needed for projectiles, triggers, selective overlap, and more practical gameplay collision rules | [TOP_FIVE_ENGINE_FEATURES_PLAN.md](TOP_FIVE_ENGINE_FEATURES_PLAN.md), [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) |
| Advanced input semantics and rebinding UX | Needed for buffered inputs, settings-menu rebinding workflows, and higher-level analog intent helpers beyond the shipped action-map layer | [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) |
| Advanced Camera2D constraints and composition | Needed for bounds, rails, composition anchors, and more opinionated follow presets on top of the shipped feel-helper core | [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [OPEN_SUGGESTIONS.md](OPEN_SUGGESTIONS.md) |
| Physics broadphase / spatial partitioning | Needed to scale beyond small and medium body counts without an O(n^2)-style bottleneck | [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) |
| 2D particle system | Needed for common feedback and VFX workflows | [TOP_FIVE_ENGINE_FEATURES_PLAN.md](TOP_FIVE_ENGINE_FEATURES_PLAN.md) |
| Game-facing UI / HUD system | Needed for menus, HUDs, and runtime UI beyond debug overlays | [TOP_FIVE_ENGINE_FEATURES_PLAN.md](TOP_FIVE_ENGINE_FEATURES_PLAN.md) |
| Scene serialization and level loading | Needed to move from code-only scenes toward data-driven content workflows | [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md), [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) |
| CMake package export / `find_package()` support | Needed for cleaner third-party engine consumption outside this repo | [../CMakeLists.txt](../CMakeLists.txt), [OPEN_SUGGESTIONS.md](OPEN_SUGGESTIONS.md) |

### Additional planned backlog

The broader ranked backlog remains in [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md). That file currently tracks additional planned work such as audio effects polish, post-processing, per-scene input blocking, finite-state-machine helpers, scripting integration, and debug drawing.

## Current Runnable Surface

- **Examples:** runnable example directories live under [../examples/](../examples/), with targets registered in [../examples/CMakeLists.txt](../examples/CMakeLists.txt). Coverage spans core rendering, sprites, tilemaps, text, transitions, physics, audio, diagnostics, storage, resources, automation, camera-feel workflows, and multi-viewport scenarios.
- **Games:** 2 games are registered in [../games/CMakeLists.txt](../games/CMakeLists.txt): [../games/pong/](../games/pong/) and [../games/fishing_game/](../games/fishing_game/).
- **Tools:** 4 tools are registered in [../tools/CMakeLists.txt](../tools/CMakeLists.txt): [../tools/vlauncher/](../tools/vlauncher/), [../tools/geometry_repl/](../tools/geometry_repl/), [../tools/hex_editor/](../tools/hex_editor/), and [../tools/resource_editor/](../tools/resource_editor/).
- **Tests:** [../tests/CMakeLists.txt](../tests/CMakeLists.txt) registers `vde_tests` and `vde_resource_editor_tests`, covering engine, game API, text, audio, physics, transitions, launcher utilities, and resource-editor domain logic.
- **Verification:** [../scripts/verify.ps1](../scripts/verify.ps1) combines build, unit test, smoke test, render verification, and lint workflows. The supporting scripts in [../scripts/](../scripts/) are part of the core project surface, not incidental utilities.

## Documentation Status

- **Canonical current status:** this file.
- **Reference docs:** [API.md](API.md), [ARCHITECTURE.md](ARCHITECTURE.md), [GETTING_STARTED.md](GETTING_STARTED.md), and [CONTRIBUTING.md](CONTRIBUTING.md).
- **Current evaluation:** [ENGINE_API_EVALUATION_2026-05-26.md](ENGINE_API_EVALUATION_2026-05-26.md) is a strong repo assessment and correctly identified the need for a canonical status doc.
- **Ranked backlog:** [REMAINING_ENGINE_DEFICIENCIES.md](REMAINING_ENGINE_DEFICIENCIES.md).
- **Historical or partially stale planning docs:** [OPEN_SUGGESTIONS.md](OPEN_SUGGESTIONS.md), [TOP_FIVE_ENGINE_FEATURES_PLAN.md](TOP_FIVE_ENGINE_FEATURES_PLAN.md), and [API_IMPROVEMENTS_PLAN.md](API_IMPROVEMENTS_PLAN.md). These remain useful as design history, but they should not be treated as the authoritative answer to current status.

## Maintenance Rule

When public behavior changes, update this document alongside the focused API or design doc for that area. Historical planning docs do not need to be rewritten each time; this file is the place that should stay current.