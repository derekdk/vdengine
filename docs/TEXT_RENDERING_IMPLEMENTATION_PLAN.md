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

5. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- `TextRenderer::createTexture()` produces correct-sized textures for known inputs
- All unit tests pass
- No new warnings introduced

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

5. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- `TrueTypeFont::loadFromFile()` succeeds for the bundled default font
- `TextRenderer::createTexture()` works with both `BitmapFont` and `TrueTypeFont`
- Graceful fallback when font file is missing

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

5. **Build and verify** — `scripts: build` + `scripts: test` + `scripts: smoke-test`

### Acceptance Criteria
- `TextEntity` renders visible text in the vertical shooter title screen
- No per-frame texture rebuild when text is unchanged
- Smoke tests pass

---

## Phase 4: Writing Example

**Goal:** A dedicated example that showcases all text rendering modes.

### Tasks

1. **Create `text_demo` example**
   - Directory: `examples/text_demo/`
   - Demonstrates: static `BitmapFont` label, `TrueTypeFont` large heading, live-updating score counter via `TextEntity`, word-wrapped paragraph
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

2. **Add smoke test**
   - Script: `smoketests/scripts/smoke_text_demo.vdescript`
   - Map entry in `scripts/smoke-test.ps1`

3. **Build and verify** — `scripts: build` + `scripts: smoke-test`

### Acceptance Criteria
- All four text modes render correctly in the demo
- Smoke test passes

---

## Dependency Overview

```
Phase 1  BitmapFont + TextRenderer (static utility)
   └─► Phase 2  TrueTypeFont (stb_truetype backend)
          └─► Phase 3  TextEntity (scene entity, lazy rebuild)
                 └─► Phase 4  text_demo example
```

Each phase is independently mergeable and useful on its own.

---

## File Inventory

| Phase | New files |
|-------|-----------|
| 1 | `include/vde/api/BitmapFont.h`, `src/api/BitmapFont.cpp`, `include/vde/api/TextRenderer.h`, `src/api/TextRenderer.cpp`, `tests/TextRenderer_test.cpp` |
| 2 | `third_party/stb/stb_truetype.h`, `include/vde/api/TrueTypeFont.h`, `src/api/TrueTypeFont.cpp`, `assets/fonts/VDE_default.ttf`, `tests/TrueTypeFont_test.cpp` |
| 3 | `include/vde/api/TextEntity.h`, `src/api/TextEntity.cpp`, `tests/TextEntity_test.cpp` |
| 4 | `examples/text_demo/main.cpp`, `smoketests/scripts/smoke_text_demo.vdescript` |
