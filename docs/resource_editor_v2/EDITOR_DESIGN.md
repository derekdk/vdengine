# VDE Resource Editor v2 — Editor Design

> **Status:** Design  
> **Supersedes:** [v1 Design](../history/resource_editor_v1/RESOURCE_EDITOR_DESIGN.md)  
> **Related:** [Canvas DSL](CANVAS_DSL.md) · [Parser & Command System](PARSER_AND_COMMAND_SYSTEM.md) · [Implementation Plan](IMPLEMENTATION_PLAN.md)

---

## 1. Overview

The VDE Resource Editor is a 2D pixel art editor built on the VDE Game API with a full ImGui interface. It supports creating, editing, saving, exporting, and procedurally generating 2D image assets.

### Why v2?

The v1 design established the right architectural principles — command-first, multi-canvas, layered DSL — but the implementation grew organically, leading to:

- A monolithic `ResourceEditorScene` acting as a god-class with ~25+ command handlers inline
- Ad-hoc argument parsing per handler with inconsistent patterns
- No structured metadata for commands (no auto-help, no validation, no autocomplete)
- Tightly coupled DSL types mixed with editor logic
- Object ownership permanently bound to canvases with no transfer/copy capability

**v2 starts from scratch** with a clean separation of concerns, a metadata-driven command system, and a modular file layout where each command is a self-contained unit.

### Core Design Principles

1. **Command-First Architecture** — Every mutation flows through a named command with typed parameters. Mouse/keyboard actions translate to commands before execution. Nothing changes state except through the command pipeline.

2. **One Command = One File** — Each command is a self-contained class with structured metadata (parameter types, descriptions, defaults). Adding a command means creating one file and one CMake entry.

3. **Metadata-Driven** — Command metadata powers auto-generated help, parameter validation before dispatch, REPL autocomplete, and ImGui tooltip hints.

4. **Canvas-Per-Resource** — Each open resource gets its own Canvas with a unique 32-bit ID and a human-readable name. One canvas is active at a time. Commands operate on the active canvas by default or target one explicitly with `@<name>`.

5. **Observable Log** — Every executed command is appended to a timestamped log. The log is the single source of truth. Users can export log ranges as replayable scripts.

6. **DSL as a Higher-Level Layer** — The Canvas DSL (`.vdecanvas`) compiles down to sequences of low-level commands. DSL operations are fully logged, undoable, and scriptable.

7. **Clean Module Boundaries** — The editor is decomposed into independent subsystems (CommandSystem, CanvasRegistry, ToolPalette, EditorPanels, FileOperations, DSL) that communicate through well-defined interfaces.

---

## 2. Architecture

### System Diagram

```
┌───────────────────────────────────────────────────────────────────────────┐
│                          ResourceEditorGame                               │
│    (BaseToolGame<BaseToolInputHandler, ResourceEditorScene>)              │
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
│  │ (registry +    │   │  (history)    │   │   (save/replay)       │       │
│  │  arg parser +  │   └───────────────┘   └───────────────────────┘       │
│  │  dispatch)     │                                                       │
│  └───────┬───────┘                                                        │
│          │  resolves @canvasId or uses active canvas                       │
│          ▼                                                                │
│  ┌────────────────────────────────────────────────────────────────┐       │
│  │              CanvasRegistry  (ID → Canvas)                     │       │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌────────────┐    │       │
│  │  │ Canvas "hero"    │  │ Canvas "tiles"   │  │ Canvas ... │    │       │
│  │  │  ImageDocument   │  │  ImageDocument   │  │            │    │       │
│  │  │  Layer stack     │  │  Layer stack     │  │            │    │       │
│  │  │  Resources map   │  │  Resources map   │  │            │    │       │
│  │  │  Op history      │  │  Op history      │  │            │    │       │
│  │  │  GPU Texture     │  │  GPU Texture     │  │            │    │       │
│  │  │  Undo/Redo stack │  │  Undo/Redo stack │  │            │    │       │
│  │  │  Zoom/Pan state  │  │  Zoom/Pan state  │  │            │    │       │
│  │  └─────────────────┘  └─────────────────┘  └────────────┘    │       │
│  └────────────────────────────────────────────────────────────────┘       │
│                                                                           │
│  ┌──────────────┐   ┌────────────────────┐   ┌──────────────────┐        │
│  │ Tool Palette  │   │ EditorPanels       │   │ Storage/Settings │        │
│  │ (brush, etc.) │   │ (ImGui windows)    │   │ (StorageManager) │        │
│  └──────────────┘   └────────────────────┘   └──────────────────┘        │
└───────────────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
User action (mouse click / keyboard / REPL input)
    │
    ▼
ToolPalette or EditorPanels → generates command string
    │
    ▼
CommandSystem.execute(commandLine)
    ├── resolve @canvas prefix (or use active canvas)
    ├── CommandRegistry.find(name) → CommandBase*
    ├── CommandArgParser.parse(args, metadata.params, ctx)
    │       → validates types, fills CommandArgs map
    │       → returns error if validation fails (before handler runs)
    ├── cmd->execute(canvasId, parsedArgs, editorContext)
    │       → handler operates on pre-validated, typed data
    │       → returns CommandResult
    └── log entry recorded with timestamp, result, canvasId
```

