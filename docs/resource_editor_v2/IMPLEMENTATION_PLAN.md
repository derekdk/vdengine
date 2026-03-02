# VDE Resource Editor — Implementation Plan

> **Status:** Phase 2 Complete  
> **Related:** [Editor Design](EDITOR_DESIGN.md) · [Canvas DSL](CANVAS_DSL.md) · [Parser & Command System](PARSER_AND_COMMAND_SYSTEM.md)

---

## Overview

This plan builds the VDE Resource Editor in **four phases**, each delivering a usable increment. Each phase is broken into numbered steps with file deliverables, verification criteria, and dependency chains.

The Resource Editor is a 2D pixel art editor built on the VDE Game API with a full ImGui interface. It is structured around a **metadata-driven command system** where every mutation flows through named, typed commands. The tool supports interactive GUI editing, a command console (REPL), script execution, and a declarative Canvas DSL for procedural asset generation.

### Core Architecture

- **Command-First** — Every state mutation is a named command with typed parameters. GUI actions generate command strings. Nothing changes state outside the command pipeline.
- **One Command = One File** — Each command is a self-contained class with structured metadata. Adding a command means creating one file and one CMake entry.
- **Metadata-Driven** — Command metadata powers auto-generated help, pre-dispatch validation, REPL autocomplete, and ImGui tooltip hints.
- **Canvas-Per-Resource** — Each open image gets its own Canvas with a unique ID and name. One canvas is active at a time; others are addressed via `@name` prefix.

### Strategy

Build the editor from the ground up in four phases:

1. **Phase 1 — Core Architecture (MVP):** Command infrastructure, editor subsystems, all core commands, and smoke tests. Delivers a fully functional multi-canvas pixel editor.
2. **Phase 2 — Features & Polish:** Named colors, REPL autocomplete, cross-canvas operations, and advanced drawing primitives.
3. **Phase 3 — Canvas DSL:** The `.vdecanvas` declarative language — parser, expression evaluator, and executor.
4. **Phase 4 — Advanced & Production:** Persistent settings, GPU optimization, comprehensive test suite, and CI integration.

---

## Phase 1: Core Architecture (MVP)

**Goal:** Build the complete editor with a clean metadata-driven command system. After this phase, the editor is a fully functional multi-canvas pixel art tool with GUI, REPL, script execution, and undo/redo.

### Step 1 — Command Infrastructure Types

> **Status:** ✅ Complete

Create the foundational types that the entire command system depends on.

**Files to create:**
```
tools/resource_editor/commands/
    CommandTypes.h         # ParamType, ParamDescriptor, CommandMetadata, CommandScope,
                           # RGBAColor helpers, IntPair, IntRect
    CommandBase.h          # CommandBase, GlobalCommand, CanvasCommand base classes,
                           # CommandResult, CommandArgs, ParsedArg
    CommandBase.cpp        # GlobalCommand::execute(), CanvasCommand::execute() with canvas resolution
    CommandArgParser.h     # CommandArgParser declaration
    CommandArgParser.cpp   # Parenthesis-aware tokenizer, tuple parsing, type validation
    CommandRegistry.h      # CommandRegistry singleton, REGISTER_COMMAND macro
    CommandRegistry.cpp    # Registry impl with compound-name longest-match lookup
    EditorContext.h        # EditorContext façade struct
    EditorContext.cpp      # getActiveCanvas() implementation (wires to CanvasRegistry + CommandSystem)
    AllCommands.cpp        # Includes all command headers to trigger REGISTER_COMMAND static initializers
```

**Files to create/modify:**
```
tools/resource_editor/CMakeLists.txt   # Add new sources
```

