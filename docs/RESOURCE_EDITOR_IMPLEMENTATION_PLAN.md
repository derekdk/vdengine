# VDE Resource Editor — Implementation Plan

Reference: [RESOURCE_EDITOR_DESIGN.md](RESOURCE_EDITOR_DESIGN.md)

---

## Design Review Summary

The design is thorough and well-aligned with existing VDE tool patterns (`geometry_repl`, `vlauncher`). The architecture — command-first with canvas resolution, multi-document registry, phased DSL integration — is sound. Below are observations and a refined plan.

### Strengths

- **Command-first architecture** cleanly decouples UI from mutation logic, enabling both interactive and batch workflows from day one.
- **Canvas-Per-Resource with `@canvasId` targeting** is a good multi-document model, and the resolution rules are clearly specified.
- **Phased delivery** is realistic — Phase 1 delivers genuine value before DSL complexity arrives.
- **ToolBase.h integration** matches existing patterns exactly. No framework changes needed.
- **No new external dependencies** — all dependencies (stb, ImGui, SQLite) are already in-tree.

### Design Issues & Adjustments

1. **Duplicate `resolveCanvas` signatures.** The design shows two `resolveCanvas` methods with conflicting return types (`pair<uint32_t, string>` and `pair<string, string>`). Implementation should use a single return type: `struct ResolvedCommand { uint32_t canvasId; std::string canvasName; std::string commandName; std::string args; }`.

2. **`CommandSystem` should not own `resolveCanvasId` for name→ID mapping.** It delegates to `CanvasRegistry::resolve()` via a non-owning pointer. The design already has this concept (`m_registry = nullptr`) but should be explicit: `CommandSystem::setRegistry(CanvasRegistry*)` in scene init.

3. **Layer system is premature for Phase 1.** Layers add compositing complexity (alpha blending, draw order, per-layer pixel buffers). The design lists `layer add`, `layer remove`, `layer select`, `layer opacity`, `layer visibility` as Phase 1 scene integration items. **Recommendation:** move layer commands to Phase 2 and keep Phase 1 focused on single-layer painting. Register stub commands that return "not yet implemented" so scripts referencing layers don't crash.

4. **`gradient_fill` and `copy_area` are advanced for Phase 1.** These require non-trivial algorithms (gradient interpolation, region copy with flip). **Recommendation:** defer to Phase 2. Register stubs in Phase 1.

5. **`drawCircle` and `floodFill` are listed as Phase 2 in the doc but `circle` and `floodfill` commands are registered in Phase 1.** Resolve by implementing basic circle (midpoint algorithm) and flood fill (queue-based) in Phase 1's `ImageDocument`, since the command infrastructure needs working handlers — no-op stubs would make the editor feel broken.

6. **`stb_image_write` implementation** is already compiled into the `vde` library (`src/stb_impl.cpp`). The Resource Editor must NOT define `STB_IMAGE_WRITE_IMPLEMENTATION` again — just include the header and link `vde`.

7. **File dialog pattern** should be copied/adapted from `geometry_repl/FileDialog.h/.cpp` rather than reimplemented, or extracted to a shared utility. For Phase 1, copy is simplest.

8. **Texture re-upload efficiency.** The design says `re-upload when generation exceeds lastUploadedGeneration`. This means a full GPU texture upload per frame when painting. For small pixel-art canvases (≤256×256) this is fine. Add a TODO to investigate staging buffer reuse for larger canvases in Phase 2.

9. **`onCanvasMouseDown/Drag/Up` takes `const std::string& canvasId`** but the canvas system uses `uint32_t id`. The palette methods should take `uint32_t canvasId` to avoid unnecessary string conversions. The design shows string canvasId in the ToolPalette interface which is inconsistent with the uint32_t used everywhere else.

10. **DSL namespace.** The design uses `vde::tools::dsl`. Since the DSL files are under `tools/resource_editor/dsl/`, the namespace should be `vde::tools::resource_editor::dsl` or simply `vde::tools::dsl` if the DSL may eventually be reused. Use `vde::tools::dsl` as designed since it's already scoped to the tool.

---

## Implementation Plan

### Dependency Chain

