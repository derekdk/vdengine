# Text Rendering Implementation Plan

This document describes the step-by-step plan for adding text rendering support to VDE. Each phase is designed to produce a buildable, testable increment.

---

## Background

VDE currently has no built-in text rendering. The `SpriteEntity` can display any `Texture`, so text can be rendered by baking a string into a texture and applying it to a sprite. The vertical shooter example already uses this approach with a hand-coded 5×7 pixel bitmap font.

The goal of this plan is to promote that pattern into a first-class VDE API with a proper font backend, so any game or tool can render text without duplicating font infrastructure.

---

## Phase 1: Embedded Pixel Font Utility

**Goal:** A zero-dependency `BitmapFont` class that ships fonts baked into the engine binary, usable immediately without any external files.

### Tasks

1. **Create `BitmapFont` class**
   - Files: `include/vde/api/BitmapFont.h`, `src/api/BitmapFont.cpp`
   - Stores a glyph table: character → bitmask rows (5×7 or 8×13 variants)
   - Ships two built-in fonts: `BitmapFont::small()` (5×7) and `BitmapFont::large()` (8×13)
   - Factory functions return `const BitmapFont&` singletons (no heap allocation)
   - Supports ASCII printable characters (0x20–0x7E)
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES` in root `CMakeLists.txt`

2. **Create `TextRenderer` utility**
   - Files: `include/vde/api/TextRenderer.h`, `src/api/TextRenderer.cpp`
   - Static method: `createTexture(VulkanContext*, text, font, style)` → `shared_ptr<Texture>`
   - `TextStyle` POD: `Color color`, `int pixelScale`, `int letterSpacing`
   - Renders each glyph left-to-right, returns a non-square RGBA texture
   - Caller sets scale on `SpriteEntity` using `tex->getWidth() / tex->getHeight()` ratio
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

3. **Expose in the `GameAPI.h` umbrella header**
   - Add `#include <vde/api/BitmapFont.h>` and `#include <vde/api/TextRenderer.h>`

4. **Write unit tests**
   - File: `tests/TextRenderer_test.cpp`
   - Verify texture dimensions (width = glyphs × glyph_width × scale, height = glyph_height × scale)
   - Verify empty string returns a valid 1×1 (or minimal) texture without crashing
   - Verify pixel content: a known character's top-left pixel should match the expected color
   - Register in `tests/CMakeLists.txt`

5. **Create `pixel_arcade_demo` example**
   - Directory: `examples/pixel_arcade_demo/`
   - A retro arcade attract screen filled with classic UI elements — **"INSERT COIN"**, a flashing high-score table with 10 entries in alternating colors, a **"PLAYER 1 READY"** banner with per-letter color cycling (green, yellow, red, cyan), and a scrolling marquee of game instructions at the bottom
   - Uses `BitmapFont::small()` for the score table, `BitmapFont::large()` for banners; varies `pixelScale` (1–4×) and `letterSpacing` to fill the screen
   - Creates ~30 text textures at startup — a stress test of the batch-creation path
   - Demonstrates that `TextRenderer` alone (no `TextEntity`) is sufficient for fully static UI layouts
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

6. **Add smoke test**
   - Script: `smoketests/scripts/smoke_pixel_arcade_demo.vdescript`
   - Map entry in `scripts/smoke-test.ps1`

7. **Build and verify** — `scripts: build` + `scripts: test` + `scripts: smoke-test`

### Acceptance Criteria
- `TextRenderer::createTexture()` produces correct-sized textures for known inputs
- All unit tests pass
- No new warnings introduced
- `pixel_arcade_demo` renders all text banners and the smoke test passes

---

## Phase 2: TTF Font Backend via stb_truetype

**Goal:** Support loading a real TrueType font file and rendering smooth anti-aliased glyphs, backed by a glyph-atlas `Texture`.

### Tasks