**Verification:**
- Compiles cleanly with no linker errors
- Unit tests pass for `CommandArgParser`:
  - Tokenizer splits `"(10, 5) to (20, 15) with red"` correctly
  - Tuple parsing: `(10, 5)` → `IntPair{10, 5}`
  - Nested rect: `((0, 0), (32, 32))` → `IntRect{0, 0, 32, 32}`
  - Flat rect: `(0, 0, 32, 32)` → `IntRect{0, 0, 32, 32}`
  - Bare-pair fallback: `10 5` parsed as `IntPair` when Point expected
  - Color parsing: `#FF0000FF`, `#FF0000`, named colors
  - Keyword matching, enum validation
  - Missing required param → error
  - Unmatched paren → error
- Unit tests pass for `CommandRegistry`:
  - Register a mock command, find it by name
  - Find by alias
  - Compound name lookup: `"draw line"` found before fallback to `"draw"`

**Dependencies:** None (foundational step)

---

### Step 2 — Editor Subsystems

> **Status:** ✅ Complete

Build the core editor modules that commands will operate on.

**Files to create:**
```
tools/resource_editor/
    ImageDocument.h        # Pixel buffer, drawing primitives, undo/redo
    ImageDocument.cpp      # stb_image load/save, Bresenham line, flood fill, etc.
    CanvasRegistry.h       # Canvas struct, multi-document container (ID → Canvas)
    CanvasRegistry.cpp     # Create/remove/resolve canvases, cross-canvas resource refs
    ToolPalette.h          # Tool state (active tool, color, brush size)
    ToolPalette.cpp        # Mouse-to-command translation
    FileOperations.h       # OS file dialogs, image I/O wrappers
    FileOperations.cpp     # COM dialogs (Windows), stb_image I/O
```

**Key design decisions:**
- `ImageDocument` is a pure pixel data model — no knowledge of canvases, commands, or GPU textures.
- `Canvas` bundles an `ImageDocument` with display state (zoom, pan), GPU texture handle, named resources, and operation history.
- `CanvasRegistry` manages multi-document editing with unique IDs and names.
- `ToolPalette` translates mouse interactions into command strings without executing them.
- `FileOperations` isolates platform-specific file dialog code.

**Verification:**
- `ImageDocument` can create, load, save, draw primitives, and undo/redo.
- `CanvasRegistry` can create, find, rename, and remove canvases.
- `ToolPalette` produces correct command strings for each tool type.
- All modules compile independently with no circular dependencies.

**Dependencies:** None (parallel with Step 1)

---

### Step 3 — Scene, Panels & Command System

> **Status:** ✅ Complete

Wire the subsystems into a functioning editor with ImGui UI and command dispatch.

**Files to create:**
```
tools/resource_editor/
    ResourceEditorScene.h      # Scene: owns all subsystems, wires everything
    ResourceEditorScene.cpp    # Initialization, per-frame GPU sync, panel delegation
    EditorPanels.h             # All ImGui panel rendering
    EditorPanels.cpp           # Canvas viewports, tool palette, color picker, console, etc.
    CommandSystem.h            # Thin dispatch (registry + parser + logging)
    CommandSystem.cpp          # @canvas prefix resolution, execute(), script execution
    main.cpp                   # Entry point, interactive/script mode selection
```

**Panels:**

| Panel | Purpose |
|-------|---------|
| Canvas Viewport(s) | Render GPU texture with zoom/pan, mouse interaction, pixel grid |
| Canvas Tabs | Tab bar for open canvases |
| Tool Palette | Tool buttons, brush size slider, color swatch |
| Color Picker | RGBA color editor with hex input |
| Properties | Canvas info (dimensions, path, dirty flag, zoom) |
| Command Console | REPL input + scrollable command/output log |
| Menu Bar | File, Edit, View menus |

**Verification:**
- Editor launches with ImGui UI visible
- Command console accepts text input and dispatches to `CommandSystem`
- `CommandSystem` resolves `@canvasName` prefix and routes to registry
- Script mode: `vde_resource_editor.exe script.txt` executes line-by-line and exits
- GPU textures sync for dirty canvases each frame

**Dependencies:** Steps 1, 2

---

### Step 4 — Core Commands

> **Status:** ✅ Complete (30 commands — 16 global, 14 canvas)

Implement all commands needed for a fully functional pixel editor. Built in four batches, each tested before moving on.