```
Step 1: Scaffolding         (no dependencies)
Step 2: ImageDocument       (no dependencies, pure data model)
Step 3: CanvasRegistry      (depends on Step 2)
Step 4: CommandSystem        (depends on Step 3 for name→id resolution)
Step 5: FileOperations       (depends on Step 2 for ImageDocument)
Step 6: ToolPalette          (depends on Step 4 for command format)
Step 7: EditorPanels         (depends on Steps 3-6)
Step 8: Scene Integration    (depends on Steps 2-7, wires everything)
Step 9: Storage Integration  (depends on Step 8)
Step 10: Smoke Tests         (depends on Step 8)
```

Steps 2 and 3 can proceed in parallel. Steps 4, 5, and 6 can be built in any order once 2+3 are done. Step 7 depends on all preceding subsystems. Step 8 is the final wiring step.

---

### Step 1: Project Scaffolding

**Goal:** Empty tool that builds, launches, shows an ImGui window, and exits cleanly.

**Files to create:**
- `tools/resource_editor/CMakeLists.txt`
- `tools/resource_editor/main.cpp`
- `tools/resource_editor/ResourceEditorScene.h`
- `tools/resource_editor/ResourceEditorScene.cpp`

**Files to modify:**
- `tools/CMakeLists.txt` — add `add_subdirectory(resource_editor)`

**Details:**

`CMakeLists.txt` — Start with only the files that exist now. DSL files will be added in Phase 3.

```cmake
add_executable(vde_resource_editor
    main.cpp
    ResourceEditorScene.cpp
    ResourceEditorScene.h
)

target_link_libraries(vde_resource_editor PRIVATE vde imgui_backend)
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

`ResourceEditorScene` — Minimal stub inheriting `BaseToolScene`:
- `executeCommand()` — log the command text, no-op
- `getToolName()` → `"Resource Editor"`
- `drawDebugUI()` — simple ImGui text: "VDE Resource Editor — Phase 1 WIP"

`main.cpp` — Standard entry point following the `geometry_repl` pattern:
- Parse `argv[1]` as script file → `ToolMode::SCRIPT`
- `ResourceEditorGame` extends `BaseToolGame<BaseToolInputHandler, ResourceEditorScene>`
- Interactive: `runTool()` with 1400×900 DPI-scaled window
- Script: headless init + `run()` + `getExitCode()`

**Verification:** Build with `scripts: build` task. Launch `build_ninja/bin/vde_resource_editor.exe`. Confirm window appears, ESC exits cleanly.

---

### Step 2: ImageDocument

**Goal:** Pure pixel data model with drawing primitives and undo/redo, fully unit-tested.

**Files to create:**
- `tools/resource_editor/ImageDocument.h`
- `tools/resource_editor/ImageDocument.cpp`
- `tests/ImageDocument_test.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add `ImageDocument.cpp`, `ImageDocument.h`
- `tests/CMakeLists.txt` — add `ImageDocument_test.cpp`

**Implementation order:**

1. **Core data:** `m_pixels` (RGBA `vector<uint8_t>`), `m_width`, `m_height`, `m_generation`, `m_dirty`, `m_filePath`
2. **Construction:** `createNew(w, h)` → transparent black buffer; `loadFromFile(path)` → `stbi_load`
3. **Pixel access:** `setPixel(x, y, rgba)`, `getPixel(x, y)`, `getPixelData()` — bounds-checked, increment generation on write
4. **Fill:** `fill(color)` — memset-like loop, increment generation
5. **drawBrush(cx, cy, radius, color):** Filled circle stamped into pixel buffer. For radius=0 use setPixel.
6. **drawLine(x1, y1, x2, y2, color, thickness):** Bresenham's line algorithm. For thickness>1, stamp brush at each point.
7. **drawRect(x, y, w, h, color, filled):** Filled or outline rectangle. Clip to canvas bounds.
8. **drawCircle(cx, cy, r, color, filled):** Midpoint circle algorithm. Filled variant fills scanlines between boundary points.
9. **floodFill(x, y, color):** Queue-based flood fill. Replace target color with fill color.
10. **flipHorizontal(), flipVertical():** In-place pixel swaps.
11. **resize(newW, newH):** Nearest-neighbor resample (pixel art — no interpolation).
12. **crop(x, y, w, h):** Extract sub-rectangle into a new buffer, replace `m_pixels`.
13. **Undo/redo:** `snapshotForUndo()` pushes full `m_pixels` copy to `m_undoStack` (capped at `kMaxUndoLevels=50`), clears redo stack. `undo()` pushes current to redo, pops undo. `redo()` reverse. Increment generation on restore.
14. **Persistence:** `saveToFile(path)` via `stbi_write_png`. `exportToFile(path)` — detect extension, dispatch to `stbi_write_png`/`stbi_write_bmp`/`stbi_write_tga`.
15. **Accessors:** `isDirty()`, `clearDirty()`, `getGeneration()`, `getFilePath()`, `setFilePath()`