---

## 3. File Organization

```
tools/resource_editor/
    CMakeLists.txt
    main.cpp                            # Entry point, mode selection
    ResourceEditorScene.h/.cpp          # Scene: owns subsystems, wires everything
    ImageDocument.h/.cpp                # Per-image pixel data + drawing primitives + undo
    CanvasRegistry.h/.cpp               # Multi-document container (ID → Canvas)
    ToolPalette.h/.cpp                  # Tool state + mouse-to-command translation
    EditorPanels.h/.cpp                 # All ImGui panel drawing code
    FileOperations.h/.cpp               # File I/O (load/save images, native dialogs)

    commands/                           # Command system (one command = one file)
        CommandBase.h                   # CommandBase, GlobalCommand, CanvasCommand
        CommandArgs.h/.cpp              # CommandArgs, ParsedArg, typed accessors
        CommandArgParser.h/.cpp         # Metadata-driven argument parser
        CommandMetadata.h               # CommandMetadata, ParamDescriptor, ParamType
        CommandRegistry.h/.cpp          # CommandRegistry singleton + REGISTER_COMMAND
        CommandSystem.h/.cpp            # Thin dispatch + log + script I/O
        EditorContext.h                 # Shared state façade

        global/                         # Global commands (not canvas-scoped)
            HelpCommand.h
            ListCommand.h
            CreateCommand.h             # Dispatches create canvas/color/image
            LoadCommand.h/.cpp
            SelectCommand.h
            SetColorCommand.h
            SetToolCommand.h
            SetSizeCommand.h
            LogCommand.h
            RunCommand.h
            ExitCommand.h
            RehostCommand.h/.cpp
            CopyhostCommand.h/.cpp

        canvas/                         # Canvas-scoped commands
            SetPixelCommand.h
            FillCommand.h
            FloodFillCommand.h
            DrawLineCommand.h
            DrawRectCommand.h
            DrawCircleCommand.h
            DrawEllipseCommand.h
            DrawArcCommand.h
            DrawBezierCommand.h
            DrawImageCommand.h/.cpp
            PickCommand.h
            UndoCommand.h
            RedoCommand.h
            SaveCommand.h
            SaveAsCommand.h
            ExportCommand.h
            CloseCommand.h
            ZoomCommand.h
            PanCommand.h
            FlipCommand.h
            ResizeCommand.h
            CropCommand.h
            LayerAddCommand.h
            LayerRemoveCommand.h
            LayerSelectCommand.h
            LayerOpacityCommand.h
            LayerVisibilityCommand.h
            CopyAreaCommand.h
            GradientFillCommand.h

    dsl/                                # Canvas DSL subsystem
        CanvasDSLTypes.h                # AST node types, symbol table, scope stack
        CanvasDSLParser.h/.cpp          # Tokenizer and AST builder (Pass 1)
        CanvasDSLExprEval.h/.cpp        # Expression evaluator with bound context
        CanvasDSLExecutor.h/.cpp        # AST walker that emits commands (Pass 2)
```

---

## 4. Component Details

### 4.1 ImageDocument

Pure pixel data model with drawing primitives and undo/redo. Has no knowledge of canvas IDs, the command system, or GPU textures.