**Batch 4a — Canvas Management (Global):**
```
tools/resource_editor/commands/global/
    CreateCanvasCommand.h      # "create canvas <name> (w, h)"
    SelectCommand.h            # "select <name>"
    ListCommand.h              # "list"
    DeleteCanvasCommand.h      # "delete <name>"
    RenameCanvasCommand.h      # "rename <old> <new>"
```

**Batch 4b — Drawing (Canvas):**
```
tools/resource_editor/commands/canvas/
    FillCommand.h              # "fill <color>"
    SetCommand.h               # "set (x, y) <color>"
    DrawLineCommand.h          # "draw line (x1, y1) to (x2, y2) with <color> [width <n>]"
    DrawRectCommand.h          # "draw rect (x1, y1) to (x2, y2) with <color> [filled|outline]"
    DrawCircleCommand.h        # "draw circle (cx, cy) radius <r> with <color> [filled|outline]"
    DrawEllipseCommand.h       # "draw ellipse (cx, cy) (rx, ry) with <color> [filled|outline]"
    FloodFillCommand.h         # "flood (x, y) <color>"
```

**Batch 4c — Edit Operations (Canvas):**
```
tools/resource_editor/commands/canvas/
    UndoCommand.h              # "undo"
    RedoCommand.h              # "redo"
    ResizeCommand.h            # "resize (w, h)"
    CropCommand.h              # "crop (x, y) (w, h)"
    FlipCommand.h              # "flip horizontal|vertical"
    RotateCommand.h            # "rotate 90|180|270"
    ClearCommand.h             # "clear"
```

**Batch 4d — File & Utility (Global):**
```
tools/resource_editor/commands/global/
    LoadCommand.h/.cpp         # "load [canvas] \"path\" [name]" (complex, needs .cpp)
    SaveCommand.h              # "save [canvas] [\"path\"]"
    ExportCommand.h            # "export [canvas] \"path\" [format]"
    HelpCommand.h              # "help [command]" — reads metadata
    HistoryCommand.h           # "history [n]"
    ZoomCommand.h              # "zoom <level>"
    GridCommand.h              # "grid on|off"
    ExitCommand.h              # "exit"
    SetColorCommand.h          # "setcolor <color>" — set active drawing color
    SetToolCommand.h           # "settool <tool>" — set active tool
    SetSizeCommand.h           # "setsize <n>" — set brush size
```

**Files to modify per batch:**
```
tools/resource_editor/CMakeLists.txt     # Add each new file
```

**Verification (per batch):**
- Each command validates parameters before execution
- `help <command>` prints auto-generated help from metadata
- Parameter validation catches type errors before dispatch
- All commands work through the REPL console and via script execution
- Undo/redo works correctly for all drawing and edit operations

**Dependencies:** Step 3

---

### Step 5 — Smoke Test & Scripted Validation

> **Status:** ✅ Complete (verified: all 30 commands pass, exit code 0)

Create a command script that exercises every command and verify it runs cleanly.

**Files created:**
```
tools/resource_editor/scripts/
    smoke_test.txt                    # Exercises all 30 commands in sequence
smoketests/
    resource_editor_smoke.ps1         # Launches editor in script mode, verifies exit code 0
    scripts/
        smoke_resource_editor.vdescript  # GUI smoke script for scripts/smoke-test.ps1
```

**Bugs found and fixed during testing:**
- `onEnter()` was never called in script mode (game loop deferred via `setActiveScene`);
  added explicit `scene->onEnter()` call in `ResourceEditorGame::onStart()` before script execution
- `CommandRegistry::findExact()` added to prevent two-word lookup fallback from stripping
  args (e.g. `help create canvas` routing to `help` and treating `canvas` as unknown command)
- `SaveCommand` filepath param changed from `ParamType::String` to `ParamType::QuotedString`
  so quoted filenames are stripped correctly (e.g. `save "output.png"`)