**Unit tests (ImageDocument_test.cpp):**
- Create new document, verify dimensions and transparent pixels
- Set/get pixel round-trip
- Fill, verify all pixels
- drawLine horizontal/vertical/diagonal, verify affected pixels
- drawRect filled and outline
- drawCircle filled and outline
- floodFill contiguous region
- Undo/redo cycle: set pixel → snapshot → set pixel → undo → verify original → redo → verify modified
- Undo stack cap at 50
- flipHorizontal, flipVertical correctness
- resize preserves pixel data (nearest-neighbor)
- crop extracts correct sub-rectangle
- Generation counter increments
- Dirty flag behavior
- saveToFile/loadFromFile round-trip (write temp file, reload, compare pixels)

**Note:** since `ImageDocument` lives under `tools/resource_editor/`, the test file needs an include path to that directory. Add `target_include_directories(vde_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools/resource_editor)` to `tests/CMakeLists.txt`, or put ImageDocument headers in a path the test can reach. Alternatively, the test can include via relative path. Follow whichever pattern is established for tool unit tests — if none exists, add the include directory.

---

### Step 3: CanvasRegistry

**Goal:** Multi-document container with ID/name lookup, fully unit-tested.

**Files to create:**
- `tools/resource_editor/CanvasRegistry.h`
- `tools/resource_editor/CanvasRegistry.cpp`
- `tests/CanvasRegistry_test.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add new files
- `tests/CMakeLists.txt` — add test file

**Implementation:**

`Canvas` struct:
```cpp
struct Canvas {
    uint32_t id;
    std::string name;
    std::unique_ptr<ImageDocument> document;
    std::shared_ptr<vde::Texture> gpuTexture;      // nullptr until scene creates
    VkDescriptorSet imguiTextureId = VK_NULL_HANDLE; // nullptr until scene creates
    uint64_t lastUploadedGeneration = 0;
    float zoomLevel = 1.0f;
    float panX = 0.0f, panY = 0.0f;
};
```

`CanvasRegistry`:
- `create(name, doc)` → assign `m_nextId++`, insert into `m_canvases` and `m_nameIndex`, return `Canvas*`
- `getById(id)`, `getByName(name)` → return `Canvas*` or `nullptr`
- `resolve(nameOrId)` → try numeric parse first (`uint32_t`), then name lookup
- `remove(id)` → erase from both maps
- `has(id)`, `hasName(name)` → boolean checks
- `getIds()` → ordered vector of IDs
- `count()` → size
- `generateUniqueName(base)` → `"untitled_1"`, `"untitled_2"`, etc.

**Unit tests:**
- Create canvas, verify ID assignment starts at 1 and increments
- getById, getByName return correct canvas
- resolve with numeric string ("3") and name ("hero")
- remove decrements count, ID not reused
- has/hasName correctness
- generateUniqueName increments suffix
- Duplicate name handling (should it fail or auto-suffix? Design doesn't specify — fail with error return)

---

### Step 4: CommandSystem

**Goal:** Central command dispatch with canvas resolution, log, and script I/O.

**Files to create:**
- `tools/resource_editor/CommandSystem.h`
- `tools/resource_editor/CommandSystem.cpp`
- `tests/CommandSystem_test.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add new files
- `tests/CMakeLists.txt` — add test file

**Implementation:**

1. **Types:** `CommandLogEntry`, `CanvasHandler`, `GlobalHandler` as designed.

2. **Resolved command struct** (replace the two conflicting `resolveCanvas` signatures):
   ```cpp
   struct ResolvedCommand {
       uint32_t canvasId = 0;     // 0 = no canvas (global or unresolved)
       std::string canvasName;    // For display in log
       std::string commandName;   // Parsed command name
       std::string argsString;    // Everything after the command name
       bool hasExplicitCanvas = false;
   };
   ```

3. **`resolveCanvas(commandLine)`** → parse optional `@name`/`@id` prefix as the first token. If present, resolve through `m_registry->resolve()`. If not present, use `m_activeCanvasId`. Return `ResolvedCommand`.

