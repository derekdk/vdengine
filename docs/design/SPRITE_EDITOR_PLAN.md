# Sprite Editor — Design & Implementation Plan

## Overview

The **Sprite Editor** is a VDE tool for creating, previewing, and exporting sprite assets. It loads source images and allows users to define sprite regions, build animation sequences, and export spritesheet metadata files (`.vdesheet` TOML) that can be consumed by VDE's game API.

The editor fills the highest-priority gaps identified in [OPEN_SUGGESTIONS.md](../OPEN_SUGGESTIONS.md): SpriteSheet management, Sprite Animation, and Sprite Flipping — by providing a visual authoring workflow that produces assets ready for runtime use.

## Goals

1. **Load source images** — PNG/JPG via `ImageLoader` / `Texture`
2. **Define sprite regions** — Grid-based auto-slice or manual rectangle selection
3. **Name sprites** — Assign names to individual frames for lookup
4. **Build animations** — Group frames into named animation sequences with per-frame timing
5. **Preview animations** — Real-time playback in the viewport
6. **Export spritesheet metadata** — Save `.vdesheet` TOML files describing regions and animations
7. **Script mode** — Batch creation of spritesheets from command-line scripts
8. **Smoke testable** — Automated input-script verification

## Non-Goals (v1)

- Runtime spritesheet packing / atlas generation (source image is used as-is)
- Pixel-level editing (use the Resource Editor for that)
- Multi-texture atlas merging
- Import from Aseprite / TexturePacker (future enhancement)

---

## Architecture

### Position in VDE

```
tools/
  ToolBase.h                     # Shared base (existing)
  sprite_editor/
    CMakeLists.txt
    main.cpp                     # Entry point, mode selection
    SpriteEditorScene.h          # Scene declaration
    SpriteEditorScene.cpp        # Scene implementation + commands
    SpriteDocument.h             # In-memory spritesheet model
    SpriteDocument.cpp
    AnimationTimeline.h          # Animation sequence data
    AnimationTimeline.cpp
    vde.toml                     # Smoke test registration
    README.md
```

### Inheritance

```
BaseToolGame<SpriteEditorInputHandler, SpriteEditorScene>
  └── SpriteEditorGame

BaseToolInputHandler
  └── SpriteEditorInputHandler   (adds sprite-specific hotkeys)

BaseToolScene
  └── SpriteEditorScene          (commands, UI, preview rendering)
```

### Data Model

```
SpriteDocument
  ├── source image path (string)
  ├── Texture (shared_ptr<vde::Texture>)
  ├── image dimensions (width × height pixels)
  ├── sprites: vector<SpriteRegion>
  │     └── SpriteRegion { name, x, y, w, h }  (pixel coords)
  ├── animations: vector<AnimationSequence>
  │     └── AnimationSequence { name, looping, frames[] }
  │           └── AnimationFrame { spriteIndex, duration }
  └── metadata (author, created date, notes)
```

---

## Exported File Format (`.vdesheet`)

TOML format, loadable by `tomlplusplus` (already in the project):

```toml
[sheet]
image = "characters.png"
image_width = 512
image_height = 256

[[sprites]]
name = "idle_0"
x = 0
y = 0
w = 64
h = 64

[[sprites]]
name = "idle_1"
x = 64
y = 0
w = 64
h = 64

[[sprites]]
name = "run_0"
x = 128
y = 0
w = 64
h = 64

# ... more sprites

[[animations]]
name = "idle"
looping = true

[[animations.frames]]
sprite = "idle_0"
duration = 0.15

[[animations.frames]]
sprite = "idle_1"
duration = 0.15

[[animations]]
name = "run"
looping = true

[[animations.frames]]
sprite = "run_0"
duration = 0.1

[[animations.frames]]
sprite = "run_1"
duration = 0.1

[[animations.frames]]
sprite = "run_2"
duration = 0.1
```

---

## UI Layout