**Responsibilities:**
- Own a CPU-side RGBA pixel buffer (`std::vector<uint8_t>`)
- Track dimensions, file path, dirty flag, generation counter
- Provide drawing primitives (point, line, rect, circle, ellipse, arc, bezier, flood fill)
- Maintain undo/redo via full-buffer snapshots (Phase 1) with a 50-level cap
- Save/load via stb_image / stb_image_write

```cpp
class ImageDocument {
public:
    static std::unique_ptr<ImageDocument> createNew(uint32_t w, uint32_t h);
    static std::unique_ptr<ImageDocument> loadFromFile(const std::string& path);

    // Pixel access
    void setPixel(uint32_t x, uint32_t y, uint32_t rgba);
    uint32_t getPixel(uint32_t x, uint32_t y) const;
    const uint8_t* getPixelData() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    // Drawing primitives
    void drawBrush(int cx, int cy, int radius, uint32_t color);
    void drawLine(int x1, int y1, int x2, int y2, uint32_t color, int thickness);
    void drawRect(int x, int y, int w, int h, uint32_t color, bool filled);
    void drawCircle(int cx, int cy, int r, uint32_t color, bool filled);
    void drawEllipse(int cx, int cy, int rx, int ry, uint32_t color, bool filled);
    void drawArc(int cx, int cy, int r, float startAngle, float endAngle, uint32_t color, int width);
    void drawBezier(const std::vector<std::pair<int,int>>& points, uint32_t color, int width);
    void floodFill(int x, int y, uint32_t color);
    void fill(uint32_t color);
    void flipHorizontal();
    void flipVertical();
    void resize(uint32_t newW, uint32_t newH);
    void crop(int x, int y, uint32_t w, uint32_t h);

    // Undo/Redo
    void snapshotForUndo();
    bool undo();
    bool redo();

    // Persistence
    bool saveToFile(const std::string& path);
    bool exportToFile(const std::string& path);  // Detects format by extension

    // Sync
    uint64_t getGeneration() const;
    bool isDirty() const;
    void clearDirty();

private:
    std::vector<uint8_t> m_pixels;
    uint32_t m_width, m_height;
    std::string m_filePath;
    bool m_dirty = false;
    uint64_t m_generation = 0;
    std::vector<std::vector<uint8_t>> m_undoStack;
    std::vector<std::vector<uint8_t>> m_redoStack;
    static constexpr size_t kMaxUndoLevels = 50;
};
```

### 4.2 Canvas & CanvasRegistry

Each open resource is wrapped in a **Canvas** that bundles the `ImageDocument` with display state, layer stack, named resources, and operation history.

```cpp
struct Canvas {
    uint32_t id;                                    // Unique numeric ID (auto-assigned)
    std::string name;                                // Human-readable name
    std::unique_ptr<ImageDocument> document;          // Pixel data + undo
    std::map<std::string, std::unique_ptr<ImageDocument>> resources;  // Named image resources
    std::vector<std::string> operationHistory;        // For deterministic recreation / DSL export
    std::shared_ptr<vde::Texture> gpuTexture;         // VDE texture for rendering
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE;  // ImGui display handle
    uint64_t lastUploadedGeneration = 0;
    float zoomLevel = 1.0f;
    float panX = 0.0f, panY = 0.0f;
};

class CanvasRegistry {
public:
    Canvas* create(const std::string& name, std::unique_ptr<ImageDocument> doc);
    Canvas* getById(uint32_t id);
    Canvas* getByName(const std::string& name);
    Canvas* resolve(const std::string& nameOrId);
    bool remove(uint32_t id);
    bool has(uint32_t id) const;
    bool hasName(const std::string& name) const;
    std::vector<uint32_t> getIds() const;
    size_t count() const;
    std::string generateUniqueName(const std::string& base = "untitled");

    // Cross-canvas resource resolution: "canvasname::imagename" or bare "imagename"
    struct ResourceRef {
        Canvas* canvas = nullptr;
        ImageDocument* image = nullptr;
    };
    ResourceRef resolveResource(const std::string& ref, uint32_t activeCanvasId);

private:
    std::map<uint32_t, std::unique_ptr<Canvas>> m_canvases;
    std::map<std::string, uint32_t> m_nameIndex;
    uint32_t m_nextId = 1;
    int m_nextUntitledIndex = 1;
};
```

