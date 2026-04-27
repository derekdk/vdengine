# Overlay Sheet API Design

## Overview

Overlay sheets are movable rectangular windows that render a full `Scene` above the
current gameplay scene or active `SceneGroup`. They are intended for character
sheets, spell lists, journals, codex pages, tactical maps, inspect windows, and
other in-game overlays that need more than a few HUD widgets.

The important constraint is that a sheet is not just a 2D panel. It must be able
to host normal VDE scene content: text, sprites, meshes, particles, lighting, and
3D cameras. In practice, a sheet is a normal scene presented through a movable
window rectangle.

This should be implemented as a thin runtime layer on top of the existing
multi-scene viewport renderer rather than as a new UI framework. The engine already
has the rendering primitives needed for per-scene viewports and per-viewport depth
clears. What is missing is a first-class runtime controller for:

- showing a scene as a window at any time
- hiding and re-showing it without rebuilding the active scene group
- moving it interactively or through animation
- routing input and focus to the topmost visible sheet
- drawing simple window chrome around the scene content

## Why Existing APIs Are Not Enough

### `pushScene()` is the wrong model

`pushScene()` is a full-screen stack mechanic. It is appropriate for pause menus
and scene replacement, but not for several movable windows that can coexist above
the world.

### `SceneGroup` is too static for runtime windows

`SceneGroup` plus `ViewportRect` already solves much of the rendering problem, but
it is a static description of active scenes. Using it directly for overlay sheets
would force the user to rebuild and reapply the whole active group whenever a sheet
is shown, hidden, moved, or brought to front.

That creates too much boilerplate for a common task.

### Current mouse routing does not match viewport ownership

The engine already has `getSceneAtScreenPosition()` and reverse-order overlay hit
testing, but the live GLFW mouse callbacks still deliver input to `m_activeScene`
instead of the topmost scene under the cursor. That is workable for split-screen
experiments, but not for draggable overlay windows.

### This is not the same feature as the future HUD/UI system

The planned `UICanvas` / HUD feature is about screen-space widgets inside a scene.
Overlay sheets are about hosting an entire scene as a movable window. The two
features complement each other:

- HUD/UI widgets solve fixed in-scene interface layout.
- Overlay sheets solve scene-as-window presentation and orchestration.

An overlay sheet scene may later use `UICanvas` internally, but the sheet system
should not wait for that work.

## Goals

- Show any registered scene as a movable rectangular window above the current scene.
- Allow sheets to host normal 2D or 3D scene content with independent cameras.
- Allow a sheet to be shown, hidden, focused, and closed at runtime.
- Allow sheet position changes to be immediate or animated.
- Keep the common case to one call from `Game`.
- Preserve existing `Scene`, `SceneGroup`, and `InputHandler` patterns as much as possible.
- Keep ImGui and debug overlays working above sheet rendering.
- Reuse the existing multi-viewport renderer instead of introducing a second rendering path.

## Non-Goals

- A full desktop-style window manager.
- Docking, tab stacks, split panes, or resize handles in the first slice.
- A new retained-mode widget tree.
- Replacing `SceneGroup`, `pushScene()`, or the planned HUD/UI work.
- Native OS windows.
- Rounded corners, background blur, or compositor-heavy effects in the first slice.

## Key Design Decisions

### 1. Sheets should be scene-backed, not widget-backed

The user requirement explicitly includes text, graphics, and 3D content. A normal
`Scene` already models that well. Reusing `Scene` avoids inventing a parallel
content model.

### 2. Sheets should be additive to the active scene group

The active scene or `SceneGroup` should remain the base world state. Visible sheets
should render after the base group rather than being folded into it by user code.

That keeps the API simple and avoids lifecycle churn whenever a window moves.

### 3. Do not overload `ViewportRect` with window-layout semantics

`ViewportRect` is a low-level normalized render primitive. It should stay that way.

Overlay sheets need higher-level placement semantics such as pixels, anchors, and
title-bar chrome. A new `SheetRect` type should resolve to a `ViewportRect`
internally using the current window size.

### 4. Keep `InputHandler` source-compatible in the first slice

Changing every input callback signature would widen the feature too much. Instead,
the engine should:

- route mouse and keyboard events to the correct visible sheet
- add viewport-local coordinate helpers for scenes that need them
- keep the existing `InputHandler` callbacks working with screen-space pixels

That is the smallest correct API expansion.

## Proposed Public API

### New Public Header

`include/vde/api/OverlaySheet.h`

### Core Types