```
┌──────────────────────────────────────────────────────────────────┐
│  Menu Bar: [File ▾] [Edit ▾] [View ▾]                           │
├────────────────────────────┬─────────────────────────────────────┤
│                            │  Sprite List                       │
│                            │  ┌───────────────────────────────┐ │
│   Source Image Viewport    │  │ ☐ idle_0    [64×64]           │ │
│                            │  │ ☑ idle_1    [64×64]           │ │
│   (zoom/pan, grid overlay, │  │ ☐ run_0     [64×64]           │ │
│    click to select region, │  │ ☐ run_1     [64×64]           │ │
│    drag to create region)  │  │ ...                           │ │
│                            │  └───────────────────────────────┘ │
│                            ├─────────────────────────────────────┤
│                            │  Sprite Properties                 │
│                            │  Name: [idle_1       ]             │
│                            │  X: [64 ] Y: [0  ]                │
│                            │  W: [64 ] H: [64 ]                │
│                            │  Anchor: [0.5] [0.5]              │
├────────────────────────────┼─────────────────────────────────────┤
│   Animation Preview        │  Animation Editor                  │
│                            │  ┌───────────────────────────────┐ │
│   ┌──────────────┐         │  │ Animation: [idle ▾]  ☑ Loop   │ │
│   │              │         │  │                               │ │
│   │  (animated   │         │  │ Timeline:                     │ │
│   │   playback)  │         │  │ [idle_0|0.15] [idle_1|0.15]   │ │
│   │              │         │  │                               │ │
│   └──────────────┘         │  │ [▶ Play] [⏸ Pause] [⏹ Stop]  │ │
│   Speed: [1.0x]            │  │ [+ Add Frame] [- Remove]      │ │
│                            │  └───────────────────────────────┘ │
├────────────────────────────┴─────────────────────────────────────┤
│  Console / Command Input                                        │
│  > load characters.png                                          │
│  Loaded 512×256 image (4 channels)                              │
│  > grid 64 64                                                   │
│  Created 32 sprite regions (8 cols × 4 rows)                    │
│  > _                                                            │
└──────────────────────────────────────────────────────────────────┘
```

### Viewport Rendering

The source image is rendered as a `SpriteEntity` in the VDE scene using `Camera2D`. Sprite region outlines are drawn as colored overlaid sprites (thin rectangles) or via ImGui's `ImDrawList` overlay. The animation preview area uses a second `SpriteEntity` that cycles UV rects according to the active animation sequence.

---

## Command Set

All commands are available via the console (REPL) and in script files.

### File Commands

| Command | Description |
|---------|-------------|
| `load <path>` | Load a source image file |
| `save <path.vdesheet>` | Export spritesheet metadata to TOML |
| `open <path.vdesheet>` | Load an existing spritesheet for editing |

### Sprite Region Commands

| Command | Description |
|---------|-------------|
| `grid <cellW> <cellH> [spacingX spacingY] [offsetX offsetY]` | Auto-slice into uniform grid |
| `add <name> <x> <y> <w> <h>` | Add a named sprite region manually |
| `remove <name>` | Remove a sprite region |
| `rename <old> <new>` | Rename a sprite region |
| `anchor <name> <x> <y>` | Set anchor point (0-1 range) |
| `list` | List all sprite regions |

### Animation Commands

| Command | Description |
|---------|-------------|
| `anim create <name> [loop]` | Create a new animation sequence |
| `anim delete <name>` | Delete an animation sequence |
| `anim addframe <anim> <sprite> [duration]` | Append a frame (default 0.1s) |
| `anim removeframe <anim> <index>` | Remove frame at index |
| `anim setduration <anim> <index> <seconds>` | Change frame duration |
| `anim list` | List all animations |
| `anim play <name>` | Preview animation in viewport |
| `anim stop` | Stop animation preview |

### View Commands

| Command | Description |
|---------|-------------|
| `zoom <level>` | Set zoom level (1.0 = fit) |
| `showgrid [on\|off]` | Toggle grid overlay on source image |
| `shownames [on\|off]` | Toggle sprite name labels |
| `select <name>` | Highlight a sprite region |

### Utility Commands

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `clear` | Clear console output |
| `info` | Show document summary (image size, sprite count, animation count) |

---

## Implementation Phases

### Phase 1 — Scaffold & Image Loading

**Files:** `main.cpp`, `SpriteEditorScene.h/cpp`, `SpriteDocument.h/cpp`, `CMakeLists.txt`, `vde.toml`

**Deliverables:**
- Tool skeleton using `BaseToolGame` / `BaseToolScene`
- `load` command: load image via `ImageLoader`, create `Texture`, display as `SpriteEntity` in `Camera2D` viewport
- `zoom` / `pan` via mouse (wheel = zoom, middle-drag = pan on Camera2D)
- Console with `help`, `clear`, `info` commands
- Script mode entry point (pass script file as argv[1])
- CMake registration in `tools/CMakeLists.txt`
- Basic smoke test script

**Engine dependencies:** `Texture`, `ImageLoader`, `SpriteEntity`, `Camera2D`, `BaseToolScene`  
**New engine code:** None