### 4.3 ToolPalette

Translates mouse interactions into command strings. Returns bare commands without `@` prefix — the caller decides targeting.

```cpp
enum class EditorTool { Brush, Eraser, ColorPicker, Fill, Line, Rect, Circle };

struct ToolState {
    EditorTool activeTool = EditorTool::Brush;
    uint32_t color = 0x000000FF;
    int brushSize = 1;
    bool shapeFilled = true;
};

class ToolPalette {
public:
    std::string onCanvasMouseDown(uint32_t canvasId, int x, int y);
    std::string onCanvasMouseDrag(uint32_t canvasId, int x, int y);
    std::string onCanvasMouseUp(uint32_t canvasId, int x, int y);

    ToolState& getState();
    void setTool(EditorTool tool);
    void setColor(uint32_t rgba);
    void setBrushSize(int size);
    bool isDrawingShape() const;

private:
    ToolState m_state;
    bool m_drawing = false;
    int m_startX = 0, m_startY = 0;
};
```

### 4.4 EditorPanels

All ImGui drawing code in a single module. Keeps ImGui concerns out of the scene class.

| Panel | Purpose |
|-------|---------|
| **Canvas Viewport(s)** | One per open canvas. Renders GPU texture with zoom/pan, mouse interaction, pixel grid overlay at high zoom. |
| **Canvas Tabs** | Tab bar showing open canvases. Clicking a tab runs `set_active`. |
| **Tool Palette** | Tool buttons, brush size slider, color swatch |
| **Color Picker** | RGBA color editor with hex input |
| **Properties Panel** | Canvas ID, name, dimensions, file path, dirty flag, zoom |
| **Command Console** | REPL input + scrollable command log |
| **Menu Bar** | File (New/Open/Save/Export/Exit), Edit (Undo/Redo), View (Zoom) |

### 4.5 FileOperations

Extracted file I/O helpers:
- Native file dialogs (Windows COM, adapted from `geometry_repl`)
- Image load/save wrappers (stb_image / stb_image_write)
- Script file read/write

### 4.6 ResourceEditorScene

The glue layer. Owns all subsystems and wires them together.

**Responsibilities:**
- Own `CommandSystem`, `CanvasRegistry`, `ToolPalette`, `EditorPanels`
- On enter: initialize subsystems, load settings from `StorageManager`
- On exit: save settings
- Each frame: sync GPU textures for dirty canvases, call `EditorPanels::draw*()`
- `executeCommand()` delegates to `CommandSystem::execute()`
- `drawDebugUI()` calls all panel draw functions
- Uses `deferCommand()` for entity mutations from render phase

**Settings stored via StorageManager:**

| Key | Type | Description |
|-----|------|-------------|
| `reseditor.recent_files` | string | JSON array of recent file paths |
| `reseditor.last_tool` | string | Last active tool name |
| `reseditor.last_color` | string | Last brush color as hex |
| `reseditor.brush_size` | string | Last brush size |
| `reseditor.active_canvas` | string | Last active canvas name |

---

## 5. Canvas Targeting

### `@canvas` Prefix

Every command line is parsed for an optional `@canvasId` prefix:

```
@hero paint (10, 5) #FF0000FF      ← targets canvas "hero" explicitly
paint (10, 5) #FF0000FF            ← targets the active canvas
```

- The `@` prefix is only meaningful as the first token.
- Global commands ignore the prefix (and produce a warning).
- Canvas-targeted commands require a valid resolved canvas. If no canvas is active and no `@` is given, they return an error.
- When logged, the full text including `@canvasId` is recorded for replay fidelity.

### Cross-Canvas Resource Access (`::`)

Image resources in one canvas can be referenced from another using `canvasname::imagename`:

```
draw hero::face_img [0] (0, 0) (32, 32)
```

Within the owning canvas, bare `imagename` suffices.

**Resolution order** for bare names:
1. Active canvas resources
2. Global objects (named colors, etc.)

---

## 6. Command Table

