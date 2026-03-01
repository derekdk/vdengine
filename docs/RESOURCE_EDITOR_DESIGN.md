# VDE 2D Resource Editor — Design & Implementation Plan

## Overview

A 2D image resource editor tool built on the VDE Game API with a full ImGui interface. All operations — whether triggered by mouse, keyboard, or typed commands — are translated into a unified command language. Commands appear in a scrollable log and can be saved as replayable command scripts. The tool supports loading, editing, saving, and exporting 2D image assets, with persistent user settings via the Storage API.

The editor also supports the **Canvas DSL** (`.vdecanvas` files) — a declarative, text-based language for procedurally creating 2D image resources. DSL scripts describe a canvas, its color palette, named geometry, layers, and drawing operations. They compile down to sequences of low-level `CommandSystem` calls, bridging declarative authoring with the editor's command-first pipeline. See [CANVAS_DSL_DESIGN.md](CANVAS_DSL_DESIGN.md) for the full DSL specification.

### Core Design Principles

1. **Command-First Architecture** — Every mutation is a named command with arguments. Mouse/keyboard actions translate to commands before execution. Nothing changes state except through the command pipeline.
2. **Canvas-Per-Resource** — Each open resource gets its own **Canvas** with a unique **32-bit integer ID** and an optional human-readable string name (e.g., `"sprite_hero"`, `"tileset_grass"`). One canvas is the **active canvas** at any time. In commands and scripts, a canvas can be referenced by its string name or its numeric ID (e.g., `@3`). Commands operate on the active canvas by default, or target an explicit canvas with the `@<name>` or `@<id>` prefix.
3. **Observable Log** — Every executed command is appended to a time-stamped log. The log is the single source of truth for "what happened." Users can select log ranges and save them as replayable `.vdescript`-style command files.
4. **DSL as a Higher-Level Authoring Layer** — The Canvas DSL (`.vdecanvas`) sits above the command system. A DSL script is parsed into an AST and executed by emitting low-level commands through the `CommandSystem`. This means DSL operations are fully logged, undoable, and scriptable — they are not a parallel mutation path.
5. **Phased Delivery** — Phase 1 delivers a working editor with essential tools. Later phases add polish, advanced features, and the Canvas DSL.

---

## Architecture

### System Diagram

```
┌───────────────────────────────────────────────────────────────────────────┐
│                          ResourceEditorGame                               │
│    (BaseToolGame<ResourceEditorInputHandler, ResourceEditorScene>)        │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌──────────────────┐                                                     │
│  │  Canvas DSL       │   .vdecanvas script                                │
│  │  ┌──────────────┐ │                                                    │
│  │  │ Parser / AST  │ │   parse → validate → execute                      │
│  │  │ Expr Evaluator│ │                                                   │
│  │  │ Executor      │─┼──▶ emits low-level commands                       │
│  │  └──────────────┘ │                                                    │
│  └────────┬─────────┘                                                     │
│           │                                                               │
│           ▼                                                               │
│  ┌───────────────┐   ┌───────────────┐   ┌───────────────────────┐       │
│  │ CommandSystem  │──▶│  Command Log  │──▶│   Script Export       │       │
│  │ (dispatch +    │   │  (history)    │   │   (save/replay)       │       │
│  │  canvas resolve│   └───────────────┘   └───────────────────────┘       │
│  └───────┬───────┘                                                        │
│          │  resolves @canvasId or uses active canvas                       │
│          ▼                                                                │
│  ┌────────────────────────────────────────────────────────────────┐       │
│  │              CanvasRegistry  (ID → Canvas)                     │       │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌────────────┐    │       │
│  │  │ Canvas "hero"    │  │ Canvas "tiles"   │  │ Canvas ... │    │       │
│  │  │  ImageDocument   │  │  ImageDocument   │  │            │    │       │
│  │  │  GPU Texture     │  │  GPU Texture     │  │            │    │       │
│  │  │  Undo/Redo stack │  │  Undo/Redo stack │  │            │    │       │
│  │  │  Zoom/Pan state  │  │  Zoom/Pan state  │  │            │    │       │
│  │  └─────────────────┘  └─────────────────┘  └────────────┘    │       │
│  └────────────────────────────────────────────────────────────────┘       │
│                                                                           │
│  ┌──────────────┐   ┌────────────────────┐   ┌──────────────────┐        │
│  │ Tool Palette  │   │ ImGui Windows      │   │ Storage/Settings │        │
│  │ (brush, etc.) │   │ (per-canvas views) │   │ (StorageManager) │        │
│  └──────────────┘   └────────────────────┘   └──────────────────┘        │
└───────────────────────────────────────────────────────────────────────────┘
```

### File Organization

```
tools/
  ToolBase.h                            # Existing shared base
  resource_editor/
    CMakeLists.txt                      # Build configuration
    main.cpp                            # Entry point, mode selection
    ResourceEditorScene.h               # Scene declaration
    ResourceEditorScene.cpp             # Scene impl, ImGui layout, update loop
    CommandSystem.h                     # Command dispatch, log, script I/O
    CommandSystem.cpp                   # Command system implementation
    ImageDocument.h                     # Per-image document model
    ImageDocument.cpp                   # Pixel manipulation, undo buffer
    ToolPalette.h                       # Tool state (brush, eraser, picker, etc.)
    ToolPalette.cpp                     # Tool logic and mouse→command translation
    EditorPanels.h                      # ImGui panel drawing functions
    EditorPanels.cpp                    # All ImGui window code
    FileOperations.h                    # Load/save/export helpers
    FileOperations.cpp                  # File I/O with native dialogs
    README.md                           # Tool documentation
    dsl/                                # Canvas DSL subsystem
      CanvasDSLTypes.h                  # AST node types, symbol table, scope stack
      CanvasDSLParser.h                 # Tokenizer and AST builder (Pass 1)
      CanvasDSLParser.cpp
      CanvasDSLExprEval.h              # Expression evaluator with bound context
      CanvasDSLExprEval.cpp
      CanvasDSLExecutor.h              # AST walker that emits CommandSystem calls (Pass 2)
      CanvasDSLExecutor.cpp
```

---

## Component Details

### 1. CommandSystem (`CommandSystem.h/.cpp`)

The heart of the editor. Every operation flows through here.

```
Mouse click on canvas "hero" at (x,y) with brush tool
  → ToolPalette translates to: "paint 12 45 #FF0000FF"          (active canvas)
  → CommandSystem::execute("paint 12 45 #FF0000FF")
     → resolves target canvas: active canvas is "hero"
     → logged as: "paint 12 45 #FF0000FF"  (active was "hero")
     → dispatches to handler with resolved canvas
     → returns success/error message

Explicit targeting (REPL or script):
  → CommandSystem::execute("@tiles paint 0 0 #00FF00FF")
     → resolves target canvas: explicit "tiles"
     → logged as: "@tiles paint 0 0 #00FF00FF"
     → dispatches to handler with resolved canvas
```

**Responsibilities:**
- Maintain a `CommandRegistry` (command name → handler lambda)
- **Resolve canvas targeting** — parse optional `@canvasId` prefix; fall back to active canvas
- Execute commands, catch errors, log results
- Maintain an ordered `std::vector<CommandLogEntry>` of executed commands
- Support saving a range of log entries to a script file
- Support replaying a script file (line-by-line execution)
- Expose the log for ImGui rendering

**Key types:**