### Phase 2 — Sprite Region Definition

**Files:** `SpriteDocument.h/cpp` (extend), `SpriteEditorScene.cpp` (extend)

**Deliverables:**
- `grid` command: auto-slices image into uniform cells, generates named regions (`sprite_0`, `sprite_1`, ...)
- `add` / `remove` / `rename` / `anchor` / `list` commands
- Visual overlay: draw rectangles around defined sprite regions on the source image (via ImGui `ImDrawList` in screen-space, transformed from world coords)
- Sprite List panel: shows all regions, click to select/highlight
- Sprite Properties panel: edit name, position, size, anchor for selected region
- Mouse interaction: click on source image to select nearest region; drag to create new manual region

**Engine dependencies:** None new  
**New engine code:** None

### Phase 3 — Animation Sequences

**Files:** `AnimationTimeline.h/cpp`, `SpriteEditorScene.cpp` (extend), `SpriteDocument.h/cpp` (extend)

**Deliverables:**
- `AnimationSequence` and `AnimationFrame` data model
- `anim create/delete/addframe/removeframe/setduration/list` commands
- Animation Editor panel: dropdown to select animation, frame strip showing thumbnails, editable duration per frame
- `anim play/stop`: preview animation on a separate `SpriteEntity` in the Animation Preview area, cycling UV rects by elapsed time
- Speed control slider

**Engine dependencies:** None new  
**New engine code:** None

### Phase 4 — Export & Import

**Files:** `SpriteDocument.cpp` (extend), `SpriteEditorScene.cpp` (extend)

**Deliverables:**
- `save <path.vdesheet>` command: serialize `SpriteDocument` to TOML via `tomlplusplus`
- `open <path.vdesheet>` command: deserialize TOML back into `SpriteDocument`, reload source image
- File dialog support (Windows COM for native open/save dialogs, like geometry_repl)
- Menu bar: File → New, Open, Save, Save As, Recent Files
- Validate round-trip: save → open → save produces identical output

**Engine dependencies:** `tomlplusplus` (already available)  
**New engine code:** None

### Phase 5 — Engine Integration (SpriteSheet + SpriteAnimation classes)

This phase adds **new engine API classes** so games can load `.vdesheet` files at runtime.

**New Engine Files:**
- `include/vde/api/SpriteSheet.h` / `src/api/SpriteSheet.cpp`
- `include/vde/api/SpriteAnimation.h` / `src/api/SpriteAnimation.cpp`

**SpriteSheet API:**
```cpp
namespace vde {

/// Defines a named rectangular region within a texture
struct SpriteRegion {
    std::string name;
    int x, y, w, h;          // pixel coordinates in source image
    float anchorX = 0.5f;
    float anchorY = 0.5f;
};

/// Loads .vdesheet files and provides UV rect lookups
class SpriteSheet {
public:
    using Ref = std::shared_ptr<SpriteSheet>;

    /// Load from a .vdesheet TOML file (also loads the referenced texture)
    static Ref load(const std::string& path, VulkanContext* ctx);

    /// Create a uniform grid sheet from an existing texture
    static Ref createGrid(std::shared_ptr<Texture> texture,
                          int cellWidth, int cellHeight,
                          int spacingX = 0, int spacingY = 0);

    /// Get UV rect for a sprite by name (for SpriteEntity::setUVRect)
    UVRect getUVRect(const std::string& name) const;

    /// Get UV rect for a sprite by index
    UVRect getUVRect(int index) const;

    /// Get the underlying texture
    std::shared_ptr<Texture> getTexture() const;

    /// Get sprite count
    int getSpriteCount() const;

    /// Get sprite region by name
    const SpriteRegion* getRegion(const std::string& name) const;
};

} // namespace vde
```

**SpriteAnimation API:**
```cpp
namespace vde {

struct AnimationFrame {
    std::string spriteName;
    float duration;           // seconds
};

class SpriteAnimation {
public:
    std::string name;
    bool looping = true;
    std::vector<AnimationFrame> frames;

    /// Get the sprite name for a given elapsed time
    const std::string& getFrameAt(float elapsed) const;

    /// Total animation duration
    float getTotalDuration() const;
};

/// Drives animation playback on a SpriteEntity
class SpriteAnimator {
public:
    void setSpriteSheet(SpriteSheet::Ref sheet);
    void addAnimation(const std::string& name, SpriteAnimation anim);

    /// Load all animations from a .vdesheet file
    void loadFromSheet(SpriteSheet::Ref sheet, const std::string& vdesheetPath);

    void play(const std::string& name, bool reset = true);
    void pause();
    void resume();
    void stop();

    void setSpeed(float speed);
    bool isPlaying() const;
    const std::string& getCurrentAnimation() const;

    /// Call each frame; updates the target SpriteEntity's UV rect
    void update(float deltaTime, SpriteEntity* target);
};

} // namespace vde
```