### Global Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `help` | `help [command]` | List commands or show help for one |
| `create canvas` | `create canvas <name> (w, h)` | Create a new blank canvas |
| `create color` | `create color <name> <hex>` | Define a named color |
| `create image` | `create image <name> <canvas>[layers] <area>` | Composite layers into a new image resource |
| `load` | `load <canvas> "<filepath>" [name]` | Load an image file as a canvas resource |
| `list` | `list` | List all open canvases |
| `select` | `select <canvas\|palette\|layer> <name>` | Set active canvas/palette/layer |
| `setcolor` | `setcolor <color>` | Set active brush color |
| `settool` | `settool <name>` | Switch active tool |
| `setsize` | `setsize <n>` | Set brush size |
| `log save` | `log save <path> [start] [end]` | Export command log |
| `log clear` | `log clear` | Clear command log |
| `run` | `run <filepath>` | Execute a command script |
| `dsl_load` | `dsl_load <filepath.vdecanvas>` | Parse and execute a DSL script |
| `dsl` | `dsl <statement>` | Execute a single DSL statement |
| `rehost` | `rehost <type> <name> [from <src>] to <dst>` | Transfer object between canvases |
| `copyhost` | `copyhost <type> <name> [from <src>] to <dst> [as <new>]` | Duplicate object to another canvas |
| `exit` | `exit` | Exit the editor |

### Canvas Commands (operate on active or `@target`)

| Command | Syntax | Description |
|---------|--------|-------------|
| `set` | `set (x, y) <color>` | Set a single pixel |
| `fill` | `fill [area] <color>` | Fill canvas/area with color |
| `floodfill` | `floodfill (x, y) with <color>` | Flood fill from point |
| `draw line` | `draw line (x1,y1) to (x2,y2) with <color> [width <n>]` | Draw a line |
| `draw rect` | `draw rect (x1,y1) to (x2,y2) with <color> [filled\|outline]` | Draw a rectangle |
| `draw circle` | `draw circle (cx,cy) radius <r> with <color> [filled\|outline]` | Draw a circle |
| `draw ellipse` | `draw ellipse (cx,cy) radius (rx,ry) with <color> [filled\|outline]` | Draw an ellipse |
| `draw arc` | `draw arc (cx,cy) radius <r> from <a1> to <a2> with <color>` | Draw an arc |
| `draw bezier` | `draw bezier <points...> with <color> [width <n>]` | Draw a bezier curve |
| `draw <image>` | `draw <img> [canvas][layer] (x,y) (w,h)` | Blit an image resource |
| `pick` | `pick (x, y)` | Pick color from pixel |
| `undo` | `undo` | Undo last operation |
| `redo` | `redo` | Redo last undone operation |
| `save` | `save [filepath]` | Save canvas |
| `saveas` | `saveas <filepath>` | Save to new path |
| `export` | `export <filepath> [format]` | Export canvas (png/bmp/tga) |
| `close` | `close` | Close the canvas |
| `resize` | `resize (w, h)` | Resize canvas |
| `crop` | `crop (x1,y1) to (x2,y2)` | Crop canvas |
| `zoom` | `zoom <level>` | Set zoom level |
| `pan` | `pan (dx, dy)` | Pan the view |
| `flip` | `flip horizontal\|vertical` | Flip image |
| `layer add` | `layer add <name> [above\|below <ref> \| at <index>]` | Add layer |
| `layer remove` | `layer remove <name>` | Remove layer |
| `layer select` | `layer select <name\|index>` | Switch active layer |
| `layer opacity` | `layer opacity <name> <0-100>` | Set layer opacity |
| `layer visibility` | `layer visibility <name> show\|hide` | Toggle layer visibility |
| `gradient_fill` | `gradient_fill <direction> <stops...>` | Fill with gradient |
| `copy_area` | `copy_area (x1,y1) (x2,y2) (dx,dy) [fliph] [flipv]` | Copy rectangular region |
| `dsl_export` | `dsl_export <filepath.vdecanvas>` | Export canvas as DSL script |

---

## 7. Workflow Examples

### Example 1: Single-Canvas Interactive Session

```
> create canvas sprite_hero (32, 32)
  Created canvas 'sprite_hero' (32x32). Active canvas set to 'sprite_hero'.

> setcolor #FF0000FF
> settool brush
> set (10, 5) #FF0000FF
  [sprite_hero] Set pixel at (10,5)

> save assets/hero_sprite.png
  [sprite_hero] Saved to assets/hero_sprite.png
```

