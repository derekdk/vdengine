# Text Widget System — Implementation Plan

This document extends the text rendering plan (Phases 1–4 complete) with an interactive widget system. Each phase builds on the previous and produces a buildable, testable increment.

---

## Problem Statement

VDE has solid text rendering (BitmapFont, TrueTypeFont, TextRenderer, TextEntity) but no reusable interactive UI widgets. Every demo that needs text input, word-wrapped paragraphs, buttons, or layout re-implements the same patterns from scratch:

- **text_adventure_demo** — manual `charBuffer`, backspace, blinking cursor, character-count word-wrap, ring-buffer scrolling narrative
- **text_metrics_demo** — manual pixel-accurate `measureTextWidthPx()`, `wordWrap()`, alignment via anchor/position math, `VisualBox` for containers
- **mission_control_demo** — duplicated `sizeToFit()`, manual scrolling log

There is no `Rect` type, no hit-testing, no focus management, and no click/hover detection.

---

## Proposed Widget Set

### User-Requested
| Widget | Purpose |
|--------|---------|
| **TextInputBox** | Single-line editable text field with cursor movement, selection, backspace/delete, copy/paste |
| **TextArea** | Multi-line text display with word wrap, text alignment (left/center/right), and vertical scrolling |

### Strongly Recommended Additions
| Widget | Purpose |
|--------|---------|
| **Label** | Enhanced TextEntity with horizontal/vertical alignment, padding, optional background color, and auto-sizing. The simplest widget and building block for all others |
| **Button** | Clickable label with hover/press/disabled visual states and an `onClick` callback. Fundamental to any interactive UI |
| **Panel** | Rectangular container with background color, optional border and title. Groups widgets visually |
| **Checkbox** | Boolean toggle with a label. Essential for settings and options |
| **ListBox** | Scrollable list of selectable text items. Needed for menus, inventories, file pickers |

### Infrastructure
| Component | Purpose |
|-----------|---------|
| **Rect** | 2D bounding box with `contains(point)`, `intersects(rect)`, and layout helpers |
| **Widget** | Base class for all widgets (extends SpriteEntity). Adds hit-testing, focus, padding, background rendering |
| **UIManager** | Singleton per scene. Routes mouse/keyboard events to the correct widget. Manages focus, tab order, hover tracking |
| **TextLayout** | Static utility promoted from demo code: `measureText()`, `wordWrap()`, `computeAlignment()` |

---

## Phase 5: Foundation — Rect, TextLayout, and Widget Base

**Goal:** Establish the infrastructure that all widgets depend on: a `Rect` type for 2D layout, a `TextLayout` utility to centralize word-wrap/alignment/measurement, and a `Widget` base class with hit-testing and focus support.

### Tasks