4. **`execute(commandLine)`:**
   - Trim whitespace, skip empty/comment lines
   - Call `resolveCanvas(commandLine)`
   - Look up `commandName` in `m_globalHandlers` first, then `m_canvasHandlers`
   - If global: call handler with args stream. If `hasExplicitCanvas`, log a warning.
   - If canvas-targeted: verify `canvasId != 0`, call handler with `(canvasId, argsStream)`.
   - Create `CommandLogEntry` with timestamp (`HH:MM:SS.mmm`), log to `m_log`
   - Return `success`

5. **`registerCommand(name, help, handler)`** and **`registerGlobalCommand(name, help, handler)`** — store in respective maps + `m_helpText`.

6. **`executeScript(filePath)`** — read lines, skip `#` comments and blank lines, call `execute()` per line. Return false on first failure.

7. **`saveFullLog(filePath)`** and **`saveLogRange(startIdx, endIdx, filePath)`:**
   - For each log entry, emit the `commandLine` as-is (it already contains `@canvasId` if explicit)
   - For commands that targeted the active canvas (no explicit `@`), emit with `@canvasName` prefix to make replay deterministic. This is the "normalization" the design mentions.

8. **`getLog()`, `getCommandNames()`, `getHelpText(command)`, `clear()`** — straightforward accessors.

9. **`setActiveCanvasId()`, `getActiveCanvasId()`, `setRegistry()`** — simple setters/getters.

**Unit tests:**
- Register global command, execute, verify log entry
- Register canvas command, set active canvas ID, execute, verify handler receives correct ID
- `@name` prefix resolution: register canvas, execute `@hero paint ...`, verify handler receives hero's ID
- `@id` prefix resolution: execute `@1 paint ...`, verify handler receives ID 1
- Global command with `@` prefix: verify warning in result
- Canvas command with no active canvas and no `@`: verify error
- `executeScript` with multi-line script
- `saveFullLog` + `executeScript` round-trip
- `getCommandNames()` returns registered names
- `getHelpText(name)` returns help string
- `clear()` empties log

**Note:** Unit tests need a mock `CanvasRegistry` (or a real one with test canvases) to verify resolution. Create a small test fixture that populates a `CanvasRegistry` with 2-3 canvases.

---

### Step 5: FileOperations

**Goal:** Image file I/O and native dialogs, extracted for reuse.

**Files to create:**
- `tools/resource_editor/FileOperations.h`
- `tools/resource_editor/FileOperations.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add new files

**Implementation:**

Copy and adapt the file dialog pattern from `geometry_repl/FileDialog.h/.cpp`:
- `openFileDialog(title, filters)` — Windows COM `IFileOpenDialog`
- `saveFileDialog(title, filters, defaultExt)` — Windows COM `IFileSaveDialog`

Image I/O wrappers:
- `loadImageFile(path, outPixels, outW, outH)` → `stbi_load`, 4-channel force, return bool
- `saveImagePNG(path, pixels, w, h)` → `stbi_write_png` (stride = `w * 4`)
- `saveImageBMP(path, pixels, w, h)` → `stbi_write_bmp`
- `saveImageTGA(path, pixels, w, h)` → `stbi_write_tga`

Script file helpers:
- `readScriptLines(path)` → return `vector<string>`, skip BOM if present
- `writeScriptLines(path, lines)` → write lines to file with newlines

**Testing:** File operations are exercised via `ImageDocument_test.cpp` (save/load round-trip). File dialogs are manual-test only (they open native OS dialogs).

---

### Step 6: ToolPalette

**Goal:** Mouse-to-command translation layer.

**Files to create:**
- `tools/resource_editor/ToolPalette.h`
- `tools/resource_editor/ToolPalette.cpp`
- `tests/ToolPalette_test.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add new files
- `tests/CMakeLists.txt` — add test file

**Design adjustment:** Change method signatures to use `uint32_t canvasId` instead of `const std::string& canvasId` for consistency with the `Canvas::id` type.

**Implementation:**

`EditorTool` enum and `ToolState` as designed.

Mouse handlers return command strings (no `@` prefix — caller decides targeting):