- `cleanupImGuiTextures()` now releases `canvas->gpuTexture` shared_ptr before Vulkan teardown
- `smoke_test.txt` corrected: `flood` requires `with` keyword; `resize` takes bare ints not a tuple

**Verification:**
- `.\smoketests\resource_editor_smoke.ps1` → PASSED (exit code 0), all 30 commands succeed
- `scripts/smoke-test.ps1` maps `vde_resource_editor.exe` → GUI smoke via vdescript
- 784/784 unit tests continue to pass

**Dependencies:** Step 4

---

## Phase 2: Features & Polish

**Goal:** Add features that leverage the clean command architecture — named colors, autocomplete, cross-canvas operations, and advanced drawing.

> **Status:** ✅ Complete

### Step 6 — Named Colors & Color Palette

> **Status:** ✅ Complete

**Files to create:**
```
tools/resource_editor/commands/global/
    DefineColorCommand.h       # "define color <name> <hex>"
    ListColorsCommand.h        # "list colors"
    UndefineColorCommand.h     # "undefine color <name>"
```

**Files to modify:**
```
tools/resource_editor/EditorContext.h
    # namedColors map for custom color definitions
tools/resource_editor/EditorPanels.cpp
    # Add color palette UI panel showing named colors
```

**Verification:**
- `define color skin #FFCC99` registers the color
- `fill skin` resolves to `#FFCC99FF`
- Color names appear in REPL autocomplete suggestions
- Colors persist across sessions via StorageManager
  - **Deferred to Phase 4 (Step 14)** — colors are in-memory only for now

**Dependencies:** Phase 1 complete

---

### Step 7 — REPL Autocomplete & Parameter Hints

> **Status:** ✅ Complete

Enhance the ImGui command input with metadata-driven features.

**Files to modify:**
```
tools/resource_editor/EditorPanels.cpp
    # Add autocomplete popup for command names
    # Add parameter hint ghost text after command recognized
    # Add enum value dropdown for Enum params
    # Add color name completion
```

**Verification:**
- Typing `dr` shows `draw line`, `draw rect`, `draw circle`, etc.
- After `draw line ` the hint shows `(x1, y1) to (x2, y2) with <color>`
- Tab-completing enum values works

**Dependencies:** Step 6

---

### Step 8 — Cross-Canvas Operations

> **Status:** ✅ Complete
>
> Implement resource transfer and compositing between canvases.

**Files to create:**
```
tools/resource_editor/commands/canvas/
    DrawImageCommand.h         # "draw <img> [layer] (x, y) (w, h)"
tools/resource_editor/commands/global/
    RehostCommand.h            # "rehost <type> <name> [from <src>] to <dest>"
    CopyhostCommand.h          # "copyhost <type> <name> [from <src>] to <dest> [as <new>]"
```

**Files to modify:**
```
tools/resource_editor/CanvasRegistry.h/.cpp
    # Add methods: transferImage(), copyImage()
    # Add cross-canvas accessor: resolve("hero::face")
tools/resource_editor/CMakeLists.txt
```

**Verification:**
- `load hero "face.png" face` then `copyhost image face from hero to body as face_copy`
- `@body draw face_copy (0, 0) (32, 32)` composites correctly
- `rehost image face from hero to body` removes from hero, adds to body

**Dependencies:** Step 6

---

### Step 9 — Arc, Bézier & Advanced Drawing

> **Status:** ✅ Complete

**Files to create:**
```
tools/resource_editor/commands/canvas/
    DrawArcCommand.h           # "draw arc (cx, cy) radius <r> from <a1> to <a2> with <color>"
    DrawBezierCommand.h        # "draw bezier (p0) (p1) (p2) (p3) with <color>"
```

**Files to modify:**
```
tools/resource_editor/ImageDocument.h/.cpp
    # Add drawArc(), drawBezier() methods
tools/resource_editor/CMakeLists.txt
```

**Verification:**
- Arc draws correct segment of circle
- Bézier draws smooth curve through control points
- Both work with undo/redo

**Dependencies:** Step 6

### Phase 2 Implementation Notes