```cpp
namespace vde {

using OverlaySheetId = uint64_t;

enum class SheetUnits : uint8_t {
    Pixels,
    Normalized,
};

enum class SheetAnchor : uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
};

enum class SheetEasing : uint8_t {
    Linear,
    EaseOutCubic,
    EaseInOutCubic,
};

struct SheetRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    SheetUnits units = SheetUnits::Pixels;
    SheetAnchor anchor = SheetAnchor::TopLeft;

    static SheetRect pixels(float x, float y, float width, float height,
                            SheetAnchor anchor = SheetAnchor::TopLeft);

    static SheetRect normalized(float x, float y, float width, float height,
                                SheetAnchor anchor = SheetAnchor::TopLeft);
};

struct SheetAnimation {
    float duration = 0.0f;
    SheetEasing easing = SheetEasing::EaseOutCubic;
    bool fade = false;
};

struct OverlaySheetOptions {
    std::string title;
    SheetRect rect = SheetRect::pixels(64.0f, 64.0f, 480.0f, 640.0f);
    bool visible = true;
    bool draggable = true;
    bool showFrame = true;
    bool showTitleBar = true;
    bool blocksMouse = true;
    bool takesKeyboardFocus = true;
    bool bringToFrontOnClick = true;
    int zOrder = 0;
};

class OverlaySheetHandle {
  public:
    OverlaySheetId id() const;
    bool isValid() const;

    void show(const SheetAnimation& animation = {});
    void hide(const SheetAnimation& animation = {});
    void close();

    bool isVisible() const;

    void moveTo(const SheetRect& rect);
    void animateTo(const SheetRect& rect, const SheetAnimation& animation = {});

    void setTitle(const std::string& title);
    void setZOrder(int zOrder);
    void focus();

    ViewportRect getResolvedViewport() const;
};

}  // namespace vde
```

### `Game` Additions

```cpp
class Game {
  public:
    OverlaySheetHandle showOverlaySheet(const std::string& sceneName,
                                        const OverlaySheetOptions& options = {});

    OverlaySheetHandle showOverlaySheet(const std::string& sceneName,
                                        std::unique_ptr<Scene> scene,
                                        const OverlaySheetOptions& options = {});

    bool hasVisibleOverlaySheets() const;
};
```

The second overload is the one-call path for the common case. It should internally
register the scene if it is not already present, then show it as a sheet.

## Example Usage

### Character Sheet

```cpp
auto characterSheet = game.showOverlaySheet(
    "character_sheet",
    std::make_unique<CharacterSheetScene>(),
    {
        .title = "Character",
        .rect = vde::SheetRect::pixels(32.0f, 32.0f, 440.0f, 620.0f),
        .draggable = true,
    }
);

characterSheet.hide({.duration = 0.15f, .fade = true});
characterSheet.show({.duration = 0.15f, .fade = true});
```

### Spell List Sliding to a New Position

```cpp
auto spellList = game.showOverlaySheet("spell_list", {
    .title = "Spells",
    .rect = vde::SheetRect::pixels(720.0f, 48.0f, 420.0f, 560.0f),
});

spellList.animateTo(
    vde::SheetRect::pixels(680.0f, 48.0f, 420.0f, 560.0f),
    {.duration = 0.20f}
);
```

### Map Window with 3D Content

```cpp
auto mapSheet = game.showOverlaySheet(
    "world_map",
    std::make_unique<WorldMapScene>(),
    {
        .title = "Map",
        .rect = vde::SheetRect::normalized(0.58f, 0.05f, 0.36f, 0.42f),
        .draggable = true,
    }
);
```

This works for 3D content because the sheet scene is still a normal `Scene` with
its own camera, lighting, and per-viewport depth isolation.

## Behavioral Model

### What a sheet is

An overlay sheet is a runtime presentation instance over a scene. It is not a new
scene type and it does not replace scene ownership.

- The `Game` still owns scenes.
- The sheet owns only placement, visibility, z-order, chrome, and animation state.
- Hiding a sheet preserves the scene state.
- Closing a sheet removes the runtime window instance, not the underlying scene registration.

### Lifecycle semantics

To keep the feature predictable and compatible with the existing scene API, the
first slice should use these rules:

- first show: call `onEnter()` if the scene is not already active elsewhere
- hide: do not call `onExit()` or `onPause()`; preserve state and stop rendering
- re-show after hide: resume rendering without a second `onEnter()`
- close: call `onExit()` for the sheet activation, then remove the sheet instance

This makes `hide()` a visibility toggle, not a scene deactivation.

### What a sheet is not

- It is not the active scene.
- It is not a scene-stack entry.
- It is not a general-purpose UI widget tree.

### Constraints for the first slice

To keep lifecycle and duplication simple, a scene should not be rendered both as a
base active scene and as an overlay sheet in the same frame. Attempting that should
be rejected clearly.

For the first slice, keep the rule even stricter:

- one visible sheet instance per scene
- a scene already present in the active scene group cannot also be shown as a sheet

Validation should happen in `Game::showOverlaySheet(...)` before the sheet instance
is created.

## Rendering Design

### Render Order

The render order should become:

1. Render the active scene or active `SceneGroup` exactly as today.
2. Render each visible overlay sheet in z-order from back to front.
3. Run `onRender()` so ImGui and debug overlays remain on top.

Implementation detail: the cleanest way to preserve current behavior is to append
visible sheets to the same `SceneRenderInfo` list used by `drawFrameMultiScene()`,
then attach the existing `onRender()` callback to the final combined entry. That
keeps ImGui above sheets without inventing a second overlay stage.

### Sheet Composition

Each visible sheet has two rectangles:

- outer rect: full window chrome including frame and title bar
- content rect: inner viewport where the sheet scene renders

For the first slice:

- draw simple rectangular chrome internally
- render the scene content directly into the content rect
- avoid offscreen composition unless a later effect requires it

Direct viewport rendering is the correct default because it keeps 3D scene support
cheap and aligned with the existing renderer.

### Why direct rendering is viable

The current multi-scene renderer already does two important things that sheets need:

- it renders each scene with its own viewport and scissor
- it clears depth in that scene's viewport region before rendering

That means a 3D map or inspect scene can render inside a sheet without depth
contamination from the world scene behind it.

### Camera behavior

When rendering a sheet scene, the engine should compute the content-rect aspect
ratio and apply it to the sheet scene's camera exactly like the current multi-scene
path does for `SceneGroup` viewports.

## Input and Focus Design

### Mouse routing

Mouse routing should use the topmost visible sheet under the cursor.

- On press, hit-test visible sheets from front to back.
- If the cursor is inside the title bar and `draggable == true`, begin a drag capture.
- If the cursor is inside the content rect, route the event to the sheet scene's input handler.
- If no visible sheet claims the mouse, fall back to the base active scene/group.

### Pointer capture

Once dragging begins, mouse move and release events should keep targeting that sheet
until the button is released, even if the cursor leaves the rectangle.

Pointer capture state must live outside the scheduler in `Game` / `OverlaySheetManager`
because GLFW callbacks fire immediately. While capture is active, mouse events route
exclusively to the captured sheet and do not fall through to the world scene.

### Keyboard focus

Keyboard and char input should go to the focused visible sheet if:

- the sheet is visible
- `takesKeyboardFocus == true`

Otherwise, existing `setFocusedScene()` behavior remains in effect for the base
scene group.

### Input blocking

If a sheet has `blocksMouse == true`, pointer events should not fall through to the
world scene underneath it. This directly addresses the existing overlay-input gap
already noted in the repo's multi-scene suggestions.

### Coordinate helpers

The first slice should keep `InputHandler` callback signatures unchanged, but add a
small helper surface for scenes rendered in non-full-window viewports.

Recommended additive API:

```cpp
struct ViewportPixels {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

class Scene {
  public:
    ViewportPixels getResolvedViewportPixels() const;
    bool screenToViewportPixels(double screenX, double screenY,
                                double& localX, double& localY) const;
};
```

That helper improves all viewport-based features, not just overlay sheets.

The manager should also cache each sheet's outer and content pixel rects. Those
cached rects are the source of truth for title-bar hit testing, drag math, and
coordinate translation during callbacks.

## Animation Model

The first slice only needs layout animation, not a general timeline system.

Required supported motions:

- show
- hide
- animate to a new rect

That is enough for:

- sliding a spell list in from the side
- moving a character sheet to avoid covering gameplay
- fading a map sheet in or out

### Implementation approach

Each sheet instance should hold:

- current resolved rect
- target rect
- animation progress
- current visibility alpha

An internal per-frame sheet update step can advance those values before rendering.

Animation interruption rule: a new `show()`, `hide()`, or `animateTo()` call should
replace the in-flight animation and start from the sheet's current interpolated
state rather than snapping back to its previous origin.

No public generic tween system is required in the first slice.

## Engine Architecture

### Public files

- `include/vde/api/OverlaySheet.h`
- `include/vde/api/Game.h` updates
- `include/vde/api/Scene.h` viewport-local helper updates

### Internal files

- `src/api/OverlaySheet.cpp`
- `src/api/OverlaySheetManager.cpp`
- `src/api/Game.cpp` integration

### Internal runtime model

```text
Game
 |- active scene / SceneGroup
 |- OverlaySheetManager
 |   |- OverlaySheetInstance[]
 |   |   |- scene name
 |   |   |- options
 |   |   |- resolved outer rect
 |   |   |- resolved content rect
 |   |   |- visibility state
 |   |   |- z-order
 |   |   |- animation state
 |   |   |- drag state
 |   |   `- focus state
 |   `- hit testing / ordering / animation update
 `- normal render loop + sheet render pass insertion