- **Brush:** `onCanvasMouseDown` → `"paint X Y #COLOR SIZE"`. `onCanvasMouseDrag` → same per drag point.
- **Eraser:** Same as Brush but with transparent color `#00000000`.
- **ColorPicker:** `onCanvasMouseDown` → `"pick X Y"`.
- **Fill:** `onCanvasMouseDown` → `"floodfill X Y #COLOR"`.
- **Line:** `onCanvasMouseDown` → record start, return `""`. `onCanvasMouseUp` → `"line X1 Y1 X2 Y2 #COLOR SIZE"`.
- **Rect:** Same pattern → `"rect X Y W H #COLOR filled"` or `"rect X Y W H #COLOR"`.
- **Circle:** `onCanvasMouseDown` → record center. `onCanvasMouseUp` → compute radius from distance → `"circle CX CY R #COLOR filled"`.

Helper: `colorToHex(uint32_t rgba)` → `"#RRGGBBAA"` string.

**Unit tests:**
- Brush: mouse-down generates `paint` command with correct coords and color
- Brush drag: each drag point generates `paint`
- Eraser: generates `paint` with `#00000000`
- ColorPicker: generates `pick`
- Fill: generates `floodfill`
- Line: mouse-down → empty, mouse-up → `line` with start-to-end coords
- Rect: mouse-up → `rect` with correct x/y/w/h
- Circle: mouse-up → `circle` with center and radius
- setTool/setColor/setBrushSize change subsequent command output

---

### Step 7: EditorPanels (ImGui)

**Goal:** All ImGui UI code in a single compilation unit, calling into the subsystems.

**Files to create:**
- `tools/resource_editor/EditorPanels.h`
- `tools/resource_editor/EditorPanels.cpp`

**Files to modify:**
- `tools/resource_editor/CMakeLists.txt` — add new files

**Implementation (in order of priority):**

1. **`drawCommandConsole(cmd)`** — Adapted from `geometry_repl`'s `ReplConsole`:
   - Top: scrollable output area showing `CommandLogEntry` items (index, timestamp, command, result)
   - Color-code: green for success, red for errors. Show `[canvasName]` prefix for canvas-scoped commands.
   - Bottom: `ImGui::InputText` with `EnterReturnsTrue` → `cmd.execute(input)`
   - Auto-scroll to bottom on new entries.

2. **`drawToolPalette(palette, cmd)`:**
   - Radio buttons for each `EditorTool`
   - On tool change → `cmd.execute("settool <name>")`
   - `ImGui::SliderInt` for brush size → `cmd.execute("setsize <n>")`
   - Color swatch showing current color (clicking opens color picker)

3. **`drawColorPicker(palette, cmd)`:**
   - `ImGui::ColorEdit4` with `ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar`
   - Hex input field: `#RRGGBBAA`
   - On change → `cmd.execute("setcolor #RRGGBBAA")`

4. **`drawCanvasTabs(canvases, cmd)`:**
   - `ImGui::BeginTabBar("CanvasTabs")`
   - One `TabItem` per canvas: `"[id] name (*if dirty)"`
   - Clicking a tab → `cmd.execute("set_active <name>")`

5. **`drawCanvasViewport(canvas, palette, cmd, isActive)`:**
   - `ImGui::Image(canvas.imguiTextureId, size * canvas.zoomLevel)`
   - Mouse event capture: convert ImGui mouse position to canvas pixel coordinates using zoom/pan
   - If mouse clicked and this canvas is not active → `cmd.execute("set_active <name>")`
   - Delegate to `palette.onCanvasMouseDown/Drag/Up(id, pixelX, pixelY)` → execute returned command
   - Mouse wheel → adjust zoom level

6. **`drawAllCanvasViewports(canvases, palette, cmd)`:**
   - Iterate `canvases.getIds()`, call `drawCanvasViewport` for each

7. **`drawPropertiesPanel(activeCanvas)`:**
   - Show: canvas ID, name, dimensions, file path, dirty flag, generation, undo/redo counts, zoom level

8. **`drawMenuBar(cmd, canvases)`:**
   - File: New (popup for size), Open, Save, Save As, Export, Exit
   - Edit: Undo, Redo
   - View: Zoom In, Zoom Out, Reset Zoom

**No unit tests for ImGui panels** — they are visual and tested via smoke tests.

---

### Step 8: Scene Integration

**Goal:** Wire all subsystems together in `ResourceEditorScene`. Register all commands. The editor is functional end-to-end.