**Test coverage:** 21 new tests in `tests/Phase2Commands_test.cpp` plus updated smoke test
(`tools/resource_editor/scripts/smoke_test.txt`) with 61 commands total. All 79 resource editor
unit tests pass.

**Known deviations to track for future phases:**

| Priority | Item | Target Phase |
|----------|------|-------------|
| High | `DrawImageCommand` registered as `"draw image"` — should add `"draw"` alias for DSL fallback | Phase 3 |
| Medium | Quadratic Bézier (3-point) not supported — only cubic (4-point) | Phase 3 |
| Low | Tuple bracket matching in REPL not implemented | Polish |
| Low | Arc angles are `Int` — could be `Float` for sub-degree precision | Polish |
| Low | `[layer]` parameter for `draw image` deferred until layer system exists | Phase 3 |
| Low | `rehost`/`copyhost` `type` enum only supports `"image"` — `"color"`, `"area"` deferred | Future |
| Low | Named color persistence deferred from Step 6 to Phase 4 (Step 14) | Polish |
| Low | Autocomplete logic is ImGui-coupled — consider extracting for unit testability | Testing |

**Intentional naming decision:** `define color` (not `create color`) was chosen for the command
name to avoid collision with `create canvas` and because `define` better conveys establishing an
alias rather than creating a resource.

---

## Phase 3: Canvas DSL

**Goal:** Implement the `.vdecanvas` declarative language as described in [Canvas DSL](CANVAS_DSL.md).

### Step 10 — DSL Parser & AST

**Files to create:**
```
tools/resource_editor/dsl/
    CanvasDSLTypes.h           # ASTNode, NodeType, ObjectType, Symbol, BoundScope
    CanvasDSLParser.h          # CanvasDSLParser class declaration
    CanvasDSLParser.cpp        # Tokenizer, statement parser, include resolution, validation
```

**Files to modify:**
```
tools/resource_editor/CMakeLists.txt
```

**Verification:**
- Unit tests: parse simple `.vdecanvas` files → correct AST
- Error cases: undefined references, duplicate names, syntax errors — all caught
- Include resolution: `include "palette.vdecanvas"` merges symbols
- Circular include detection

**Dependencies:** Phase 2 complete

---

### Step 11 — Expression Evaluator

**Files to create:**
```
tools/resource_editor/dsl/
    CanvasDSLExprEval.h        # Expression evaluator declaration
    CanvasDSLExprEval.cpp      # Arithmetic + bound variable resolution (lb, rb, cx, w, etc.)
```

**Verification:**
- `lb + 2` evaluates correctly given a BoundScope
- `w / 2` evaluates to half-width
- `cx` resolves to center x
- Nested expressions: `(rb - lb) / 4`
- Unknown variable → error

**Dependencies:** Step 10

---

### Step 12 — DSL Executor

**Files to create:**
```
tools/resource_editor/dsl/
    CanvasDSLExecutor.h        # Executor declaration
    CanvasDSLExecutor.cpp      # AST walker, symbol resolution, command emission
```

**Files to create (commands):**
```
tools/resource_editor/commands/global/
    DslLoadCommand.h           # "dsl_load <filepath>"
    DslExportCommand.h         # "dsl_export <canvas> <filepath>"
```

**Verification:**
- A `.vdecanvas` file that creates a canvas, defines colors, draws shapes → produces correct pixel output
- All emitted commands appear in the command log
- `dsl_load hero.vdecanvas` then undo walks back each emitted command
- Batch mode: `vde_resource_editor.exe --dsl hero.vdecanvas` exits with code 0

**Dependencies:** Steps 10, 11

---

### Step 13 — DSL Advanced Features

Implement control flow, macros, and complex object types.

**Scope:**
- `repeat N { ... }` loops
- `for var in range(start, end) { ... }` loops
- `if <condition> { ... }` conditionals
- `macro <name>(<params>) { ... }` definitions and calls
- Gradient objects (linear, radial) with per-pixel fill
- Pattern objects (checkerboard, stripe, etc.)
- Layer management (create, select, flatten)