```cpp
struct CommandLogEntry {
    size_t index;              // Sequential number
    std::string timestamp;     // HH:MM:SS.mmm
    std::string commandLine;   // The full command as logged (includes @name if explicit)
    uint32_t canvasId;         // Resolved numeric canvas ID (0 for global commands)
    std::string canvasName;    // Resolved canvas name at the time of execution (for display)
    std::string result;        // Output/error message (may be empty)
    bool success;              // Did the command succeed?
};

/// Handler receives the resolved numeric canvasId (0 for global commands)
/// and a stream positioned after the command name for argument parsing.
class CommandSystem {
public:
    /// Canvas-targeted handler: receives (canvasId, args).
    using CanvasHandler = std::function<std::string(uint32_t canvasId,
                                                     std::istringstream& args)>;
    /// Global handler: not canvas-scoped (e.g., help, exit, list, set_active).
    using GlobalHandler = std::function<std::string(std::istringstream& args)>;

    void registerCommand(const std::string& name, const std::string& help,
                         CanvasHandler handler);
    void registerGlobalCommand(const std::string& name, const std::string& help,
                               GlobalHandler handler);

    bool execute(const std::string& commandLine);
    bool executeScript(const std::string& filePath);
    bool saveLogRange(size_t startIdx, size_t endIdx, const std::string& filePath);
    bool saveFullLog(const std::string& filePath);

    const std::vector<CommandLogEntry>& getLog() const;
    std::vector<std::string> getCommandNames() const;        // For tab-completion
    std::string getHelpText(const std::string& command) const;
    void clear();                                             // Clear log

    // Active canvas management
    void setActiveCanvasId(uint32_t id);
    uint32_t getActiveCanvasId() const;
    // Convenience: resolve a name-or-id string to a numeric ID.
    // Returns 0 if not found.
    uint32_t resolveCanvasId(const std::string& nameOrId) const;

private:
    /// Parses optional "@name" or "@id" prefix from commandLine.
    /// Returns {resolvedNumericId, remainingLine}. If no prefix, uses m_activeCanvasId.
    std::pair<uint32_t, std::string> resolveCanvas(const std::string& commandLine);
    std::pair<std::string, std::string> resolveCanvas(const std::string& commandLine);

    std::map<std::string, CanvasHandler> m_canvasHandlers;
    std::map<std::string, GlobalHandler> m_globalHandlers;
    std::map<std::string, std::string> m_helpText;
    std::vector<CommandLogEntry> m_log;
    size_t m_nextIndex = 0;
    uint32_t m_activeCanvasId = 0;   // 0 = no active canvas
    CanvasRegistry* m_registry = nullptr;  // Non-owning pointer for name→id resolution
};
```

**Canvas ID resolution rules:**

Every command line is parsed for an optional `@canvasId` prefix before the command name:

```
@canvasId command arg1 arg2 ...     ← targets canvas "canvasId" explicitly
command arg1 arg2 ...               ← targets the active canvas (set by set_active)
```

- The `@` prefix is **only** meaningful as the first token on a command line.
- Global commands (`help`, `exit`, `list`, `set_active`, `log`, `run`, `new`, `load`, `create image`, `dsl_load`) ignore the canvas prefix (and produce a warning if one is supplied).
- Canvas-targeted commands (`paint`, `fill`, `save`, `undo`, etc.) require a valid resolved canvas. If no canvas is active and no `@canvasId` is given, they return an error.
- When a command is logged, the full text including the `@canvasId` prefix (if explicit) is recorded. This means replaying the log reproduces the exact targeting.

**Built-in commands (registered during scene init):**

*Global commands (not canvas-scoped):*