**Files to modify:**
- `tools/resource_editor/ResourceEditorScene.h` — add member variables for all subsystems
- `tools/resource_editor/ResourceEditorScene.cpp` — full implementation

**Implementation:**

**Members:**
```cpp
CommandSystem m_commandSystem;
CanvasRegistry m_canvasRegistry;
ToolPalette m_toolPalette;
```

**`onEnter()` — Command registration:**

*Global commands:*
- `help [command]` — list all commands or show help for one
- `new <w> <h> [name]` — create canvas via `m_canvasRegistry.create()`, set active, create GPU texture
- `open <filepath> [name]` — load via `FileOperations::loadImageFile()`, create canvas
- `list` — iterate canvases, format table
- `set_active <name|id>` — resolve, call `m_commandSystem.setActiveCanvasId()`
- `setcolor <hex>` — parse hex, call `m_toolPalette.setColor()`
- `settool <name>` — parse name, call `m_toolPalette.setTool()`
- `setsize <n>` — parse int, call `m_toolPalette.setBrushSize()`
- `log save <filepath> [start] [end]` — delegate to `m_commandSystem.saveLogRange/Full()`
- `log clear` — delegate to `m_commandSystem.clear()`
- `run <filepath>` — delegate to `m_commandSystem.executeScript()`
- `exit` — call `quitTool()`

*Canvas-targeted commands — each handler receives `(uint32_t canvasId, istringstream& args)`:*
- `paint <x> <y> <color> [size]` — `snapshotForUndo()`, `drawBrush()`, increment generation
- `fill <color>` — `snapshotForUndo()`, `fill()`
- `floodfill <x> <y> <color>` — `snapshotForUndo()`, `floodFill()`
- `line <x1> <y1> <x2> <y2> <color> [size]` — `snapshotForUndo()`, `drawLine()`
- `rect <x> <y> <w> <h> <color> [filled]` — `snapshotForUndo()`, `drawRect()`
- `circle <cx> <cy> <r> <color> [filled]` — `snapshotForUndo()`, `drawCircle()`
- `pick <x> <y>` — `getPixel()`, call `m_toolPalette.setColor()`
- `undo` — `document->undo()`
- `redo` — `document->redo()`
- `save [filepath]` — `document->saveToFile()` or `FileOperations::saveImagePNG()`
- `saveas <filepath>` — always save to new path
- `export <filepath> [format]` — `document->exportToFile()`
- `close` — remove canvas from registry, clean up ImGui texture
- `zoom <level>` — set `canvas.zoomLevel`
- `pan <dx> <dy>` — adjust `canvas.panX/panY`
- `fliph` — `snapshotForUndo()`, `document->flipHorizontal()`
- `flipv` — `snapshotForUndo()`, `document->flipVertical()`
- `resize <w> <h>` — `snapshotForUndo()`, `document->resize()`
- `crop <x> <y> <w> <h>` — `snapshotForUndo()`, `document->crop()`

*Phase 1 stubs (return "not yet implemented"):*
- `layer add`, `layer remove`, `layer select`, `layer opacity`, `layer visibility`
- `gradient_fill`, `copy_area`, `dsl_load`, `dsl`, `dsl_export`

**`executeCommand(cmdLine)`** — delegate to `m_commandSystem.execute(cmdLine)`. Also call `addConsoleMessage()` for BaseToolScene compat.

**`drawDebugUI()`** — call all `EditorPanels::draw*()` functions.

**`update(dt)`:**
- Handle pending file dialogs (deferred from command handlers, processed here)
- Iterate canvases: if `doc->getGeneration() > canvas.lastUploadedGeneration`:
  - Upload pixel data to `canvas.gpuTexture` (create/recreate VDE `Texture`, call `loadFromData()` + `uploadToGPU()`)
  - Create/recreate ImGui descriptor: `ImGui_ImplVulkan_AddTexture(sampler, imageView, SHADER_READ_ONLY_OPTIMAL)`
  - Update `canvas.lastUploadedGeneration`

**`onBeforeImGuiShutdown()`** — iterate all canvases, call `ImGui_ImplVulkan_RemoveTexture()` for each `imguiTextureId`.

**`ResourceEditorGame`:**
```cpp
class ResourceEditorGame : public BaseToolGame<BaseToolInputHandler, ResourceEditorScene> {
public:
    ResourceEditorGame(ToolMode mode, const std::string& scriptFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile) {}

    void onStart() override {
        BaseToolGame::onStart();
        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                m_commandSystem.executeScript(m_scriptFile);  // or via scene
            }
            quit();
        }
    }
private:
    std::string m_scriptFile;
};
```