**Verification:**
- Loop generates correct number of iterations
- Macro with parameters expands correctly
- Gradients produce smooth color transitions
- Layer operations maintain correct compositing order

**Dependencies:** Step 12

---

## Phase 4: Advanced & Production

**Goal:** Production-quality features, performance, and CI integration.

### Step 14 — Persistent Settings

**Files to modify:**
```
tools/resource_editor/ResourceEditorScene.cpp
    # Save/restore: window layout, recent files, named colors, grid state, zoom
    # Use StorageManager from VDE API
```

**Verification:**
- Close and reopen — settings are preserved
- Named colors survive restart
- Recent files list populated

**Dependencies:** Phase 3 complete

---

### Step 15 — GPU Texture Pipeline Optimization

`ImageDocument` stores CPU pixels and uploads to a GPU texture on every change.

**Files to modify:**
```
tools/resource_editor/ImageDocument.h/.cpp
    # Track dirty region (bounding box of changed pixels)
    # Only re-upload dirty region via partial texture update
tools/resource_editor/CanvasRegistry.h/.cpp
    # Batch GPU uploads per frame (one upload per dirty canvas, not per command)
```

**Verification:**
- Large canvases (256×256, 512×512) remain responsive during drawing
- GPU upload cost measured before/after optimization

**Dependencies:** Phase 3 complete

---

### Step 16 — Comprehensive Test Suite

**Files to create:**
```
tests/tools/
    test_CommandArgParser.cpp    # Tokenizer, tuple parsing, all param types
    test_CommandRegistry.cpp     # Registration, lookup, compound names, aliases
    test_CanvasDSLParser.cpp     # AST generation, error reporting, includes
    test_CanvasDSLExprEval.cpp   # Expression evaluation with bound scopes
    test_CanvasDSLExecutor.cpp   # End-to-end DSL → command emission
    test_ImageDocument.cpp       # Pixel operations, undo/redo correctness
```

**Verification:**
- All tests pass
- Coverage of edge cases: empty args, max-length commands, unicode in strings
- DSL error reporting tests verify line numbers and suggestions

**Dependencies:** Can be written incrementally alongside Phases 1–3

---

### Step 17 — CI Smoke Test Integration

**Files to create/modify:**
```
smoketests/resource_editor/
    basic_commands.txt           # Create, draw, save, verify
    dsl_smoke.vdecanvas          # DSL script that produces known output
    verify_output.ps1            # Compare exported PNG against reference
scripts/smoke-test.ps1
    # Add resource_editor to the smoke test runner
```

**Verification:**
- Smoke tests run in CI, exit 0 on success
- Regression detected if pixel output changes unexpectedly

**Dependencies:** Steps 5, 12

---

## Dependency Graph