1. **Create `Rect` struct**
   - File: `include/vde/api/GameTypes.h` (add to existing types)
   - Fields: `float x, y, width, height` (x/y = bottom-left in VDE's coordinate system)
   - Methods: `contains(float px, float py)`, `intersects(const Rect&)`, `center()`, `topLeft()`, `bottomRight()`
   - `operator==` for comparisons

2. **Create `TextLayout` utility**
   - Files: `include/vde/api/TextLayout.h`, `src/api/TextLayout.cpp`
   - Promotes and unifies the duplicated helpers from demos:
     - `measureTextWidthPx(const TrueTypeFont& font, const std::string& text) → float`
     - `measureTextWidthPx(const BitmapFont& font, const TextStyle& style, const std::string& text) → float`
     - `wordWrap(const TrueTypeFont& font, const std::string& text, float maxWidthPx) → vector<string>`
     - `wordWrap(const BitmapFont& font, const TextStyle& style, const std::string& text, float maxWidthPx) → vector<string>`
     - `getLineHeightPx(const TrueTypeFont& font) → float`
     - `getLineHeightPx(const BitmapFont& font, const TextStyle& style) → float`
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add to `GameAPI.h` umbrella header

3. **Create `Widget` base class**
   - Files: `include/vde/api/Widget.h`, `src/api/Widget.cpp`
   - Inherits `SpriteEntity` (so it lives in the scene graph and renders as a sprite)
   - Adds:
     - `Rect m_bounds` — widget bounding box in world coordinates
     - `bool m_focused`, `bool m_hovered`, `bool m_enabled`
     - `Color m_backgroundColor`, `Color m_borderColor`, `float m_borderWidth`
     - `Padding m_padding` — `{float top, right, bottom, left}`
     - `void setBounds(Rect)` — sets position and size, rebuilds background texture
     - `bool hitTest(float worldX, float worldY)` — tests point against `m_bounds`
     - Virtual hooks: `onFocusGained()`, `onFocusLost()`, `onHoverEnter()`, `onHoverLeave()`
     - Virtual hooks: `onMouseDown(float x, float y)`, `onMouseUp(float x, float y)`, `onCharInput(unsigned int codepoint)`, `onKeyDown(int key)`, `onKeyRepeat(int key)`
     - `setFocusable(bool)` — whether this widget can receive keyboard focus
     - Protected: `rebuildBackground()` — creates a solid-color texture with optional border for the widget's background
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

4. **Create `UIManager`**
   - Files: `include/vde/api/UIManager.h`, `src/api/UIManager.cpp`
   - Owned by the user (one per scene or shared), not a singleton inside the engine
   - API:
     - `addWidget(Widget::Ref)` / `removeWidget(Widget::Ref)`
     - `setFocus(Widget::Ref)` — sets keyboard focus, calls `onFocusLost`/`onFocusGained`
     - `getFocusedWidget() → Widget::Ref`
     - `tabNext()` / `tabPrev()` — cycles focus through focusable widgets in insertion order
     - `handleMouseMove(float worldX, float worldY)` — updates hover state
     - `handleMouseDown(float worldX, float worldY)` — focus + dispatch click to topmost hit widget
     - `handleMouseUp(float worldX, float worldY)`
     - `handleMouseScroll(float xOffset, float yOffset)` — dispatch to hovered widget
     - `handleCharInput(unsigned int codepoint)` — dispatch to focused widget
     - `handleKeyDown(int key)` — Tab cycles focus; else dispatch to focused widget
     - `handleKeyRepeat(int key)` — dispatch to focused widget
   - The demo's `InputHandler` calls these methods to bridge VDE input events to widgets
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add `Widget.h`, `UIManager.h`, `TextLayout.h` to `GameAPI.h`

5. **Write unit tests**
   - File: `tests/TextLayout_test.cpp`
     - Verify `measureTextWidthPx()` returns expected values for known strings with BitmapFont
     - Verify `wordWrap()` breaks at word boundaries and respects max width
     - Verify empty string returns empty vector from `wordWrap()`
   - File: `tests/Widget_test.cpp`
     - Verify `hitTest()` returns true/false for points inside/outside bounds
     - Verify `Rect::contains()` with edge cases
     - Verify focus gained/lost callbacks fire correctly via `UIManager`
     - Verify `UIManager::tabNext()` cycles through focusable widgets
   - Register both in `tests/CMakeLists.txt`

6. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- `Rect` contains/intersects work correctly
- `TextLayout::wordWrap()` produces results identical to existing demo implementations for the same inputs
- `Widget::hitTest()` correctly determines point-in-rect
- `UIManager` focus cycling works with Tab key
- All unit tests pass

---

## Phase 6: Label and Panel Widgets

**Goal:** Two display-only widgets that are building blocks for everything else: `Label` (text with alignment and background) and `Panel` (rectangular container).

### Tasks

1. **Create `Label` widget**
   - Files: `include/vde/api/Label.h`, `src/api/Label.cpp`
   - Inherits `Widget`
   - Adds:
     - `enum class HAlign { Left, Center, Right }`
     - `enum class VAlign { Top, Center, Bottom }`
     - `void setText(const std::string& text)`
     - `void setFont(const BitmapFont& font)` / `void setTrueTypeFont(const TrueTypeFont* font)`
     - `void setTextStyle(const TextStyle& style)`
     - `void setAlignment(HAlign h, VAlign v)`
     - `void setPadding(Padding)` — space between bounds edge and text
     - `void setAutoSize(bool)` — when true, bounds grow to fit text content
   - Internally creates a child `TextEntity` for the text, positioned within bounds according to alignment and padding
   - Rebuilds on dirty (same lazy pattern as TextEntity)
   - Background rendered by `Widget::rebuildBackground()`
   - Not focusable by default
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

2. **Create `Panel` widget**
   - Files: `include/vde/api/Panel.h`, `src/api/Panel.cpp`
   - Inherits `Widget`
   - Adds:
     - `void setTitle(const std::string& title)` — optional header text
     - `void setTitleFont(const TrueTypeFont* font)` / `void setTitleFont(const BitmapFont& font)`
     - `Rect getContentArea() const` — bounds minus border/title area, for child positioning
   - Purely visual: renders a background rectangle with optional border and optional title bar
   - Children are positioned manually by the user within `getContentArea()`
   - Not focusable
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

3. **Write unit tests**
   - File: `tests/Label_test.cpp`
     - Verify text content updates propagate to internal TextEntity
     - Verify alignment positions the text correctly within bounds
     - Verify auto-size grows bounds to fit text
   - File: `tests/Panel_test.cpp`
     - Verify `getContentArea()` accounts for border width and title height
     - Verify title text renders when set
   - Register in `tests/CMakeLists.txt`

4. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- Label renders text with correct alignment inside its bounds
- Label auto-sizes when `setAutoSize(true)` and text changes
- Panel provides correct content area insets
- All unit tests pass

---

## Phase 7: TextInputBox Widget

**Goal:** A production-quality single-line text input with cursor movement, text selection, backspace/delete, and clipboard support.

### Tasks

1. **Create `TextInputBox` widget**
   - Files: `include/vde/api/TextInputBox.h`, `src/api/TextInputBox.cpp`
   - Inherits `Widget`
   - **Text editing:**
     - `void setValue(const std::string& text)` / `const std::string& getValue() const`
     - `void setPlaceholder(const std::string& text)` — greyed-out hint when empty
     - `void setMaxLength(size_t)` — character limit (0 = unlimited)
     - `onCharInput()` inserts character at cursor position
     - Backspace deletes character before cursor (or selected range)
     - Delete key removes character after cursor (or selected range)
   - **Cursor:**
     - `m_cursorPos` — index into text string (0 = before first char)
     - Left/Right arrow keys move cursor one character
     - Home/End move cursor to start/end of text
     - Ctrl+Left/Right skip by word boundary
     - Cursor blinks on 0.5s interval (visual only, doesn't affect state)
     - Cursor rendered as a thin vertical bar at the correct pixel offset (computed via `TextLayout::measureTextWidthPx()` on the substring before cursor)
   - **Selection:**
     - Shift+Left/Right/Home/End extends selection from anchor to cursor
     - Ctrl+A selects all
     - Typing or pasting replaces selected range
     - Selected text rendered with inverted or highlight background
   - **Clipboard:**
     - Ctrl+C copies selected text
     - Ctrl+V pastes from clipboard, replacing selection if any
     - Ctrl+X cuts selected text
     - Uses GLFW clipboard API (`glfwGetClipboardString` / `glfwSetClipboardString`)
   - **Visual:**
     - Background color changes on focus (subtle highlight)
     - Text scrolls horizontally when content exceeds visible width (viewport offset)
     - Placeholder text rendered in 50% alpha when value is empty and unfocused
   - **Callbacks:**
     - `void setOnValueChanged(std::function<void(const std::string&)>)` — fires on every edit
     - `void setOnSubmit(std::function<void(const std::string&)>)` — fires on Enter key
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add to `GameAPI.h` umbrella header

2. **Write unit tests**
   - File: `tests/TextInputBox_test.cpp`
   - Test cases:
     - Character insertion at cursor position
     - Backspace at start of text (no-op)
     - Delete at end of text (no-op)
     - Cursor movement: left, right, home, end
     - Word-skip with Ctrl+Left/Right
     - Selection: Shift+Right selects, typing replaces
     - Ctrl+A selects all, then Backspace clears
     - Max-length enforcement
     - Placeholder visibility when empty vs non-empty
     - `onValueChanged` callback fires on each edit
     - `onSubmit` callback fires on Enter
   - Register in `tests/CMakeLists.txt`

3. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- All cursor movement keys work correctly at boundaries
- Selection + delete/type replaces the selected range
- Clipboard operations work via GLFW
- Horizontal scroll keeps cursor visible when text overflows
- Blinking cursor renders at the correct position
- All unit tests pass

---

## Phase 8: TextArea Widget

**Goal:** A multi-line text display/edit area with word wrapping, text alignment, and vertical scrolling.

### Tasks

1. **Create `TextArea` widget**
   - Files: `include/vde/api/TextArea.h`, `src/api/TextArea.cpp`
   - Inherits `Widget`
   - **Text content:**
     - `void setText(const std::string& text)` — sets full content (may contain `\n`)
     - `const std::string& getText() const`
     - `void appendText(const std::string& text)` — appends and auto-scrolls to bottom
     - `void clear()`
   - **Word wrap:**
     - `void setWordWrap(bool enabled)` — default `true`
     - Uses `TextLayout::wordWrap()` at the pixel-accurate level
     - Reflows on bounds resize or font change
   - **Alignment:**
     - `void setAlignment(HAlign align)` — Left (default), Center, Right
     - Each wrapped line is positioned per the alignment within the content area
   - **Scrolling:**
     - `void setScrollable(bool)` — default `true`
     - Vertical scroll via mouse wheel when hovered
     - `scrollToTop()` / `scrollToBottom()` — programmatic scroll
     - `void setScrollPosition(float normalized)` — 0.0 = top, 1.0 = bottom
     - Visual scroll bar indicator on the right edge when content overflows (thin colored bar)
     - Content clips to bounds (lines outside visible region are not rendered)
   - **Appearance:**
     - `void setLineSpacing(float multiplier)` — default 1.2
     - `void setFont(...)` / `void setTrueTypeFont(...)` / `void setTextStyle(...)`
     - Background and border inherited from Widget
   - **Implementation strategy:**
     - Internally manages a `vector<TextEntity::Ref>` pool for visible lines
     - On text change: runs word-wrap → computes total lines → updates scroll range
     - On scroll: repositions visible-line entities from the pool to show the correct window of text
     - Pool size = max visible lines + 1 (one buffer line for smooth scroll)
     - Lines outside the visible window have `setVisible(false)`
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add to `GameAPI.h` umbrella header

2. **Write unit tests**
   - File: `tests/TextArea_test.cpp`
   - Test cases:
     - Word wrap produces correct line count for known text + width
     - Alignment positions lines correctly (left edge, center, right edge)
     - `appendText()` followed by scroll-to-bottom leaves last line visible
     - `scrollToTop()` shows first line
     - Empty text renders without crash
     - Resizing bounds triggers reflow
     - `setWordWrap(false)` disables wrapping — long lines extend beyond visible area
   - Register in `tests/CMakeLists.txt`

3. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- Word wrap breaks correctly at word boundaries using pixel-accurate measurement
- All three alignments position text correctly within the content area
- Mouse-wheel scrolling moves the visible window through the content
- Scroll bar indicator reflects scroll position
- Content does not render outside widget bounds
- All unit tests pass

---

## Phase 9: Button and Checkbox Widgets

**Goal:** Two interactive widgets that complete the basic widget set: `Button` for actions and `Checkbox` for boolean toggles.

### Tasks

1. **Create `Button` widget**
   - Files: `include/vde/api/Button.h`, `src/api/Button.cpp`
   - Inherits `Widget` (internally uses a Label for text)
   - **States:** Normal, Hovered, Pressed, Disabled — each with configurable `Color` for background and text
   - `void setText(const std::string& text)`
   - `void setFont(...)` / `void setTrueTypeFont(...)`
   - `void setOnClick(std::function<void()>)` — fires on mouse-up inside bounds after mouse-down inside bounds
   - `void setEnabled(bool)` — disabled buttons don't respond to clicks and use disabled colors
   - `onMouseDown()` → Pressed state; `onMouseUp()` inside bounds → fires onClick, returns to Hovered; outside → returns to Normal
   - Hover state tracks via `onHoverEnter()` / `onHoverLeave()`
   - Keyboard: Enter/Space while focused fires onClick
   - Focusable by default
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

2. **Create `Checkbox` widget**
   - Files: `include/vde/api/Checkbox.h`, `src/api/Checkbox.cpp`
   - Inherits `Widget`
   - **State:** `bool m_checked`
   - `void setChecked(bool)` / `bool isChecked() const`
   - `void setLabel(const std::string& text)` — text displayed to the right of the check indicator
   - `void setOnToggle(std::function<void(bool)>)` — fires with new state on toggle
   - Visual: small square indicator (filled when checked, empty when unchecked) + label text
   - Clicking anywhere on the widget (indicator or label) toggles the state
   - Keyboard: Space while focused toggles
   - Focusable by default
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`

3. **Add both to `GameAPI.h` umbrella header**

4. **Write unit tests**
   - File: `tests/Button_test.cpp`
     - onClick fires on valid click sequence (down inside → up inside)
     - onClick does NOT fire on down inside → up outside (drag-out cancel)
     - Disabled button does not fire onClick
     - Enter key fires onClick when focused
   - File: `tests/Checkbox_test.cpp`
     - Toggle changes `isChecked()` state
     - `onToggle` callback receives correct new state
     - `setChecked()` programmatically updates visual without firing callback
   - Register in `tests/CMakeLists.txt`

5. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- Button shows correct visual state for normal/hover/press/disabled
- Button click requires down+up inside bounds (standard click behavior)
- Checkbox toggles on click and on Space key
- Callbacks fire with correct arguments
- All unit tests pass

---

## Phase 10: ListBox Widget

**Goal:** A scrollable list of selectable text items — useful for menus, inventories, settings.

### Tasks

1. **Create `ListBox` widget**
   - Files: `include/vde/api/ListBox.h`, `src/api/ListBox.cpp`
   - Inherits `Widget`
   - **Items:**
     - `void setItems(const std::vector<std::string>& items)`
     - `void addItem(const std::string& item)`
     - `void removeItem(size_t index)`
     - `void clearItems()`
   - **Selection:**
     - `void setSelectedIndex(int index)` — -1 = no selection
     - `int getSelectedIndex() const`
     - `std::string getSelectedItem() const`
     - `void setOnSelectionChanged(std::function<void(int, const std::string&)>)`
     - Click on an item selects it (highlighted background)
     - Up/Down arrow keys change selection when focused
     - Enter key on focused item fires a separate `onItemActivated` callback
   - **Scrolling:**
     - Mouse wheel scrolls the list
     - Selection is kept visible (auto-scroll when arrow-key selection moves off-screen)
   - **Visual:**
     - Items rendered as a column of Labels within the bounds
     - Selected item has distinct background color
     - Hovered item (not selected) has subtle highlight
     - Pool pattern: only visible items have entities (same as TextArea)
   - Add to `VDE_PUBLIC_HEADERS` and `VDE_SOURCES`
   - Add to `GameAPI.h` umbrella header

2. **Write unit tests**
   - File: `tests/ListBox_test.cpp`
     - Selection changes on `setSelectedIndex()`
     - Arrow keys navigate selection
     - Selection wraps or clamps at boundaries
     - `onSelectionChanged` fires with correct arguments
     - Scroll position auto-adjusts to keep selection visible
   - Register in `tests/CMakeLists.txt`

3. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- Items render correctly in a scrollable column
- Click selects the correct item
- Arrow keys navigate the selection
- All unit tests pass

---

## Phase 11: Widget Showcase Demo

**Goal:** A capstone demo that exercises every widget in a realistic interactive application, proving they compose correctly end-to-end.

### Tasks

1. **Create `widget_showcase_demo` example**
   - Directory: `examples/widget_showcase_demo/`
   - **Layout:** A single scene divided into themed sections, each showcasing a widget category:

   - **Section 1 — "User Profile" form (top-left)**
     - Panel titled "User Profile" containing:
       - Label "Name:" + TextInputBox for name entry
       - Label "Bio:" + TextArea (3 lines visible, scrollable) for biography
       - Checkbox "Receive notifications"
       - Button "Save" — on click, copies form values into a read-only summary TextArea below
     - Demonstrates: Panel, Label, TextInputBox, TextArea, Checkbox, Button composing together

   - **Section 2 — "Message Log" (top-right)**
     - Panel titled "Messages" containing:
       - TextArea with word-wrap and right-aligned timestamps, scrollable, populated with ~30 lines of sample messages on startup
       - TextInputBox at bottom for typing new messages
       - Button "Send" — appends the input text to the message log and clears the input
     - Demonstrates: TextArea scrolling + appendText, TextInputBox submit, Button

   - **Section 3 — "Settings" (bottom-left)**
     - Panel titled "Settings" containing:
       - Checkbox "Dark Mode" — toggles all widget background colors between light and dark themes
       - Checkbox "Word Wrap" — toggles word wrap on the message log TextArea
       - ListBox with alignment options: "Left", "Center", "Right" — changing selection updates the message log alignment
       - ListBox with font options: lists available TrueType fonts — changing selection updates the font used by the message log
     - Demonstrates: Checkbox callbacks, ListBox selection, dynamic property changes on other widgets

   - **Section 4 — "Keyboard Test" (bottom-right)**
     - Panel titled "Keyboard Test" containing:
       - Large TextInputBox for free typing
       - Label showing cursor position: "Cursor: 12 / 45"
       - Label showing selection range: "Selection: 5-12"
       - Label showing last key event: "Last key: Ctrl+A"
       - Button "Clear" — clears the text input
       - Button "Select All" — selects all text programmatically
     - Demonstrates: TextInputBox cursor/selection API, real-time state inspection

   - **Tab navigation** — Tab key cycles focus through all focusable widgets; focused widget shows a highlighted border
   - **Mouse interaction** — all widgets respond to hover and click
   - Register with `add_vde_example()` in `examples/CMakeLists.txt`

2. **Add smoke test**
   - Script: `smoketests/scripts/smoke_widget_showcase_demo.vdescript`
   - Scripted input sequence:
     - Types "Hello World" into the name field
     - Presses Tab to advance to bio field
     - Types a multi-line bio
     - Clicks Save button (scripted mouse position)
     - Scrolls the message log
     - Toggles Dark Mode checkbox
     - Selects "Center" alignment from the ListBox
     - Waits 2 seconds, then exits
   - Map entry in `scripts/smoke-test.ps1`

3. **Build and verify** — `scripts: build` + `scripts: test` + `scripts: smoke-test`

### Acceptance Criteria
- All widgets render correctly and respond to input
- Tab navigation cycles through focusable widgets in order
- Form submission copies values to the summary
- Theme toggle visually updates all widget backgrounds
- Mouse hover/click and keyboard interactions all function correctly
- Smoke test passes

---

## Dependency Overview

```
Phase 1–4 (complete)  BitmapFont, TrueTypeFont, TextRenderer, TextEntity
│
└─► Phase 5   Rect + TextLayout + Widget + UIManager (foundation)
    │
    ├─► Phase 6   Label + Panel (display widgets)
    │   │
    │   ├─► Phase 7   TextInputBox (editable text field)
    │   │
    │   ├─► Phase 8   TextArea (multi-line display + scroll)
    │   │
    │   ├─► Phase 9   Button + Checkbox (interactive controls)
    │   │
    │   └─► Phase 10  ListBox (scrollable selection list)
    │
    └─► Phase 11  widget_showcase_demo (capstone, all widgets)
```

Phases 7–10 depend on Phase 6 (Label/Panel) but are independent of each other and can be implemented in any order. Phase 11 requires all previous phases.

---

## File Inventory

| Phase | New / Modified Files |
|-------|---------------------|
| 5 | `include/vde/api/GameTypes.h` *(modified: add Rect)*, `include/vde/api/TextLayout.h`, `src/api/TextLayout.cpp`, `include/vde/api/Widget.h`, `src/api/Widget.cpp`, `include/vde/api/UIManager.h`, `src/api/UIManager.cpp`, `tests/TextLayout_test.cpp`, `tests/Widget_test.cpp` |
| 6 | `include/vde/api/Label.h`, `src/api/Label.cpp`, `include/vde/api/Panel.h`, `src/api/Panel.cpp`, `tests/Label_test.cpp`, `tests/Panel_test.cpp` |
| 7 | `include/vde/api/TextInputBox.h`, `src/api/TextInputBox.cpp`, `tests/TextInputBox_test.cpp` |
| 8 | `include/vde/api/TextArea.h`, `src/api/TextArea.cpp`, `tests/TextArea_test.cpp` |
| 9 | `include/vde/api/Button.h`, `src/api/Button.cpp`, `include/vde/api/Checkbox.h`, `src/api/Checkbox.cpp`, `tests/Button_test.cpp`, `tests/Checkbox_test.cpp` |
| 10 | `include/vde/api/ListBox.h`, `src/api/ListBox.cpp`, `tests/ListBox_test.cpp` |
| 11 | `examples/widget_showcase_demo/main.cpp`, `smoketests/scripts/smoke_widget_showcase_demo.vdescript` |

**Modified across phases:** `CMakeLists.txt` (sources), `include/vde/api/GameAPI.h` (umbrella), `examples/CMakeLists.txt` (demo), `tests/CMakeLists.txt` (tests), `scripts/smoke-test.ps1` (smoke map)

---

## Design Notes

### Why SpriteEntity as the widget base?

Every VDE entity is scene-graph-based and renders via Vulkan. Widgets render their visuals by baking pixels into textures (same as TextEntity). This keeps the rendering path unified — no separate UI render pass, no additional pipeline. Widgets are entities that happen to respond to input.

### Why UIManager is user-owned, not engine-embedded?

VDE is a lightweight rendering engine, not a full UI framework. Making UIManager explicit keeps the engine non-opinionated. Games that don't need widgets pay nothing. Games that do create a UIManager and wire their InputHandler to it — one bridge function per event type.

### Content clipping strategy

TextArea and ListBox need content clipping. The approach is entity-level: lines outside the visible window are set to `setVisible(false)`. This avoids scissor-rect complexity in the Vulkan pipeline. Lines partially visible at the scroll boundary are rendered at offset but may peek beyond bounds — acceptable for a game UI toolkit (not pixel-perfect like a desktop GUI).

### Texture rebuild budget

Each widget that displays text (Label, TextInputBox, TextArea lines, Button, Checkbox label, ListBox items) uses the same lazy dirty-flag pattern as TextEntity. Texture rebuilds are batched to at most once per frame per widget. For TextArea/ListBox with many visible lines, only the lines whose text actually changed are rebuilt.

---

## Optional Future Work (Out of Scope)

These ideas are explicitly deferred and not part of this plan:

- **Layout managers** (flex, grid) — manual positioning is sufficient for the initial system
- **Slider / ProgressBar** — useful but not text-centric; can be added later following the same Widget pattern
- **Drag-and-drop** — complex interaction model, not needed for text widgets
- **Rich text / mixed fonts** — would require a fundamentally different TextRenderer; defer to a later plan
- **Tooltip** — hover-delayed popup text; simple to add later on top of UIManager hover tracking
- **Context menu** — right-click popup; can be built from Panel + ListBox when needed