**Verification:** Build. Launch interactively. Execute `new 32 32 test` in console. Verify canvas appears. Paint pixels. Undo. Save. Reopen. Run a multi-line script file.

---

### Step 9: Storage Integration

**Goal:** Persist user preferences across sessions.

**Files to modify:**
- `tools/resource_editor/ResourceEditorScene.cpp`

**Implementation:**

In `onEnter()`:
```cpp
StorageManager::getInstance().init_storage("vde_resource_editor");
// Load saved preferences
auto lastTool = StorageManager::getInstance().getStringData("reseditor.last_tool");
if (lastTool) m_toolPalette.setTool(parseToolName(*lastTool));
// ... similarly for last_color, brush_size
```

In `onExit()` (or scene destructor):
```cpp
StorageManager::getInstance().setStringData("reseditor.last_tool", toolToString(m_toolPalette.getState().activeTool));
StorageManager::getInstance().setStringData("reseditor.last_color", colorToHex(m_toolPalette.getState().color));
// ... similarly for brush_size, window dimensions, active canvas name
StorageManager::getInstance().shutdown();
```

---

### Step 10: Smoke Tests & Documentation

**Goal:** Automated and manual test scripts, and tool README.

**Files to create:**
- `smoketests/scripts/smoke_resource_editor.vdescript` — input-script smoke test
- `tools/resource_editor/scripts/test_basic.txt` — single-canvas command script
- `tools/resource_editor/scripts/test_multi_canvas.txt` — multi-canvas command script
- `tools/resource_editor/README.md` — tool documentation

**Smoke test (`smoke_resource_editor.vdescript`):**
```vdescript
wait startup
wait 1s
press F1
wait 500
exit
```

**Basic command script (`test_basic.txt`):**
```
# Create a canvas, paint, and save
new 16 16 smoke_test
setcolor #FF0000FF
settool brush
paint 8 8 #FF0000FF 1
paint 9 8 #00FF00FF 1
paint 8 9 #0000FFFF 1
save smoke_test_output.png
exit
```

**Multi-canvas command script (`test_multi_canvas.txt`):**
```
# Test multi-canvas workflow
new 8 8 canvas_a
new 8 8 canvas_b
set_active canvas_a
paint 0 0 #FF0000FF 1
@canvas_b paint 0 0 #00FF00FF 1
list
@canvas_a save canvas_a_output.png
@canvas_b save canvas_b_output.png
exit
```

**README.md** — Overview, usage (interactive + script), full command reference, controls, examples.

---

## Phase 2: Usability & Polish

Implement after Phase 1 is stable. Each item is independent and can be done in any order.

| Item | Scope | Notes |
|------|-------|-------|
| Pixel grid overlay | `EditorPanels` | Draw grid lines via `ImGui::GetWindowDrawList()` when zoom ≥ 4x |
| Shape preview | `EditorPanels` | Ghost overlay (draw list lines/rects/circles) while `ToolPalette::isDrawingShape()` |
| Tab-completion | `EditorPanels` | Match command names from `CommandSystem::getCommandNames()` on Tab key |
| Keyboard shortcuts | `ResourceEditorScene` | Ctrl+Z/Y/S/N, B/E/G/I tool hotkeys, map to commands |
| DPI-aware layout | `EditorPanels` | Scale all `SetNextWindowSize` / `SetNextWindowPos` by `getDPIScale()` |
| Recent files menu | `EditorPanels` | Load from `StorageManager`, show in File menu |
| Dirty indicator | `EditorPanels` | Show `*` in canvas tab title when `isDirty()` |
| Confirm on close | `EditorPanels`, `ResourceEditorScene` | ImGui popup when closing dirty canvas or exiting |
| Layer system | `ImageDocument`, `CanvasRegistry`, `CommandSystem` | Multi-layer compositing with per-layer pixel buffers |
| Gradient fill | `ImageDocument` | Linear gradient computation |
| Copy area | `ImageDocument` | Region copy with optional flip |

---

## Phase 3: Canvas DSL

Implement after Phase 1 is stable and optionally after some Phase 2 items.