**Deliverables:**
- Engine-side `SpriteSheet` and `SpriteAnimator` classes
- Unit tests for both (load, UV rect lookup, animation frame timing)
- Integration with `ResourceManager` for caching
- Example usage in an updated demo (e.g., enhance `sidescroller` or create `animation_demo`)

### Phase 6 — Polish & Automation

**Deliverables:**
- Complete smoke test script (`smoke_sprite_editor.vdescript`)
- Register in `scripts/smoke-test.ps1` `$smokeScriptMap`
- README.md with usage instructions and screenshots
- Keyboard shortcuts (Delete = remove selected, Ctrl+S = save, Ctrl+Z = undo last command)
- Undo support via command log replay (execute in reverse)
- Grid snapping toggle for manual region placement

---

## Key Design Decisions

### Why TOML for `.vdesheet`?

- `tomlplusplus` v3.4.0 is already a project dependency (used by vlauncher)
- Human-readable and hand-editable
- Maps cleanly to the hierarchical sprite/animation data model
- Consistent with existing `vde.toml` config files in the project

### Why not pack textures?

Atlas packing (combining multiple source images into one texture) is a solved problem with mature external tools (TexturePacker, Aseprite export). The Sprite Editor focuses on _defining regions and animations_ on existing images, which is the workflow gap in VDE. Atlas packing can be added later as a separate tool or Phase 7 enhancement.

### Why render overlays with ImDrawList?

ImGui's `ImDrawList` allows drawing directly in screen-space over the VDE viewport. This avoids creating dozens of thin `SpriteEntity` objects for frame outlines and is simpler to implement for UI affordances (selection handles, drag handles, grid lines). It also doesn't interfere with the scene's entity list.

### Camera2D for the viewport

The source image is displayed using `Camera2D` orthographic projection. Zoom/pan manipulate the camera position and viewport size. This matches the 2D-games skill pattern and ensures pixel-accurate rendering at any zoom level.

---

## Runtime Usage Example

After the Sprite Editor produces a `.vdesheet` file, games use it like this:

```cpp
void onEnter() override {
    setup2D(20.0f, 15.0f);

    // Load spritesheet
    m_sheet = vde::SpriteSheet::load("assets/characters.vdesheet", getVulkanContext());

    // Create animated sprite
    m_player = addEntity<vde::SpriteEntity>();
    m_player->setTexture(m_sheet->getTexture());
    m_player->setScale(vde::Scale(2.0f, 2.0f, 1.0f));

    // Set up animator
    m_animator.setSpriteSheet(m_sheet);
    m_animator.loadFromSheet(m_sheet, "assets/characters.vdesheet");
    m_animator.play("idle");
}

void onUpdate(float dt) override {
    m_animator.update(dt, m_player);

    if (isMoving()) {
        m_animator.play("run");
    } else {
        m_animator.play("idle");
    }
}
```

---

## Dependencies Summary

| Dependency | Status | Used For |
|------------|--------|----------|
| `BaseToolScene` / `BaseToolGame` | ✅ Existing | Tool skeleton |
| `Texture` / `ImageLoader` | ✅ Existing | Source image loading |
| `SpriteEntity` | ✅ Existing | Image display + animation preview |
| `Camera2D` | ✅ Existing | 2D viewport |
| `tomlplusplus` | ✅ Existing (v3.4.0) | `.vdesheet` serialization |
| `ImGui` | ✅ Existing (`imgui_backend`) | Editor UI panels |
| `SpriteSheet` class | ❌ New (Phase 5) | Runtime API |
| `SpriteAnimator` class | ❌ New (Phase 5) | Runtime API |

## Estimated Scope

| Phase | New Files | Lines (est.) |
|-------|-----------|-------------|
| 1 — Scaffold | 6 | ~400 |
| 2 — Regions | 0 (extend) | ~500 |
| 3 — Animations | 2 | ~450 |
| 4 — Export/Import | 0 (extend) | ~350 |
| 5 — Engine API | 4 + tests | ~600 |
| 6 — Polish | 2 (smoke/readme) | ~200 |
| **Total** | **~14 files** | **~2,500 lines** |