| Command | Args | Description |
|---------|------|-------------|
| `help` | `[command]` | List commands or show help for one |
| `new` | `<width> <height> [name]` | Create a new blank canvas. `name` defaults to `"untitled_N"`. Assigns the next available `uint32_t` ID. Becomes the active canvas. |
| `load` | `<canvasname> "<filepath>" [imagename]` | Load an image file as a named resource in a canvas. If `canvasname` doesn't exist, creates a new canvas sized to the image. `imagename` defaults to filename stem. See §12.1 in CANVAS_DSL_DESIGN.md. |
| `create image` | `<name> <canvasname>[layers...] <areaname>` | Composite specified layers within an area into a new undisplayed image resource. Omit layers to use all. |
| `list` | | List all open canvases with their numeric IDs, names, dimensions, and dirty flags |
| `set_active` | `<name\|id>` | Set the active canvas by name or numeric ID |
| `log save` | `<filepath> [startIdx] [endIdx]` | Save command log to script file |
| `log clear` | | Clear command log |
| `run` | `<filepath>` | Execute a command script file |
| `dsl_load` | `<filepath.vdecanvas>` | Parse and execute a `.vdecanvas` DSL script. Creates a new canvas (named from the DSL's `create canvas` statement) and replays all drawing operations through the command system. |
| `dsl` | `<dsl statement>` | Execute a single Canvas DSL statement interactively (e.g., `dsl create color red #FF0000`). Useful for testing DSL snippets in the REPL. |
| `setcolor` | `<color>` | Set the active brush color (global palette state) |
| `settool` | `<toolname>` | Switch active tool (brush/eraser/picker/fill/line/rect/circle) |
| `setsize` | `<size>` | Set brush/tool size |
| `exit` | | Exit the editor |

*Canvas-targeted commands (operate on active canvas or explicit `@canvasId`):*

| Command | Args | Description |
|---------|------|-------------|
| `save` | `[filepath]` | Save canvas to its path or a new path |
| `saveas` | `<filepath>` | Save canvas to a new path |
| `export` | `<filepath> [format]` | Export canvas (png/bmp/tga) |
| `close` | | Close the canvas |
| `undo` | | Undo last operation on the canvas |
| `redo` | | Redo last undone operation on the canvas |
| `resize` | `<width> <height>` | Resize the canvas |
| `crop` | `<x> <y> <w> <h>` | Crop the canvas to a rectangle |
| `fill` | `<color>` | Fill entire canvas or selection with color |
| `paint` | `<x> <y> <color> [size]` | Paint a pixel/brush stroke |
| `line` | `<x1> <y1> <x2> <y2> <color> [size]` | Draw a line |
| `rect` | `<x> <y> <w> <h> <color> [filled]` | Draw a rectangle |
| `circle` | `<cx> <cy> <r> <color> [filled]` | Draw a circle |
| `draw` | `<imagename> [layer] <x> <y> <w> <h>` | Draw (blit) a named image resource at position and size. `imagename` can use `canvasname::imagename` for cross-canvas access. `[layer]` is an optional layer index in brackets. |
| `pick` | `<x> <y>` | Pick color from pixel (sets global brush color) |
| `zoom` | `<level>` | Set canvas zoom level |
| `pan` | `<dx> <dy>` | Pan the canvas view |
| `fliph` | | Flip image horizontally |
| `flipv` | | Flip image vertically |
| `dsl_export` | `<filepath.vdecanvas>` | Export the canvas's command history as a `.vdecanvas` DSL script (best-effort reconstruction). |
| `layer add` | `<name> [above\|below <ref> \| at <index>]` | Add a named layer to the compositing stack. `at <index>` inserts at that position (pushing existing layers up). |
| `layer remove` | `<name>` | Remove a layer |
| `layer select` | `<name\|index>` | Switch the active drawing layer |
| `layer opacity` | `<name> <0-100>` | Set layer opacity |
| `layer visibility` | `<name> <show\|hide>` | Toggle layer visibility |
| `gradient_fill` | `<direction> <stop1_pos> <stop1_color> ...` | Fill with a linear gradient |
| `copy_area` | `<x1> <y1> <x2> <y2> <dest_x> <dest_y> [fliph] [flipv]` | Copy a rectangular region |
| `floodfill` | `<x> <y> <color>` | Flood-fill contiguous pixels from a point |

### 2. ImageDocument (`ImageDocument.h/.cpp`)

Represents a single loaded/created image with undo/redo support. This is the pure pixel data model — it has no knowledge of canvas IDs or the command system.

**Responsibilities:**
- Own a CPU-side RGBA pixel buffer (`std::vector<uint8_t>`)
- Track dimensions, file path, dirty flag
- Provide pixel get/set and drawing primitives (line, rect, circle, fill)
- Maintain an undo/redo stack of pixel buffer snapshots
- Track a "generation" counter that increments on every mutation — the Canvas checks this to know when to re-upload the GPU texture

**Key interface:**

```cpp
class ImageDocument {
public:
    // Construction
    static std::unique_ptr<ImageDocument> createNew(uint32_t w, uint32_t h);
    static std::unique_ptr<ImageDocument> loadFromFile(const std::string& path);

    // Pixel access
    void setPixel(uint32_t x, uint32_t y, uint32_t rgba);
    uint32_t getPixel(uint32_t x, uint32_t y) const;
    const uint8_t* getPixelData() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    // Drawing primitives (operate on pixel buffer)
    void drawBrush(int cx, int cy, int radius, uint32_t color);
    void drawLine(int x1, int y1, int x2, int y2, uint32_t color, int thickness);
    void drawRect(int x, int y, int w, int h, uint32_t color, bool filled);
    void drawCircle(int cx, int cy, int r, uint32_t color, bool filled);
    void floodFill(int x, int y, uint32_t color);
    void fill(uint32_t color);
    void flipHorizontal();
    void flipVertical();
    void resize(uint32_t newW, uint32_t newH);
    void crop(int x, int y, uint32_t w, uint32_t h);

    // Undo/Redo
    void snapshotForUndo();      // Call before a mutation batch
    bool undo();
    bool redo();
    size_t getUndoCount() const;
    size_t getRedoCount() const;

    // Persistence
    bool saveToFile(const std::string& path);   // stb_image_write PNG
    bool exportToFile(const std::string& path);  // Supports png/bmp/tga by extension
    bool isDirty() const;
    void clearDirty();

    // GPU texture sync
    uint64_t getGeneration() const;             // Incremented on every pixel change
    const std::string& getFilePath() const;
    void setFilePath(const std::string& path);

private:
    std::vector<uint8_t> m_pixels;              // RGBA
    uint32_t m_width, m_height;
    std::string m_filePath;
    bool m_dirty = false;
    uint64_t m_generation = 0;

    // Undo stack — stores full pixel buffer snapshots
    // Phase 2 can optimize to delta-based undo
    std::vector<std::vector<uint8_t>> m_undoStack;
    std::vector<std::vector<uint8_t>> m_redoStack;
    static constexpr size_t kMaxUndoLevels = 50;
};
```

### 2b. Canvas — The Per-Resource Container

Each open resource is wrapped in a **Canvas** that bundles the `ImageDocument` with its display state, unique ID, and named image resources. The scene maintains a `CanvasRegistry` (map of ID → Canvas). One canvas is marked **active** at a time.

Each canvas also maintains an **operation history** — the ordered sequence of commands (including `load`) that produced its current pixel state. This enables deterministic recreation, script export, and undo/redo.

Resources (images) within a canvas are accessed from other canvases with the `::` double-colon accessor: `canvasname::imagename`. Within the owning canvas, bare `imagename` suffices.

```cpp
struct Canvas {
    uint32_t id;                                 // Unique numeric canvas ID (auto-assigned)
    std::string name;                             // Human-readable name (e.g. "hero", "tiles")
    std::unique_ptr<ImageDocument> document;      // Pixel data + undo
    std::map<std::string, std::unique_ptr<ImageDocument>> resources;  // Named image resources (loaded or composited)
    std::vector<std::string> operationHistory;    // Ordered commands for deterministic recreation
    std::shared_ptr<vde::Texture> gpuTexture;     // VDE texture for rendering
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE; // ImGui display handle
    uint64_t lastUploadedGeneration = 0;          // Track when GPU texture is stale
    float zoomLevel = 1.0f;                       // Per-canvas zoom
    float panX = 0.0f, panY = 0.0f;              // Per-canvas pan offset
};

/// Owns all open canvases and provides lookup by numeric ID or string name.
/// Supports cross-canvas resource resolution via the `::` accessor.
class CanvasRegistry {
public:
    Canvas* create(const std::string& name, std::unique_ptr<ImageDocument> doc);
    Canvas* getById(uint32_t id);
    Canvas* getByName(const std::string& name);
    /// Resolve a token that may be a numeric string ("3") or a name ("hero").
    Canvas* resolve(const std::string& nameOrId);
    bool remove(uint32_t id);
    bool has(uint32_t id) const;
    bool hasName(const std::string& name) const;
    std::vector<uint32_t> getIds() const;         // Ordered list of canvas IDs
    size_t count() const;

    /// Resolve a resource reference. Accepts bare "imagename" (searches active canvas)
    /// or "canvasname::imagename" (explicit canvas). Returns {canvas, imageDoc} or nulls.
    struct ResourceRef {
        Canvas* canvas = nullptr;
        ImageDocument* image = nullptr;
    };
    ResourceRef resolveResource(const std::string& ref, uint32_t activeCanvasId);

    // Generates a unique name like "untitled_1", "untitled_2", ...
    std::string generateUniqueName(const std::string& base = "untitled");

private:
    std::map<uint32_t, std::unique_ptr<Canvas>> m_canvases;
    std::map<std::string, uint32_t> m_nameIndex;   // name → id for fast lookup
    uint32_t m_nextId = 1;
    int m_nextUntitledIndex = 1;
};
```

### 3. ToolPalette (`ToolPalette.h/.cpp`)

Translates mouse interactions into commands.

**Responsibilities:**
- Track the currently active tool (brush, eraser, color picker, fill, line, rect, circle)
- Track brush color (RGBA), brush size, and other tool-specific parameters
- On mouse events, convert to the appropriate command string and return it
- Provide state accessors so ImGui panels can display current tool info

**Key types:**

```cpp
enum class EditorTool {
    Brush,
    Eraser,
    ColorPicker,
    Fill,
    Line,
    Rect,
    Circle
};

struct ToolState {
    EditorTool activeTool = EditorTool::Brush;
    uint32_t color = 0x000000FF;         // RGBA packed
    int brushSize = 1;
    bool shapeFilled = true;
};

class ToolPalette {
public:
    // Mouse-to-command translation.
    // canvasId is the ID of the canvas the mouse is over.
    // Returns a command string, or empty if no command (e.g., just hovering).
    // The returned command does NOT include @canvasId — the caller prepends it
    // only when the target differs from the active canvas.
    std::string onCanvasMouseDown(const std::string& canvasId, int canvasX, int canvasY);
    std::string onCanvasMouseDrag(const std::string& canvasId, int canvasX, int canvasY);
    std::string onCanvasMouseUp(const std::string& canvasId, int canvasX, int canvasY);

    // State
    ToolState& getState();
    const ToolState& getState() const;
    void setTool(EditorTool tool);
    void setColor(uint32_t rgba);
    void setBrushSize(int size);

    // For line/rect/circle — stores start point on mouse-down,
    // generates the shape command on mouse-up
    bool isDrawingShape() const;

private:
    ToolState m_state;
    bool m_drawing = false;
    int m_startX = 0, m_startY = 0;
    int m_lastX = 0, m_lastY = 0;
};
```

**Mouse interaction flow for shapes:**
1. Mouse-down → record start position, set `m_drawing = true`
2. Mouse-drag → for brush/eraser: emit `paint` commands per drag point. For shapes: update preview overlay (no command yet).
3. Mouse-up → for shapes: emit the final `line`/`rect`/`circle` command from start to end position.

### 4. EditorPanels (`EditorPanels.h/.cpp`)

All ImGui drawing code, organized as free functions or a helper class. Keeps ImGui concerns out of the scene class.

**Windows:**

| Window | Purpose |
|--------|---------|
| **Canvas Viewport(s)** | One ImGui window per visible canvas. Displays the image, handles mouse interaction, shows pixel grid at high zoom. Each viewport title includes the canvas ID. Clicking inside a canvas viewport implicitly runs `set_active <canvasId>` if it differs from the current active. |
| **Tool Palette** | Tool selection buttons, brush size slider, color picker |
| **Color Picker** | Detailed RGBA color editor with hex input |
| **Canvas Tabs** | Tab bar across the top showing open canvases by ID. Clicking a tab runs `set_active <canvasId>`. |
| **Properties Panel** | Shows canvas ID, image dimensions, file path, dirty flag, generation, zoom level for the active canvas |
| **Command Console** | REPL input field + scrollable command log |
| **Command Log** | Full log with selectable entries for script export |

**Key functions:**

```cpp
namespace EditorPanels {
    void drawMenuBar(CommandSystem& cmd, CanvasRegistry& canvases);
    void drawCanvasViewport(Canvas& canvas, ToolPalette& palette,
                            CommandSystem& cmd, bool isActive);
    void drawAllCanvasViewports(CanvasRegistry& canvases, ToolPalette& palette,
                                CommandSystem& cmd);
    void drawToolPalette(ToolPalette& palette, CommandSystem& cmd);
    void drawColorPicker(ToolPalette& palette, CommandSystem& cmd);
    void drawCanvasTabs(CanvasRegistry& canvases, CommandSystem& cmd);
    void drawPropertiesPanel(Canvas* activeCanvas);
    void drawCommandConsole(CommandSystem& cmd);
    void drawCommandLog(CommandSystem& cmd);
}
```

The canvas viewport is the most complex panel:
- Renders the `Canvas`'s GPU texture as an ImGui image (via `ImGui::Image` with the `Canvas::imguiTextureId`)
- Applies the canvas's per-instance zoom/pan transforms
- Translates ImGui mouse coordinates into canvas pixel coordinates
- **On mouse interaction:** if this canvas is not the active canvas, emits `set_active <canvasId>` first
- Delegates to `ToolPalette::onCanvasMouseDown/Drag/Up(canvasId, ...)` for command generation
- If the ToolPalette returns a command and the canvas is already active, executes it directly; if explicit targeting is needed (rare — e.g., scripted multi-canvas operations), prepends `@canvasId`
- Draws a shape preview overlay when dragging for line/rect/circle tools

### 5. FileOperations (`FileOperations.h/.cpp`)

File I/O helpers, separated for clarity and testability.

**Responsibilities:**
- Native file dialog wrappers (open/save) — reuse the pattern from `geometry_repl/FileDialog.h`
- Image loading via stb_image (already available in VDE's third_party)
- Image saving via stb_image_write (PNG, BMP, TGA)
- Command script file I/O (read lines, write lines)

```cpp
namespace FileOperations {
    std::string openFileDialog(const char* filter);
    std::string saveFileDialog(const char* filter, const char* defaultExt);
    
    bool loadImageFile(const std::string& path, std::vector<uint8_t>& outPixels,
                       uint32_t& outWidth, uint32_t& outHeight);
    bool saveImagePNG(const std::string& path, const uint8_t* pixels,
                      uint32_t width, uint32_t height);
    bool saveImageBMP(const std::string& path, const uint8_t* pixels,
                      uint32_t width, uint32_t height);
    bool saveImageTGA(const std::string& path, const uint8_t* pixels,
                      uint32_t width, uint32_t height);

    std::vector<std::string> readScriptLines(const std::string& path);
    bool writeScriptLines(const std::string& path,
                          const std::vector<std::string>& lines);
}
```

### 5b. Canvas DSL Integration (`dsl/`)

The Canvas DSL subsystem bridges `.vdecanvas` scripts with the editor's command-first architecture. A DSL script is a higher-level authoring language that compiles down to sequences of `CommandSystem` calls. See [CANVAS_DSL_DESIGN.md](CANVAS_DSL_DESIGN.md) for the full language specification.

**Data flow:**

```
.vdecanvas file
     │
     ▼
CanvasDSLParser      Pass 1: tokenize, build AST, resolve includes,
     │                       validate references (colors, palettes,
     │                       points, areas used before definition)
     ▼
AST (CanvasDSLTypes)  Validated tree of statements with symbol table
     │
     ▼
CanvasDSLExecutor    Pass 2: walk AST, evaluate expressions against
     │                       bound context, emit CommandSystem calls
     ▼
CommandSystem         Low-level commands (new, paint, fill, line, rect,
                      circle, layer select, save, export, ...)
```

**Key types (`dsl/CanvasDSLTypes.h`):**

```cpp
namespace vde::tools::dsl {

/// All DSL object types that can be created with 'create <type> <name>'
enum class ObjectType {
    Canvas, Image, Color, Palette, Point, Area, Layer, Gradient, Pattern, Macro
};

/// AST node tags
enum class NodeType {
    CreateCanvas, CreateImage, CreateColor, CreatePalette, CreatePoint, CreateArea,
    CreateLayer, CreateGradient, CreatePattern, CreateMacro,
    Metadata, SelectStmt, PaletteOp, LayerProp, VariableDef,
    DrawLine, DrawRect, DrawCircle, DrawEllipse, DrawArc, DrawBezier, DrawImage,
    FillCmd, SetPixel, FloodFill,
    CopyArea, MoveArea, ClearArea, TileArea,
    LoadImage, Resize, Crop, Flip, Rotate,
    ScopedBlock, RepeatLoop, ForLoop, IfStmt,
    Include, MacroCall, Export, Comment
};

/// A single node in the AST produced by the parser.
struct ASTNode {
    NodeType type;
    size_t lineNumber;                        // Source line for error reporting
    std::string sourceFile;                   // Source file path
    std::vector<std::string> tokens;          // Raw tokens for this statement
    std::vector<std::unique_ptr<ASTNode>> children;  // For blocks (in, repeat, for, if, macro)
};

/// Symbol table entry — tracks named objects (colors, points, areas, etc.)
struct Symbol {
    ObjectType objectType;
    std::string name;
    std::map<std::string, std::string> properties;  // Dot-notation properties
};

/// Scope for expression evaluation — pushed/popped for 'in <area>' blocks
struct BoundScope {
    int lb, rb, tb, bb;       // Current bounds
    int cx, cy;               // Current center
    int w, h;                 // Current dimensions
};

/// Parse result returned by CanvasDSLParser
struct ParseResult {
    bool success = false;
    std::vector<std::unique_ptr<ASTNode>> statements;
    std::vector<std::string> errors;           // "file:line: error: message"
    std::vector<std::string> warnings;
};

} // namespace vde::tools::dsl
```

**Parser (`dsl/CanvasDSLParser.h`):**

```cpp
namespace vde::tools::dsl {

class CanvasDSLParser {
public:
    /// Parse a .vdecanvas script file. Resolves includes relative to the
    /// script's directory. Validates all named references.
    ParseResult parse(const std::string& filePath);

    /// Parse from a string (for REPL / in-editor usage).
    ParseResult parseString(const std::string& source,
                            const std::string& virtualPath = "<string>");

private:
    struct Token { std::string text; size_t line; };

    std::vector<Token> tokenize(const std::string& source, const std::string& filePath);
    std::unique_ptr<ASTNode> parseStatement(/* ... */);
    void resolveIncludes(std::vector<std::unique_ptr<ASTNode>>& stmts,
                         const std::string& baseDir);
    void validateReferences(const std::vector<std::unique_ptr<ASTNode>>& stmts);

    std::map<std::string, Symbol> m_symbols;
    std::vector<std::string> m_errors;
    std::set<std::string> m_includedFiles;     // Prevent circular includes
};

} // namespace vde::tools::dsl
```

**Executor (`dsl/CanvasDSLExecutor.h`):**

```cpp
namespace vde::tools::dsl {

class CanvasDSLExecutor {
public:
    /// Execute a parsed DSL script by emitting commands through the
    /// given CommandSystem. Returns true if all commands succeeded.
    /// Errors are collected in the result.
    struct ExecuteResult {
        bool success = false;
        size_t commandsEmitted = 0;
        std::vector<std::string> errors;
    };

    ExecuteResult execute(const ParseResult& parsed, CommandSystem& cmd);

private:
    void executeNode(const ASTNode& node, CommandSystem& cmd);
    int evaluateExpr(const std::string& expr);   // Stack-based evaluator
    std::string resolveColor(const std::string& ref);  // Name → #RRGGBBAA

    std::vector<BoundScope> m_scopeStack;       // Canvas bounds at bottom
    std::map<std::string, Symbol> m_symbols;    // Populated from parse result
    std::map<std::string, int> m_variables;     // User-defined variables
};

} // namespace vde::tools::dsl
```

**DSL-to-Command mapping (key examples):**

| DSL Statement | Command(s) Emitted |
|---|---|
| `create canvas hero 32 32` | `new 32 32 hero` |
| `create color skin #FFCC99` | *(symbol table only — no command)* |
| `create palette hero_pal ...` | *(symbol table only — no command)* |
| `load hero "assets/face.png" face` | `load hero "assets/face.png" face` |
| `create image torso hero[base] body` | `create image torso hero[base] body` |
| `background bg` | `fill #00000000` (resolved color) |
| `set 10, 5 skin` | `paint 10 5 #FFCC99FF 1` |
| `fill body armor` | Sequence of `paint` or bulk `fill` within area bounds |
| `draw line p1 to p2 with c` | `line x1 y1 x2 y2 #color 1` |
| `draw rect p1 to p2 with c filled` | `rect x y w h #color filled` |
| `draw circle cx, cy radius r with c` | `circle cx cy r #color filled` |
| `draw face hero[0] 0, 0 32, 32` | `draw face [0] 0 0 32 32` (on resolved canvas) |
| `fill gradient sky` | Per-pixel `paint` commands (or future bulk gradient op) |
| `create layer highlights above` | `layer add highlights above` |
| `create layer overlay at 2` | `layer add overlay at 2` |
| `select layer highlights` | `layer select highlights` |
| `export png "output/hero.png"` | `export output/hero.png png` |
| `copy area to x, y` | Batch of `paint` commands (or `copy_area` command) |

**Integration with the editor:**

1. **Load DSL into editor** — The `dsl_load` command parses a `.vdecanvas` file and replays it through the `CommandSystem`. The resulting canvas appears as a normal editable canvas in the registry. All emitted commands appear in the log. `load` commands within the script are replayed to reload the image resources.
2. **Export canvas as DSL** — The `dsl_export` command reconstructs a `.vdecanvas` script from a canvas's operation history. This is a best-effort reconstruction that maps logged commands back to DSL statements.
3. **Batch execution** — When a `.vdecanvas` file is passed on the command line, the editor parses and executes it, then exits. This enables CI/CD asset generation from DSL scripts.
4. **REPL integration** — Individual DSL statements can be entered in the command console with a `dsl` prefix (e.g., `dsl create color red #FF0000FF`). This lets users test DSL snippets interactively.
5. **Cross-canvas resource access** — Image resources in one canvas can be referenced from another using the `::` double-colon accessor (e.g., `hero::face_img`). Within the owning canvas, bare names resolve directly.

**Error reporting:**

All DSL errors include the source file, line number, and a descriptive message:

```
hero_sprite.vdecanvas:14: error: undefined color 'armorr' (did you mean 'armor'?)
hero_sprite.vdecanvas:22: error: area 'torso' used before definition
hero_sprite.vdecanvas:35: error: duplicate name 'body' — an area with this name already exists
```

Parse errors are reported before any commands are emitted (fail-fast). Execution errors halt processing at the failing line and report how many commands were successfully emitted before the failure.

### 6. ResourceEditorScene (`ResourceEditorScene.h/.cpp`)

The glue. Inherits `BaseToolScene`, owns all subsystems, and wires them together.

**Responsibilities:**
- Own the `CommandSystem`, `ToolPalette`, `CanvasRegistry`, and DSL subsystem (`CanvasDSLParser`, `CanvasDSLExecutor`)
- Register all commands with `CommandSystem` during `onEnter()` — including `dsl_load`, `dsl`, and `dsl_export`
- Implement `executeCommand()` by delegating to `CommandSystem`
- Implement `drawDebugUI()` by calling `EditorPanels` functions
- Manage GPU textures: iterate `CanvasRegistry` each frame, re-upload when a canvas's document generation exceeds `lastUploadedGeneration`
- Create/destroy `ImTextureID` descriptor sets per canvas for displaying textures in ImGui
- Track the active canvas ID in `CommandSystem` and sync it with UI tab/viewport focus
- On enter: load settings from `StorageManager` (last window layout, recent files, last tool/color, last active canvas)
- On exit: save settings to `StorageManager`
- Use `deferCommand()` for entity mutations triggered from ImGui
- Route `.vdecanvas` files passed on the command line through the DSL parser/executor instead of the command script replay path

**Settings stored via StorageManager:**

| Key | Type | Description |
|-----|------|-------------|
| `reseditor.recent_files` | string | JSON array of recent file paths |
| `reseditor.last_tool` | string | Last active tool name |
| `reseditor.last_color` | string | Last brush color as hex |
| `reseditor.brush_size` | string | Last brush size |
| `reseditor.window_width` | string | Last window width |
| `reseditor.window_height` | string | Last window height |
| `reseditor.zoom_level` | string | Last zoom level |
| `reseditor.active_canvas` | string | Last active canvas name (names are stable across sessions; numeric IDs are not persisted) |

### 7. main.cpp

Standard tool entry point following ToolBase patterns:

```cpp
int main(int argc, char** argv) {
    ToolMode mode = ToolMode::INTERACTIVE;
    std::string scriptFile;

    if (argc > 1) {
        scriptFile = argv[1];
        mode = ToolMode::SCRIPT;
    }

    ResourceEditorGame tool(mode, scriptFile);

    if (mode == ToolMode::INTERACTIVE) {
        float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
        uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
        uint32_t height = static_cast<uint32_t>(900 * dpiScale);
        return runTool(tool, "VDE Resource Editor", width, height, argc, argv);
    } else {
        vde::GameSettings settings;
        settings.windowWidth = 800;
        settings.windowHeight = 600;
        settings.windowTitle = "VDE Resource Editor (Script)";
        settings.enableValidation = false;

        if (!tool.initialize(settings)) {
            std::cerr << "Failed to initialize resource editor\n";
            return 1;
        }
        tool.run();
        return tool.getExitCode();
    }
}
```

### 8. CMakeLists.txt

```cmake
# Resource Editor
# 2D image resource editor with command-driven architecture

add_executable(vde_resource_editor
    main.cpp
    ResourceEditorScene.cpp
    ResourceEditorScene.h
    CommandSystem.cpp
    CommandSystem.h
    ImageDocument.cpp
    ImageDocument.h
    ToolPalette.cpp
    ToolPalette.h
    EditorPanels.cpp
    EditorPanels.h
    FileOperations.cpp
    FileOperations.h
    dsl/CanvasDSLTypes.h
    dsl/CanvasDSLParser.cpp
    dsl/CanvasDSLParser.h
    dsl/CanvasDSLExprEval.cpp
    dsl/CanvasDSLExprEval.h
    dsl/CanvasDSLExecutor.cpp
    dsl/CanvasDSLExecutor.h
)

target_link_libraries(vde_resource_editor PRIVATE
    vde
    imgui_backend
)

if(WIN32)
    target_link_libraries(vde_resource_editor PRIVATE ole32 uuid)
endif()

add_dependencies(vde_resource_editor copy_tool_shaders)

add_custom_command(TARGET vde_resource_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:vde_resource_editor>/shaders
    COMMENT "Copying shaders to resource editor directory..."
)
```

---

## Command Workflow Examples

### Example 1: Single-Canvas Interactive Session

User opens the editor, creates an image, paints, saves.

```
# User clicks "New 32x32" in UI — translated to:
> new 32 32 sprite_hero
  Created canvas 'sprite_hero' (32x32). Active canvas set to 'sprite_hero'.

# User selects brush tool, red color — translated to:
> settool brush
  Tool set to brush
> setcolor #FF0000FF
  Color set to #FF0000FF

# User clicks at pixel (10, 5) on the active canvas — translated to:
> paint 10 5 #FF0000FF 1
  [sprite_hero] Painted at (10,5)

# User drags across pixels — each point generates:
> paint 11 5 #FF0000FF 1
> paint 12 5 #FF0000FF 1
> paint 12 6 #FF0000FF 1

# User uses color picker at (10,5) — translated to:
> pick 10 5
  [sprite_hero] Picked color #FF0000FF

# User saves — translated to:
> save assets/hero_sprite.png
  [sprite_hero] Saved to assets/hero_sprite.png
```

### Example 2: Multi-Canvas Session

User works with two images simultaneously.

```
# Create two canvases:
> new 32 32 hero
  Created canvas 'hero' (32x32). Active canvas set to 'hero'.
> new 16 16 bullet
  Created canvas 'bullet' (16x16). Active canvas set to 'bullet'.

# Paint on the active canvas (bullet):
> paint 8 8 #FFFF00FF 2
  [bullet] Painted at (8,8)

# Switch active canvas by clicking the "hero" tab:
> set_active hero
  Active canvas set to 'hero'.

# Paint on hero (now active):
> paint 16 16 #FF0000FF 1
  [hero] Painted at (16,16)

# Explicitly target bullet without switching active:
> @bullet fill #0000FFFF
  [bullet] Filled with #0000FFFF

# Active is still hero — this paints on hero:
> paint 17 16 #FF0000FF 1
  [hero] Painted at (17,16)

# List open canvases:
> list
  * hero    32x32  (active, modified)
    bullet  16x16  (modified)
```

(The `list` command shows numeric ID, name, dimensions, and state.)

```
> list
  id  name    dimensions  state
   1  hero    32x32       active, modified
   2  bullet  16x16       modified

# Save both:
> save assets/hero.png
  [hero] Saved to assets/hero.png
> @bullet save assets/bullet.png
  [bullet] Saved to assets/bullet.png
```

### Example 3: Saving Log as Script

After a session, the user selects log entries and saves. The exported script preserves explicit canvas targeting so it replays correctly regardless of active state:

```
# hero_and_bullet.txt
new 32 32 hero
new 16 16 bullet
@bullet paint 8 8 #FFFF00FF 2
set_active hero
paint 16 16 #FF0000FF 1
@bullet fill #0000FFFF
paint 17 16 #FF0000FF 1
@hero save assets/hero.png
@bullet save assets/bullet.png
```

Note: When exporting a log range as a script, the system **normalizes** canvas-targeted commands. Commands that ran on the active canvas are exported **with** an explicit `@canvasId` prefix if the active canvas at that point in the log might differ during replay (conservative approach). Alternatively, `set_active` commands are inserted to match the original active state.

### Example 4: Script Replay

```bash
vde_resource_editor.exe hero_and_bullet.txt
```

Executes all commands in batch, produces both PNGs, and exits.

### Example 5: Canvas DSL Workflow

User loads a `.vdecanvas` script, which creates a canvas and draws procedurally:

```
# Load a DSL script — creates canvas 'hero', draws the full sprite:
> dsl_load assets/hero_sprite.vdecanvas
  Parsed hero_sprite.vdecanvas (0 errors, 0 warnings).
  Executing... 47 commands emitted.
  Created canvas 'hero' (32x32). Active canvas set to 'hero'.

# The command log shows every low-level operation:
> log
  [1] 10:05:30.100  new 32 32 hero
  [2] 10:05:30.101  fill #00000000
  [3] 10:05:30.102  @hero paint 10 5 #FFCC99FF 1
  [4] 10:05:30.103  @hero line 8 2 23 2 #8B4513FF 2
  ... (47 entries)

# The canvas is now editable like any other — make manual tweaks:
> paint 15 15 #FF0000FF 1
  [hero] Painted at (15,15)

# Export the result:
> export output/hero_sprite.png png
  [hero] Exported to output/hero_sprite.png

# Or export back to DSL (best-effort reconstruction):
> dsl_export assets/hero_sprite_v2.vdecanvas
  [hero] Exported DSL script to assets/hero_sprite_v2.vdecanvas (52 statements)
```

### Example 6: Batch DSL Execution (CI/CD)

Run a `.vdecanvas` file from the command line to generate assets without a GUI:

```bash
# Generate a sprite from a DSL script:
vde_resource_editor.exe --dsl assets/hero_sprite.vdecanvas

# The script's 'export' directive writes the output file.
# The editor exits after execution with code 0 on success.
```

This enables asset pipelines where `.vdecanvas` scripts are version-controlled source files and output images are build artifacts.

### Example 7: Interactive DSL Snippets

Test DSL statements one at a time in the REPL console:

```
> new 16 16 test
  Created canvas 'test' (16x16). Active canvas set to 'test'.

# Enter DSL statements with the 'dsl' prefix:
> dsl create color red #FF0000FF
  [dsl] Registered color 'red'
> dsl create color blue #0000FFFF
  [dsl] Registered color 'blue'
> dsl draw line 0, 0 to 15, 15 with red
  [test] line 0 0 15 15 #FF0000FF 1
> dsl draw circle 8, 8 radius 5 with blue filled
  [test] circle 8 8 5 #0000FFFF filled
```

### Example 8: Loading Images and Cross-Canvas Drawing

Load external images as resources and compose them across canvases:

```
# Load an image — creates canvas 'hero' sized to the image:
> load hero "assets/face_template.png"
  Loaded 'assets/face_template.png' as 'face_template' in new canvas 'hero' (32x32).
  Active canvas set to 'hero'.

# Load another image into the same canvas (added as undisplayed resource):
> load hero "assets/badge.png" badge
  Loaded 'assets/badge.png' as 'badge' into canvas 'hero'.

# Draw the badge onto the hero canvas at a specific position:
> draw badge [0] 2 2 8 8
  [hero] Drew 'badge' at (2,2) size 8x8 on layer 0

# Create a sprite sheet canvas and draw images from hero into it:
> new 128 128 sheet
  Created canvas 'sheet' (128x128). Active canvas set to 'sheet'.
> draw hero::face_template [0] 0 0 32 32
  [sheet] Drew 'hero::face_template' at (0,0) size 32x32 on layer 0
> draw hero::badge [0] 32 0 8 8
  [sheet] Drew 'hero::badge' at (32,0) size 8x8 on layer 0

# Create an image resource by compositing layers:
> create image torso_sprite hero[base, outline] body_area
  Created image 'torso_sprite' in canvas 'hero' from layers [base, outline] area 'body_area'.

# Save the sheet:
> save assets/spritesheet.png
  [sheet] Saved to assets/spritesheet.png
```

---

## Phased Implementation Plan

### Phase 1 — Minimum Viable Editor (Target: Workable)

The goal is a running tool that can create, edit, save, and reload simple pixel art. All core infrastructure is in place. UI can be rough but functional.

#### Step 1.1: Project Scaffolding
- [ ] Create `tools/resource_editor/` directory
- [ ] Create `CMakeLists.txt` with build config
- [ ] Add `add_subdirectory(resource_editor)` to `tools/CMakeLists.txt`
- [ ] Create `main.cpp` with interactive/script mode selection
- [ ] Create stub `ResourceEditorScene.h/.cpp` inheriting `BaseToolScene`
- [ ] Create stub `ResourceEditorGame` inheriting `BaseToolGame`
- [ ] Verify the tool builds and launches with an empty scene

#### Step 1.2: Command System & Canvas Resolution
- [ ] Create `CommandSystem.h/.cpp`
- [ ] Implement `resolveCanvas()` — parse optional `@canvasId` prefix from command line, fall back to `m_activeCanvasId`
- [ ] Implement dual registration: `registerCommand()` (canvas-targeted) and `registerGlobalCommand()` (not canvas-scoped)
- [ ] Implement `CommandLogEntry` with timestamp, result, and `canvasId` tracking
- [ ] Implement `execute()` with canvas resolution, `getLog()`, `getCommandNames()`, `getHelpText()`
- [ ] Implement `setActiveCanvasId()` / `getActiveCanvasId()` (uint32_t)
- [ ] Implement `resolveCanvasId()` — accept name or decimal string, return uint32_t via CanvasRegistry
- [ ] Implement `saveFullLog()` and `saveLogRange()` with smart `@canvasId` normalization for replay
- [ ] Implement `executeScript()` for script replay (line-by-line)
- [ ] Register `help`, `exit`, `set_active`, `list`, `log save`, `log clear`, `run` global commands
- [ ] Write unit tests for command dispatch, canvas resolution, and log

#### Step 1.3: Image Document & Canvas Registry
- [ ] Create `ImageDocument.h/.cpp`
- [ ] Implement `createNew()` and pixel buffer allocation (RGBA)
- [ ] Implement `setPixel()`, `getPixel()`, `fill()`
- [ ] Implement `drawBrush()` (single pixel and radius)
- [ ] Implement `drawLine()` (Bresenham's)
- [ ] Implement `drawRect()` (filled and outline)
- [ ] Implement undo/redo with full-buffer snapshots
- [ ] Implement `saveToFile()` via stb_image_write (PNG)
- [ ] Implement `loadFromFile()` via stb_image
- [ ] Implement generation counter for GPU sync
- [ ] Create `Canvas` struct with `uint32_t id`, `std::string name`, ImageDocument + GPU texture + zoom/pan state + resources map + operation history
- [ ] Create `CanvasRegistry` class with `create()`, `getById()`, `getByName()`, `resolve()`, `remove()`, `has()`, `hasName()`, `getIds()`, `generateUniqueName()`
- [ ] Implement `::` resource resolution — `resolveResource(token)` parses `canvasname::imagename` or bare `imagename` against active canvas
- [ ] Maintain `m_nameIndex` (name → id) in `CanvasRegistry` for O(1) name lookups
- [ ] Write unit tests for pixel operations, undo/redo, and canvas registry (including name/id resolution)

#### Step 1.4: File Operations
- [ ] Create `FileOperations.h/.cpp`
- [ ] Implement `loadImageFile()` wrapping stb_image
- [ ] Implement `saveImagePNG()` wrapping stb_image_write
- [ ] Implement native file dialogs (Windows COM, following geometry_repl pattern)
- [ ] Implement script file read/write helpers

#### Step 1.5: Tool Palette
- [ ] Create `ToolPalette.h/.cpp`
- [ ] Implement `EditorTool` enum and `ToolState`
- [ ] Implement `onCanvasMouseDown/Drag/Up(canvasId, x, y)` for Brush tool → `paint` commands
- [ ] Implement `onCanvasMouseDown(canvasId, ...)` for ColorPicker tool → `pick` commands
- [ ] Implement `onCanvasMouseDown(canvasId, ...)` for Fill tool → `fill` commands
- [ ] Implement Eraser tool (brush with transparent color)
- [ ] Shape tools (line/rect/circle) — record start on mouse-down, emit on mouse-up
- [ ] Palette methods return bare commands; caller prepends `@canvasId` only when targeting non-active canvas

#### Step 1.6: Editor Panels (ImGui)
- [ ] Create `EditorPanels.h/.cpp`
- [ ] Implement `drawCommandConsole()` — REPL input + scrollable log output (shows `[canvasId]` prefix on canvas-targeted results)
- [ ] Implement `drawToolPalette()` — tool buttons + brush size + color swatch
- [ ] Implement `drawColorPicker()` — ImGui::ColorEdit4 with hex input
- [ ] Implement `drawCanvasViewport(Canvas&, ...)` — render per-canvas texture with its zoom/pan, handle mouse events, emit `set_active` on click if not already active, translate to palette commands
- [ ] Implement `drawAllCanvasViewports()` — iterate `CanvasRegistry` and draw a viewport per canvas
- [ ] Implement `drawCanvasTabs()` — tab bar per open canvas showing canvas ID, clicking a tab runs `set_active`
- [ ] Implement `drawPropertiesPanel()` — shows active canvas ID, image dimensions, path, dirty state
- [ ] Implement `drawMenuBar()` — File (New/Open/Save/SaveAs/Export/Exit), Edit (Undo/Redo), View (Zoom)

#### Step 1.7: Scene Integration
- [ ] Wire `CommandSystem` and `CanvasRegistry` into `ResourceEditorScene`
- [ ] Register global commands: `new`, `load`, `create image`, `list`, `set_active`, `setcolor`, `settool`, `setsize`, `help`, `exit`, `log save`, `log clear`, `run`
- [ ] Register canvas-targeted commands: `save`, `saveas`, `export`, `close`, `undo`, `redo`, `fill`, `paint`, `line`, `rect`, `circle`, `draw` (image blit), `pick`, `zoom`, `pan`, `fliph`, `flipv`, `resize`, `crop`, `floodfill`, `copy_area`, `gradient_fill`
- [ ] Register layer commands: `layer add` (with `above`/`below`/`at <index>` support), `layer remove`, `layer select`, `layer opacity`, `layer visibility`
- [ ] In `new` / `load` handlers: create `Canvas` in `CanvasRegistry` (assigns uint32_t id + name) and call `cmd.setActiveCanvasId(id)`. `load` records the operation in canvas history.
- [ ] In canvas-targeted handlers: look up `Canvas` from `CanvasRegistry` by resolved uint32_t `canvasId`; return error if not found
- [ ] Implement `::` resource resolution in `draw` handler — parse `canvasname::imagename` to locate resources across canvases
- [ ] Create VDE `Texture` per canvas, re-upload when document generation exceeds `canvas.lastUploadedGeneration`
- [ ] Create/cache ImGui texture descriptor sets per canvas for display
- [ ] Call `EditorPanels` functions from `drawDebugUI()`
- [ ] Handle script mode: load and execute script file, then quit

#### Step 1.8: Storage Integration
- [ ] Initialize `StorageManager` with app name `"vde_resource_editor"`
- [ ] Save/load recent file list on startup/shutdown
- [ ] Save/load last tool, color, brush size
- [ ] Save/load window dimensions

#### Step 1.9: Smoke Test
- [ ] Create `smoketests/scripts/smoke_resource_editor.vdescript` — launch, wait, press F1, wait, exit
- [ ] Create a sample command script `tools/resource_editor/scripts/test_basic.txt` that exercises `new` (creates canvas), `set_active`, `paint`, `save`, `exit`
- [ ] Create a multi-canvas command script `tools/resource_editor/scripts/test_multi_canvas.txt` that opens two canvases, paints on each (using `set_active` and `@canvasId`), saves both
- [ ] Verify both interactive and script modes work

---

### Phase 2 — Usability & Polish

- [ ] **Pixel grid overlay** — draw grid lines at high zoom levels in the canvas viewport
- [ ] **Shape preview** — show ghost outline of line/rect/circle while dragging
- [ ] **drawCircle()** — Midpoint circle algorithm
- [ ] **floodFill()** — Scanline flood fill algorithm
- [ ] **Selection tool** — rectangular selection, move, copy, paste as commands
- [ ] **Flip operations** — `fliph`, `flipv` commands
- [ ] **Resize/crop** — `resize`, `crop` commands with pixel resampling
- [ ] **BMP/TGA export** — `saveImageBMP()`, `saveImageTGA()` in FileOperations
- [ ] **Tab-completion** in REPL console (match command names)
- [ ] **Keyboard shortcuts** — Ctrl+Z (undo), Ctrl+Y (redo), Ctrl+S (save), Ctrl+N (new), B (brush), E (eraser), G (fill), I (picker)
- [ ] **DPI-aware layout** — scale all ImGui window sizes by `getDPIScale()`
- [ ] **Recent files menu** — "File > Open Recent" using StorageManager data
- [ ] **Dirty indicator** — show `*` in document tab when unsaved
- [ ] **Confirm on close** — prompt to save dirty documents before closing

### Phase 3 — Canvas DSL

The Canvas DSL enables declarative, replayable, text-based image authoring. This phase implements the DSL subsystem and integrates it with the editor.

#### Step 3.1: DSL Types & Parser
- [ ] Create `dsl/CanvasDSLTypes.h` — AST node types, symbol table, scope stack structs
- [ ] Create `dsl/CanvasDSLParser.h/.cpp` — tokenizer, AST builder, include resolution
- [ ] Implement Pass 1: tokenize lines, handle comments (`//`, `/* */`), blank lines
- [ ] Parse `create canvas`, `create color`, `create palette` statements
- [ ] Parse `create point`, `create area` (both `to` and `size` forms, optional canvas), `create layer` (with `above`/`below`/`at <index>`, optional canvas), `create image` statements
- [ ] Parse `create gradient`, `create pattern`, `create macro` statements
- [ ] Parse drawing commands: `set`, `fill`, `draw line/rect/circle/ellipse/arc/bezier`, `draw <imagename>` (image blit), `floodfill`
- [ ] Parse `load <canvasname> "<filepath>" [imagename]` statements
- [ ] Parse area operations: `copy`, `move`, `clear`, `tile`
- [ ] Parse control flow: `repeat`, `for`, `if/else`, `in <area>` scoped blocks
- [ ] Parse `include`, macro calls, `export` directives
- [ ] Implement symbol table — register named objects, detect duplicates, suggest corrections
- [ ] Implement reference validation — verify colors, palettes, points, areas, images are defined before use
- [ ] Implement `::` cross-canvas resource resolution (e.g., `hero::face_img`)
- [ ] Implement `include` resolution — read included files, detect circular includes
- [ ] Error reporting with file path, line number, and descriptive messages
- [ ] Write unit tests for parser: valid scripts, syntax errors, undefined references, circular includes

#### Step 3.2: Expression Evaluator
- [ ] Create `dsl/CanvasDSLExprEval.h/.cpp` — stack-based expression evaluator
- [ ] Evaluate integer literals, bound references (`lb`, `rb`, `tb`, `bb`, `cx`, `cy`, `w`, `h`)
- [ ] Evaluate arithmetic (`+`, `-`, `*`, `/`), percentage expressions (`50%w`)
- [ ] Evaluate dot-notation property access (`head.cx`, `mypal.count`, `my_sprite.width`)
- [ ] Evaluate `::` cross-canvas accessor (`hero::badge`, `sheet::texture`)
- [ ] Evaluate palette index lookups (`mypal[armor]`, `mypal[0]`)
- [ ] Implement scope stack — push/pop for `in <area>` blocks
- [ ] Implement variable storage and expansion
- [ ] Write unit tests for expression evaluation with various bound contexts

#### Step 3.3: DSL Executor
- [ ] Create `dsl/CanvasDSLExecutor.h/.cpp` — AST walker emitting CommandSystem calls
- [ ] Map `create canvas` → `new <w> <h> <name>`
- [ ] Map `background` / `fill` → `fill <resolved_color>`
- [ ] Map `set <point> <color>` → `paint <x> <y> <resolved_color> 1`
- [ ] Map `draw line/rect/circle` → `line`/`rect`/`circle` commands
- [ ] Map `load <canvas> "<path>" [name]` → `load <canvas> "<path>" [name]` command
- [ ] Map `create image <name> <canvas>[layers] <area>` → `create image` command
- [ ] Map `draw <imagename> <canvas>[layer] <pos> <size>` → `draw` (image blit) command
- [ ] Map `create layer` / `select layer` → `layer add`/`layer select` commands (including `at <index>` form)
- [ ] Map `create area` → area registration (both `to` and `size` syntax, optional canvas)
- [ ] Map `export` → `export <path> <format>`
- [ ] Handle `in <area>` scope pushing/popping during execution
- [ ] Handle `repeat`/`for` loops — unroll and emit commands
- [ ] Handle `if/else` conditionals — evaluate and branch
- [ ] Handle macro expansion — inline macro body with parameter substitution
- [ ] Handle gradient fills — compute per-pixel colors, emit `paint` sequence
- [ ] Handle pattern fills — tile pattern data, emit `paint` sequence
- [ ] Handle area operations (`copy`, `move`, `tile`) — emit `copy_area`/`paint` sequences
- [ ] Fail-fast on execution errors — report line, commands emitted so far
- [ ] Write unit tests for executor: simple scripts, loops, macros, error cases

#### Step 3.4: Editor Integration
- [ ] Register `dsl_load` global command — parse + execute `.vdecanvas` file
- [ ] Register `dsl` global command — parse + execute single DSL statement (REPL)
- [ ] Register `dsl_export` canvas-targeted command — reconstruct `.vdecanvas` from command history
- [ ] Add "File > Load DSL Script..." menu item in EditorPanels
- [ ] Add "File > Export as DSL..." menu item in EditorPanels
- [ ] Support `--dsl <file.vdecanvas>` command-line flag for batch execution (parse, execute, exit)
- [ ] Update `main.cpp` to detect `.vdecanvas` extension and route to DSL execution
- [ ] Update FileOperations to handle `.vdecanvas` and `.vdepalette` file filters in dialogs
- [ ] Write integration tests: load DSL → verify canvas pixels → export DSL → reload → compare

#### Step 3.5: DSL Smoke Tests
- [ ] Create `tools/resource_editor/scripts/test_dsl_basic.vdecanvas` — simple canvas with colors, shapes
- [ ] Create `tools/resource_editor/scripts/test_dsl_load_draw.vdecanvas` — load images, draw cross-canvas, create image compositing
- [ ] Create `tools/resource_editor/scripts/test_dsl_layers.vdecanvas` — multi-layer canvas with `at <index>` positioning
- [ ] Create `tools/resource_editor/scripts/test_dsl_macros.vdecanvas` — macro definitions and calls
- [ ] Create `tools/resource_editor/scripts/test_dsl_include.vdecanvas` — include directive test
- [ ] Create `tools/resource_editor/scripts/shared_colors.vdepalette` — shared palette file
- [ ] Verify batch execution produces expected output images

### Phase 4 — Advanced Features

- [ ] **Delta-based undo** — store only changed pixel regions instead of full snapshots to reduce memory
- [ ] **Sprite sheet mode** — grid overlay with frame navigation, animation preview
- [ ] **Palette management UI** — define/edit a color palette interactively, restrict painting to palette colors. Palettes defined in the DSL or via `.vdepalette` files can be loaded into the palette panel.
- [ ] **Tiling preview** — show the image tiled 3x3 for seamless texture authoring
- [ ] **Import/export formats** — support for additional formats (JPEG import, ICO export)
- [ ] **Macro recording** — named command macros (not just log export) that can be bound to keys. Macros can be exported as DSL `create macro` definitions.
- [ ] **ImGui docking** — enable docking layout so users can rearrange panels
- [ ] **Multi-document diffing** — compare two images side by side
- [ ] **DSL visual debugger** — step through `.vdecanvas` execution line-by-line, highlighting affected pixels
- [ ] **DSL autocomplete** — in the REPL, offer color names, point/area names, and macro names from the DSL symbol table
- [ ] **Parametric DSL scripts** — accept arguments from the command line (`dsl_load hero.vdecanvas --color_scheme=dark`)

---

## Implementation Notes for AI Agent

### Build & Test Cycle

```powershell
# Build:
# Use VS Code task 'scripts: build' or:
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1

# Test:
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test.ps1

# Run the editor:
./build_ninja/bin/vde_resource_editor.exe

# Run a command script:
./build_ninja/bin/vde_resource_editor.exe tools/resource_editor/scripts/test_basic.txt

# Run a DSL script (batch mode — execute and exit):
./build_ninja/bin/vde_resource_editor.exe --dsl tools/resource_editor/scripts/test_dsl_basic.vdecanvas
```

### Key Patterns to Follow

1. **ToolBase.h inheritance** — Follow the `geometry_repl` pattern exactly. `ResourceEditorGame` extends `BaseToolGame<ResourceEditorInputHandler, ResourceEditorScene>`.
2. **configureInputScriptFromArgs before setWorkingDirectoryToExecutablePath** — The `runTool()` helper handles this, just pass `argc, argv`.
3. **deferCommand() for render-phase mutations** — Any entity add/remove from `drawDebugUI()` or `onRender()` must go through `deferCommand()`.
4. **Texture display in ImGui** — Create a `VkDescriptorSet` from the texture's `VkImageView` + `VkSampler` using `ImGui_ImplVulkan_AddTexture()`. Store it in `Canvas::imguiTextureId`. Only recreate when the texture is re-uploaded (i.e., when `canvas.lastUploadedGeneration < doc.getGeneration()`).
5. **stb_image / stb_image_write** — Already available in `third_party/stb/`. Use `#define STB_IMAGE_WRITE_IMPLEMENTATION` in exactly one `.cpp` file. Check if it's already defined elsewhere in the project to avoid duplicate symbols.
6. **Color format** — Use `#RRGGBBAA` hex strings in commands for full round-trip fidelity. Internally store as `uint32_t` packed RGBA.
7. **StorageManager init** — Call `StorageManager::getInstance().init_storage("vde_resource_editor")` during `onEnter()`. Call `shutdown()` during `onExit()`.
8. **Naming conventions** — PascalCase classes, camelCase methods, `m_` prefix for members, `#pragma once`, `vde::tools::` namespace.
9. **Include order** — Corresponding header first, then VDE headers, then third-party (ImGui, Vulkan, stb), then standard library.
10. **Error handling** — All commands must validate arguments and return clear error messages. Never crash on bad input.
11. **DSL integration** — The Canvas DSL subsystem lives under `dsl/` and only interfaces with the rest of the editor through `CommandSystem`. The parser and executor never directly mutate `ImageDocument` or `CanvasRegistry` — they always emit commands. This keeps the command log as the single source of truth.
12. **DSL file extensions** — `.vdecanvas` for canvas scripts, `.vdepalette` for shared palette definitions. Both are UTF-8 text files.
13. **Cross-canvas resource access (`::`)** — Use `canvasname::imagename` to reference image resources owned by another canvas. Within the active canvas, bare `imagename` suffices. Resolution order: local scope → active canvas resources → global objects.
14. **Canvas operation history** — Every canvas maintains an ordered log of the commands that produced its state. The `load` command is recorded so canvases can be recreated deterministically. This log is the basis for `dsl_export` script generation and undo/redo.
15. **Image resources** — Each canvas has a `std::map<std::string, std::unique_ptr<ImageDocument>> resources` map for loaded/composited images. These are not automatically displayed — use the `draw` command to blit them onto a layer.

### Testing Strategy

- **Unit tests** for `CommandSystem` (register, execute, canvas resolution with `@canvasId`, active canvas fallback, log, script I/O, `::` resource resolution)
- **Unit tests** for `CanvasRegistry` (create, getById, getByName, resolve, remove, generateUniqueName, resource map management)
- **Unit tests** for `ImageDocument` (create, pixel ops, undo/redo, save/load)
- **Unit tests** for `ToolPalette` (mouse-to-command translation with canvasId)
- **Unit tests** for `CanvasDSLParser` (tokenization, AST construction, include resolution, symbol validation, error messages, `::` accessor parsing)
- **Unit tests** for `CanvasDSLExprEval` (bound arithmetic, dot-notation, `::` accessor, palette lookups, scope stack push/pop)
- **Unit tests** for `CanvasDSLExecutor` (command emission for each DSL statement type, loop unrolling, macro expansion, error handling)
- **Integration tests** for DSL round-trip (load `.vdecanvas` → verify canvas pixels → export → reload → compare)
- **Smoke test** (`.vdescript`) for interactive launch verification
- **Command script test** (single canvas) for batch mode verification
- **Command script test** (multi-canvas with `set_active` and `@canvasId`) for multi-document batch verification
- **DSL script tests** (`.vdecanvas`) for batch DSL execution verification (basic shapes, layers, macros, includes)

### Completion Criteria per Step

Each step is done when:
1. Code compiles without warnings
2. Relevant unit tests pass
3. The tool launches and the new functionality is exercisable (interactive or script)
4. Commands produce correct log entries
5. No regressions in previously completed steps

---

## Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| VDE engine (`vde` library) | Root CMakeLists.txt | Core rendering, window, input, resources |
| Dear ImGui (docking) | FetchContent (already in tools/CMakeLists.txt) | All editor UI |
| stb_image | third_party/stb/ | Image file loading (PNG, BMP, TGA, JPEG) |
| stb_image_write | third_party/stb/ | Image file saving (PNG, BMP, TGA) |
| SQLite (via StorageManager) | Already in VDE | Persistent settings |
| Windows COM (ole32, uuid) | System | Native file dialogs |

No new external dependencies are required.