### Step 3.1: DSL Types & Parser (`dsl/CanvasDSLTypes.h`, `dsl/CanvasDSLParser.h/.cpp`)
- AST node types, symbol table, `ParseResult` struct
- Tokenizer: line-by-line, split on whitespace, handle quoted strings, strip comments
- Statement parsers for each `create`, `draw`, `fill`, `select`, control flow keyword
- Symbol table registration + duplicate detection
- Reference validation (forward reference → error with suggestion)
- Include resolution (relative paths, circular include guard)
- Unit tests: `tests/CanvasDSLParser_test.cpp`

### Step 3.2: Expression Evaluator (`dsl/CanvasDSLExprEval.h/.cpp`)
- Stack-based evaluator for integer arithmetic
- Bound references: `lb`, `rb`, `tb`, `bb`, `cx`, `cy`, `w`, `h`
- Percentage expressions: `50%w` → `w / 2`
- Dot-notation: `point.x`, `area.cx`
- Palette index: `pal[name]`, `pal[0]`
- Scope push/pop for `in <area>` blocks
- Unit tests: `tests/CanvasDSLExprEval_test.cpp`

### Step 3.3: DSL Executor (`dsl/CanvasDSLExecutor.h/.cpp`)
- Walk AST, emit `CommandSystem` calls for each node
- Handle loops (repeat/for) by iterating and emitting per-iteration
- Handle macros by inlining body with parameter substitution
- Handle `if/else` by evaluating conditions
- Handle `in <area>` by pushing/popping scope
- Fail-fast with line number + commands-emitted context
- Unit tests: `tests/CanvasDSLExecutor_test.cpp`

### Step 3.4: Editor Integration
- Register `dsl_load`, `dsl`, `dsl_export` commands (replace Phase 1 stubs)
- Add menu items in `EditorPanels`
- Detect `.vdecanvas` extension in `main.cpp` for batch mode
- Integration tests

### Step 3.5: DSL Test Scripts
- `scripts/test_dsl_basic.vdecanvas` — simple shapes and colors
- `scripts/test_dsl_layers.vdecanvas` — multi-layer
- `scripts/test_dsl_macros.vdecanvas` — macro definitions and calls
- `scripts/test_dsl_include.vdecanvas` — include directive

---

## Phase 4: Advanced Features

Low-priority items for future work. No detailed planning needed yet.

- Delta-based undo (memory optimization)
- Sprite sheet mode (grid overlay, animation preview)
- Palette management UI
- Tiling preview (3×3 repeat)
- Additional export formats (JPEG, ICO)
- Macro recording (named sequences, key binding)
- ImGui docking layout
- Multi-document diff
- DSL visual debugger
- DSL autocomplete
- Parametric DSL scripts

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| GPU texture re-upload perf for large canvases | Medium | Phase 1 targets ≤256×256 pixel art; staging buffer optimization deferred to Phase 2 |
| ImGui texture descriptor leak | High | `onBeforeImGuiShutdown()` cleanup + `isImGuiVulkanBackendAvailable()` guard |
| Full-buffer undo snapshots use memory | Medium | 256×256×4 = 256KB per snapshot × 50 = 12.5MB — acceptable for Phase 1. Delta undo in Phase 4. |
| Canvas DSL scope creep | High | DSL is entirely Phase 3; Phase 1 registers only stubs. Clear phase boundary prevents early coupling. |
| stb_image_write duplicate symbol | Build break | Already verified: implementation is in `src/stb_impl.cpp` in `vde` lib. Resource Editor only includes headers. |

---

## Estimated Effort

| Step | Effort | Cumulative |
|------|--------|------------|
| Step 1: Scaffolding | Small | Small |
| Step 2: ImageDocument | Medium | Medium |
| Step 3: CanvasRegistry | Small | Medium |
| Step 4: CommandSystem | Medium | Medium-Large |
| Step 5: FileOperations | Small | Medium-Large |
| Step 6: ToolPalette | Small-Medium | Large |
| Step 7: EditorPanels | Large | Large |
| Step 8: Scene Integration | Large | Very Large |
| Step 9: Storage Integration | Small | Very Large |
| Step 10: Smoke Tests & Docs | Small | Very Large |
| **Phase 1 Total** | **Very Large** | |
| Phase 2: Polish | Medium (per item) | |
| Phase 3: Canvas DSL | Very Large | |
| Phase 4: Advanced | Large (per item) | |