```

### Scheduler integration

Overlay sheets need two kinds of updates:

- manager-level updates for drag and animation state
- scene updates for visible sheet scenes

Recommended behavior:

- an `overlaySheets.update` manager task is always present
- visible or animating sheet scenes are included in the scheduler update set
- hidden sheets preserve state but do not update by default
- sheet animation state updates every frame while a sheet is visible or animating
- showing, hiding, closing, or creating a sheet triggers one scheduler rebuild
- moving or animating an already-visible sheet does not rebuild the graph

This keeps graph rebuilds tied to membership changes, not every frame.

### Focus ownership

When a visible sheet takes keyboard focus, the engine should remember the previously
focused base scene. When the sheet closes or explicitly releases focus, restore the
previous base-scene focus if it still exists.

That preserves the existing `setFocusedScene()` model instead of replacing it.

### Camera aspect ownership

`OverlaySheetManager` should resolve the current content rect, but the render path
should own the final camera aspect-ratio update because it already has the active
pixel size immediately before issuing draw commands.

This guarantees correct results after window resize, drag, or animation in the same
frame that the sheet renders.

## Relationship to Existing Features

### `SceneGroup`

`SceneGroup` remains the right API for fixed simultaneous scenes such as:

- split-screen
- permanent HUD scenes
- always-on minimaps

Overlay sheets are the right API for temporary, movable, user-controlled scene
windows.

### `pushScene()`

`pushScene()` remains the right API for full-screen scene-stack behavior. Overlay
sheets should not reuse those semantics.

### Future `UICanvas`

`UICanvas` can later be used inside a sheet scene for labels, lists, buttons, and
status blocks. That is a good complement, not a conflict.

## Acceptance Criteria

- [ ] Any registered scene can be shown as an overlay sheet with one `Game` call.
- [ ] A sheet can be hidden and shown again without rebuilding the active scene group.
- [ ] A sheet can be dragged by its title bar.
- [ ] A sheet can be moved programmatically and animated to a new position.
- [ ] A sheet can host 3D content with correct camera aspect ratio and isolated depth.
- [ ] Mouse input goes to the topmost blocking visible sheet under the cursor.
- [ ] Keyboard input can focus a visible sheet without breaking existing scene focus APIs.
- [ ] Base scene rendering and ImGui overlays still work unchanged.

## Implementation Plan

### Phase 1: Core data model and API surface

- Add `OverlaySheet.h` public types.
- Add `Game::showOverlaySheet(...)` entry points.
- Add internal manager and handle plumbing.
- Add `SheetRect` resolution from pixels or normalized units.

### Phase 2: Rendering integration

- Render visible sheets after the active scene/group.
- Add simple internal chrome drawing for frame and title bar.
- Compute content rect and camera aspect ratio from the resolved layout.

### Phase 3: Input and dragging

- Route mouse hit-testing through visible sheets first.
- Implement title-bar dragging and pointer capture.
- Add keyboard focus handoff to focused visible sheet.
- Add viewport-local coordinate helpers on `Scene`.

### Phase 4: Animation

- Add show, hide, and `animateTo()` support.
- Update layout animation in the per-frame sheet manager update.

### Phase 5: Documentation, examples, and tests

- Update `API-DOC.md` and `docs/API.md`.
- Add an example such as `overlay_sheet_demo` or extend `multi_scene_demo`.
- Add unit tests for layout resolution, z-order hit testing, and input routing.
- Add a smoke scenario that opens, moves, hides, and re-shows a sheet.

## Testing Plan

### Unit tests

- `SheetRect` resolution for pixels and normalized coordinates
- anchor behavior under window resize
- z-order hit testing
- show/hide state transitions
- dragging updates the correct sheet rect

### Integration tests

- input goes to the topmost visible sheet first
- input falls back to the world when no sheet is hit
- keyboard focus can move from world scene to sheet and back

### Smoke test scenario

- launch gameplay scene
- open character sheet
- drag it to a new position
- open map sheet
- hide character sheet
- re-show character sheet
- assert expected viewport sizes and scene activity

### Visual verification

At least one example should visually confirm:

- chrome draws in the correct order
- sheet motion is smooth
- 3D sheet content renders correctly above the world scene

## Recommended First Example

The best demonstration is a small gameplay scene with three sheets:

- character sheet with text and icon layout
- spell list that slides in and out
- 3D map sheet using its own camera

This exercises every important requirement without over-scoping the implementation.

## Summary

The correct feature is not a new windowing subsystem and not a new UI toolkit. It
is a focused scene-orchestration layer that lets the user present normal VDE scenes
as movable overlay windows.

That gives the API a strong new capability with a small mental model:

- the world remains the active scene or scene group
- any other scene can be shown as a sheet
- the sheet can be moved, hidden, focused, and animated
- the content inside the sheet is still just VDE scene content

This is a good additive API because it solves the requested use case directly,
reuses the renderer the engine already has, and stays compatible with the future
HUD/UI work instead of competing with it.