```
Step 1 ───┐
           ├── Step 3 ─── Step 4 (a,b,c,d) ─── Step 5
Step 2 ───┘                                       │
                                       ┌───────────┘
                                       ▼
                                  Step 6 ─── Step 7
                                       │         │
                                  Step 8    Step 9
                                       │         │
                                       ▼         ▼
                                  Step 10 ─── Step 11 ─── Step 12 ─── Step 13
                                                               │
                                       ┌───────────────────────┘
                                       ▼
                             Step 14, 15, 16, 17

Steps 1 and 2 can proceed in parallel.
Tests (Step 16) grow incrementally alongside all phases.
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Static registration order (REGISTER_COMMAND) varies across compilers | Medium | Low | Registry populated before first use; no ordering dependency between commands |
| Compound command name conflicts | Low | Medium | Longest-match lookup with deterministic ordering; unit test coverage |
| DSL expression evaluator complexity creeps | Medium | Medium | Phase 3 starts with integer-only expressions; float/string support deferred |
| GPU partial upload not supported on all drivers | Low | High | Fallback to full re-upload if partial fails; optimization is opt-in |
| Large `.vdecanvas` files slow to parse | Low | Low | Lazy parsing; users typically work with small scripts |
| Undo across rehost (two-canvas transaction) | Medium | Medium | Phase 2 rehost is non-undoable; transaction model deferred |

---

## Effort Estimates

| Phase | Steps | Estimated Effort | Cumulative |
|-------|-------|-----------------|------------|
| **Phase 1** — Core Architecture | 1–5 | 6–8 sessions | 6–8 |
| **Phase 2** — Features & Polish | 6–9 | 4–5 sessions | 10–13 |
| **Phase 3** — Canvas DSL | 10–13 | 6–8 sessions | 16–21 |
| **Phase 4** — Advanced | 14–17 | 3–4 sessions | 19–25 |

A "session" is one focused working block. The estimates assume AI-assisted implementation.

---

## File Organization (Final State)

```
tools/resource_editor/
    CMakeLists.txt
    main.cpp                           # Entry point, interactive/script mode
    ResourceEditorScene.h/.cpp         # Thin scene coordinator
    ImageDocument.h/.cpp               # Pixel buffer, undo/redo, draw primitives
    CanvasRegistry.h/.cpp              # Canvas storage, cross-canvas access
    CommandSystem.h/.cpp               # Thin dispatch (registry + parser + log)
    EditorContext.h                     # Shared state façade
    ToolPalette.h/.cpp                 # Active tool, brush, color state
    EditorPanels.h/.cpp                # ImGui panel rendering
    FileOperations.h/.cpp              # OS file dialogs, image I/O
    commands/
        CommandTypes.h                 # Enums, descriptors, metadata
        CommandBase.h                  # Base classes
        CommandArgParser.h/.cpp        # Metadata-driven parser
        CommandRegistry.h/.cpp         # Singleton registry + macro
        global/
            CreateCanvasCommand.h
            SelectCommand.h
            ListCommand.h
            DeleteCanvasCommand.h
            RenameCanvasCommand.h
            LoadCommand.h/.cpp
            SaveCommand.h
            ExportCommand.h
            HelpCommand.h
            HistoryCommand.h
            ZoomCommand.h
            GridCommand.h
            ExitCommand.h
            DefineColorCommand.h
            ListColorsCommand.h
            UndefineColorCommand.h
            RehostCommand.h
            CopyhostCommand.h
            DslLoadCommand.h
            DslExportCommand.h
        canvas/
            FillCommand.h
            SetCommand.h
            DrawLineCommand.h
            DrawRectCommand.h
            DrawCircleCommand.h
            DrawEllipseCommand.h
            DrawArcCommand.h
            DrawBezierCommand.h
            DrawImageCommand.h
            FloodFillCommand.h
            UndoCommand.h
            RedoCommand.h
            ResizeCommand.h
            CropCommand.h
            FlipCommand.h
            RotateCommand.h
            ClearCommand.h
    dsl/
        CanvasDSLTypes.h
        CanvasDSLParser.h/.cpp
        CanvasDSLExprEval.h/.cpp
        CanvasDSLExecutor.h/.cpp
    scripts/
        smoke_test.txt
```

---

## Getting Started

**Begin with Steps 1 and 2 in parallel.** The command infrastructure types (Step 1) and editor subsystems (Step 2) are independent foundations that can be built simultaneously. They converge in Step 3 where the scene wires everything together.

Step 1 implementation order:
1. Create `commands/` directory structure
2. Implement `CommandTypes.h` — all enums, structs, descriptors
3. Implement `CommandBase.h` — base classes with virtual interface
4. Implement `CommandArgParser` — tokenizer + tuple parsing + validation
5. Implement `CommandRegistry` — singleton + macro + compound lookup
6. Write unit tests for parser and registry

Step 2 implementation order:
1. Implement `ImageDocument` — pixel buffer + drawing primitives + undo/redo
2. Implement `CanvasRegistry` — Canvas struct + multi-document container
3. Implement `ToolPalette` — tool state + mouse-to-command translation
4. Implement `FileOperations` — image I/O + native file dialogs