### Example 2: Multi-Canvas Session

```
> create canvas hero (32, 32)
> create canvas bullet (16, 16)
  Active canvas set to 'bullet'.

> set (8, 8) #FFFF00FF
  [bullet] Set pixel at (8,8)

> select canvas hero
> set (16, 16) #FF0000FF
  [hero] Set pixel at (16,16)

> @bullet fill #0000FFFF
  [bullet] Filled with #0000FFFF

> list
  id  name    dimensions  state
   1  hero    32x32       active, modified
   2  bullet  16x16       modified
```

### Example 3: Loading and Compositing Images

```
> load hero "assets/face.png"
  Loaded 'face' into new canvas 'hero' (32x32).

> load hero "assets/badge.png" badge
  Loaded 'badge' as resource in canvas 'hero'.

> draw badge [0] (2, 2) (8, 8)
  [hero] Drew 'badge' at (2,2) size 8x8

> create canvas sheet (128, 128)
> draw hero::face [0] (0, 0) (32, 32)
  [sheet] Drew 'hero::face' at (0,0) size 32x32
```

### Example 4: DSL Workflow

```
> dsl_load assets/hero_sprite.vdecanvas
  Parsed hero_sprite.vdecanvas (0 errors).
  Executing... 47 commands emitted.
  Created canvas 'hero' (32x32).

> set (15, 15) #FF0000FF
  [hero] Set pixel at (15,15)

> dsl_export assets/hero_sprite_v2.vdecanvas
  [hero] Exported DSL script (52 statements)
```

### Example 5: Batch Execution (CI/CD)

```bash
vde_resource_editor.exe --dsl assets/hero_sprite.vdecanvas
# Script's 'export' directive writes output. Exits with code 0 on success.

vde_resource_editor.exe scripts/generate_assets.txt
# Executes command script line-by-line.
```

---

## 8. Entry Point & Modes

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
        if (!tool.initialize(settings)) return 1;
        tool.run();
        return tool.getExitCode();
    }
}
```

Detects `.vdecanvas` extension vs `.txt` and routes accordingly: DSL scripts go through the parser/executor, plain scripts go through line-by-line `execute()`.

---

## 9. GPU Texture Management

Each frame, the scene iterates all canvases and checks if `document->getGeneration() > canvas.lastUploadedGeneration`. If so:

1. Create/recreate a VDE `Texture` from the pixel data via `loadFromData()` + `uploadToGPU()`.
2. Create/recreate an ImGui descriptor set via `ImGui_ImplVulkan_AddTexture()`.
3. Update `canvas.lastUploadedGeneration`.

On `onBeforeImGuiShutdown()`, all ImGui texture descriptor sets are cleaned up.

For Phase 1, full texture re-uploads are acceptable for pixel art (≤256×256). Larger canvases may benefit from staging buffer reuse in a future phase.

---

## 10. Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| VDE engine (`vde` library) | Root CMakeLists.txt | Core rendering, window, input |
| Dear ImGui (docking) | FetchContent | All editor UI |
| stb_image | third_party/stb/ | Image loading |
| stb_image_write | third_party/stb/ | Image saving (PNG, BMP, TGA) |
| SQLite (StorageManager) | Already in VDE | Persistent settings |
| Windows COM (ole32, uuid) | System | Native file dialogs |

No new external dependencies required.

---

## 11. Conventions

- **Naming:** PascalCase classes, camelCase methods, `m_` prefix for members
- **Headers:** `#pragma once`, `vde::tools::` namespace
- **Include order:** Corresponding header → VDE headers → Third-party → Standard library
- **Color format:** `#RRGGBBAA` hex strings in commands; `uint32_t` packed RGBA internally
- **Error handling:** All commands validate arguments and return clear error messages. Never crash on bad input.
- **File extensions:** `.vdecanvas` for canvas scripts, `.vdepalette` for shared palettes
- **Texture display:** `VkDescriptorSet` from `ImGui_ImplVulkan_AddTexture()` stored in `Canvas::imguiTextureId`
- **STB usage:** Implementation defined in `src/stb_impl.cpp` (already in `vde` lib). Resource Editor only includes headers.