1. **Add `stb_truetype.h` to third-party**
   - File: `third_party/stb/stb_truetype.h`
   - Source: [nothings/stb](https://github.com/nothings/stb) — public domain single-header
   - Add `third_party/stb/` to the include paths in root `CMakeLists.txt`

2. **Create `TrueTypeFont` class**
   - Files: `include/vde/api/TrueTypeFont.h`, `src/api/TrueTypeFont.cpp`
   - `TrueTypeFont::loadFromFile(VulkanContext*, path, sizePixels)` → builds a glyph atlas `Texture`
   - Atlas layout: 512×512 RGBA, glyphs packed row-by-row; falls back to 1024×1024 if needed
   - Stores per-glyph UV rectangles and advance widths
   - Reports `isLoaded()` and provides a fallback to `BitmapFont::small()` if file loading fails
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add TTF font asset (`assets/fonts/VDE_default.ttf`) — an open-licensed font (e.g. [Inconsolata](https://fonts.google.com/specimen/Inconsolata))

3. **Extend `TextRenderer` to accept `TrueTypeFont`**
   - Overload `createTexture(VulkanContext*, text, TrueTypeFont&, TextStyle)` using atlas UVs
   - Renders into an off-GPU pixel buffer, uploads once — avoids per-frame GPU work

4. **Write unit tests**
   - File: `tests/TrueTypeFont_test.cpp`
   - Test that loading a missing file falls back gracefully (no crash, returns false from `isLoaded()`)
   - Test that atlas texture dimensions are powers of two
   - Register in `tests/CMakeLists.txt`

5. **Create `font_specimen_demo` example**
   - Directory: `examples/font_specimen_demo/`
   - A side-by-side type specimen viewer: the left panel uses `BitmapFont::small()` and `BitmapFont::large()` to render the full printable ASCII glyph grid (rows of 16 characters, every glyph visible); the right panel renders the same pangram *"The quick brown fox jumps over the lazy dog 0123456789"* at six TrueType sizes (10 px → 96 px) stacked vertically
   - A third **"poster"** section at the bottom combines a large TTF headline with a small-pixel-font sub-line, demonstrating both renderers compositing naturally in the same scene
   - Includes a keyboard shortcut (`Tab`) that cycles the TTF render through three different bundled font files, demonstrating runtime atlas rebuilding
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

6. **Add smoke test**
   - Script: `smoketests/scripts/smoke_font_specimen_demo.vdescript`
   - Map entry in `scripts/smoke-test.ps1`

7. **Build and verify** — `scripts: build` + `scripts: test` + `scripts: smoke-test`

### Acceptance Criteria
- `TrueTypeFont::loadFromFile()` succeeds for the bundled default font
- `TextRenderer::createTexture()` works with both `BitmapFont` and `TrueTypeFont`
- Graceful fallback when font file is missing
- `font_specimen_demo` renders all panels and the smoke test passes

---

## Phase 3: TextEntity

**Goal:** A first-class scene entity that owns its text texture and rebuilds it lazily when content changes.

### Tasks

1. **Create `TextEntity` class**
   - Files: `include/vde/api/TextEntity.h`, `src/api/TextEntity.cpp`
   - Inherits `SpriteEntity`
   - Properties: `setText(string)`, `setFont(BitmapFont)`, `setTrueTypeFont(TrueTypeFont*)`, `setStyle(TextStyle)`
   - Dirty-flags any property change; rebuilds texture at the start of the next `update()` frame
   - On first use, auto-creates a 1×1 transparent placeholder texture so rendering never crashes
   - Preserves user-set position, anchor, and Z-depth across texture rebuilds
   - Add to `GameAPI.h` umbrella header

2. **Handle word wrapping (optional, stretch goal)**
   - `setMaxWidth(float worldUnits)` — wraps text at word boundaries to fit the specified width
   - Multi-line texture is taller; line height is `glyphHeight * lineSpacing`

3. **Write unit tests**
   - File: `tests/TextEntity_test.cpp`
   - Verify that calling `setText()` twice only rebuilds the texture once per frame
   - Verify texture dimensions update after `setStyle({ .pixelScale = 2 })`
   - Register in `tests/CMakeLists.txt`

4. **Update the vertical shooter example**
   - Replace the manual `createTextTexture()` calls in `Sprites.cpp` with `TextEntity` instances
   - Remove the embedded font table from `Sprites.cpp`

5. **Create `mission_control_demo` example**
   - Directory: `examples/mission_control_demo/`
   - A simulated space mission-control dashboard packed with live-updating `TextEntity` panels:
     - **Mission clock** — counts up in `HH:MM:SS.cc` format, updating every frame
     - **Telemetry grid** — 12 rows of labeled sensor readings (altitude, velocity, fuel %, temperature) that oscillate with pseudo-random noise, each row a separate `TextEntity`, demonstrating lazy rebuild under high per-frame churn
     - **Event log** — a 6-line scrolling log where a new status message is appended every 2 seconds; the oldest line is evicted (demonstrates `setText()` on cycling content)
     - **Alert banner** — a large `TextEntity` that switches between "NOMINAL", "CAUTION", and "WARNING" states driven by threshold logic, changing `TextStyle` color on each state transition (green → yellow → red)
   - All panels update on independent cadences (every frame, every 0.5 s, every 2 s) to stress-test the dirty-flag system and confirm no spurious rebuilds
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

6. **Add smoke test**
   - Script: `smoketests/scripts/smoke_mission_control_demo.vdescript`
   - Map entry in `scripts/smoke-test.ps1`

7. **Build and verify** — `scripts: build` + `scripts: test` + `scripts: smoke-test`

### Acceptance Criteria
- `TextEntity` renders visible text in the vertical shooter title screen
- No per-frame texture rebuild when text is unchanged
- `mission_control_demo` shows all panels updating correctly
- Smoke tests pass

---

## Phase 4: Capstone Demo — Interactive Text Adventure

**Goal:** A fully interactive text adventure game that pushes every text-rendering feature to its limit and proves they compose correctly end-to-end.

### Tasks

1. **Create `text_adventure_demo` example**
   - Directory: `examples/text_adventure_demo/`
   - A playable single-screen text adventure: rooms, items, and events are described via scrolling narrative; the player types short commands (`GO NORTH`, `TAKE TORCH`, `LOOK`) via a live `TextEntity` command prompt
   - **UI layout:**
     - Large TTF-rendered room title at the top (32 px, bold feel via `pixelScale`)
     - Pixel-font `BitmapFont::small()` mini-map in the top-right corner: ASCII art room grid, updates on every move
     - Scrolling narrative area: a ring-buffer of up to 10 `TextEntity` lines; each new event inserts at the top and old lines scroll down, demonstrating word-wrap for long descriptions
     - Live command-prompt `TextEntity` at the bottom with a blinking cursor (toggles `"_"` suffix every 0.5 s via `setText()`)
     - Status bar: pixel-font inventory list and HP counter, both `TextEntity`, rebuild only when values change
   - **Gameplay breadth:** at least 6 connected rooms, 8 items, and 4 triggered events — enough content to exercise long wrapped paragraphs, special characters, and rapid state changes
   - **Keyboard input:** letter keys append to the command buffer; `Enter` dispatches; `Backspace` deletes — all routed through VDE's existing input system, showcasing `TextEntity` as a reusable text-input widget pattern
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

2. **Add smoke test**
   - Script: `smoketests/scripts/smoke_text_adventure_demo.vdescript`
   - Scripted input sequence: sends `LOOK`, `GO NORTH`, `TAKE TORCH`, `INVENTORY` commands over 5 seconds; asserts the demo does not crash and exits cleanly
   - Map entry in `scripts/smoke-test.ps1`

3. **Build and verify** — `scripts: build` + `scripts: smoke-test`

### Acceptance Criteria
- All UI panels render without artifact: TTF title, pixel mini-map, scrolling narrative, blinking prompt, status bar
- Word-wrap correctly breaks long room descriptions at the panel boundary
- Commands typed via keyboard update the prompt `TextEntity` in real time
- Smoke test scripted sequence completes without crash

---

## Dependency Overview

```
Phase 1  BitmapFont + TextRenderer (static utility)
│           └─► pixel_arcade_demo
└─► Phase 2  TrueTypeFont (stb_truetype backend)
│               └─► font_specimen_demo
    └─► Phase 3  TextEntity (scene entity, lazy rebuild)
    │               └─► mission_control_demo
        └─► Phase 4  text_adventure_demo (capstone, all features)
```

Each phase is independently mergeable and useful on its own. Each phase's demo can be smoke-tested immediately after that phase lands.

---

## File Inventory

| Phase | New files |
|-------|-----------|
| 1 | `include/vde/api/BitmapFont.h`, `src/api/BitmapFont.cpp`, `include/vde/api/TextRenderer.h`, `src/api/TextRenderer.cpp`, `tests/TextRenderer_test.cpp`, `examples/pixel_arcade_demo/main.cpp`, `smoketests/scripts/smoke_pixel_arcade_demo.vdescript` |
| 2 | `third_party/stb/stb_truetype.h`, `include/vde/api/TrueTypeFont.h`, `src/api/TrueTypeFont.cpp`, `assets/fonts/VDE_default.ttf`, `tests/TrueTypeFont_test.cpp`, `examples/font_specimen_demo/main.cpp`, `smoketests/scripts/smoke_font_specimen_demo.vdescript` |
| 3 | `include/vde/api/TextEntity.h`, `src/api/TextEntity.cpp`, `tests/TextEntity_test.cpp`, `examples/mission_control_demo/main.cpp`, `smoketests/scripts/smoke_mission_control_demo.vdescript` |
| 4 | `examples/text_adventure_demo/main.cpp`, `smoketests/scripts/smoke_text_adventure_demo.vdescript` |
