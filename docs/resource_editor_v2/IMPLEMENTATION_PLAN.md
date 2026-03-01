# VDE Resource Editor v2 — Implementation Plan

> **Status:** Plan  
> **Related:** [Editor Design](EDITOR_DESIGN.md) · [Canvas DSL](CANVAS_DSL.md) · [Parser & Command System](PARSER_AND_COMMAND_SYSTEM.md)

---

## Overview

This plan builds the Resource Editor v2 in **four phases**, each delivering a usable increment. Each phase is broken into numbered steps with file deliverables, verification criteria, and dependency chains.

**Starting state:** A working v1 scaffolding exists in `tools/resource_editor/` with:
- `main.cpp` — working entry point using `BaseToolGame`
- `ResourceEditorScene.h/.cpp` — scene with command system, canvas registry
- `ImageDocument.h/.cpp` — pixel buffer with undo/redo, draw primitives
- `CanvasRegistry.h/.cpp` — canvas storage with uint32_t IDs + names
- `CommandSystem.h/.cpp` — string dispatch with handler map
- `ToolPalette.h/.cpp` — tool state (tool type, brush, color)
- `EditorPanels.h/.cpp` — ImGui panels
- `FileOperations.h/.cpp` — load/save/export with COM dialogs

**Strategy:** Incrementally replace v1 internals while keeping the tool functional at each step. Existing files are refactored in place when possible. New subsystems (metadata types, command classes, DSL) are added alongside and wired in once ready.

---

## Phase 1: Core Command System Refactor (MVP)

**Goal:** Replace the ad-hoc command dispatch with the metadata-driven system. After this phase the editor has the same feature set as v1, but with clean architecture.

### Step 1 — Command Infrastructure Types

Create the foundational types that everything else depends on.

**Files to create:**
```
tools/resource_editor/commands/
    CommandTypes.h         # ParamType, ParamDescriptor, CommandMetadata, CommandScope,
                           # RGBAColor helpers, IntPair, IntRect
    CommandBase.h          # CommandBase, GlobalCommand, CanvasCommand base classes,
                           # CommandResult, CommandArgs, ParsedArg
    CommandArgParser.h     # CommandArgParser declaration
    CommandArgParser.cpp   # Parenthesis-aware tokenizer, tuple parsing, type validation
    CommandRegistry.h      # CommandRegistry singleton, REGISTER_COMMAND macro
    CommandRegistry.cpp    # Registry impl with compound-name longest-match lookup
    EditorContext.h        # EditorContext façade struct
```

**Files to modify:**
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

### Step 2 — EditorContext & Wiring

Wire `EditorContext` into the existing scene so commands can access shared state without referencing `ResourceEditorScene`.

**Files to modify:**
```
tools/resource_editor/ResourceEditorScene.h/.cpp
    # Add EditorContext member
    # Initialize it in scene setup
    # Pass it to CommandSystem
tools/resource_editor/CommandSystem.h/.cpp
    # Add CommandRegistry pointer and EditorContext pointer
    # Add new execute(commandLine) path that uses registry+parser
    # Keep old dispatch path temporarily (dual-path dispatch)
```

**Verification:**
- Editor launches and works as before (old path still active)
- `EditorContext` is fully populated and accessible

**Dependencies:** Step 1

---

### Step 3 — Migrate Core Commands

Convert the most-used command handlers from inline scene methods to standalone command classes. Migrate in batches, testing after each.

**Batch 3a — Canvas Management (Global):**
```
tools/resource_editor/commands/global/
    CreateCanvasCommand.h      # "create canvas <name> (w, h)"  
    SelectCommand.h            # "select <name>"  
    ListCommand.h              # "list"  
    DeleteCanvasCommand.h      # "delete <name>"  
    RenameCanvasCommand.h      # "rename <old> <new>"  
```

**Batch 3b — Drawing (Canvas):**
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

**Batch 3c — Edit Operations (Canvas):**
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

**Batch 3d — File & Utility (Global):**
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
```

**Files to modify per batch:**
```
tools/resource_editor/CMakeLists.txt     # Add each new file
tools/resource_editor/CommandSystem.cpp  # Switch dispatch for migrated commands to registry path
tools/resource_editor/ResourceEditorScene.cpp  # Remove migrated handler methods
```

**Verification (per batch):**
- Each migrated command works identically to its v1 version
- `help <command>` prints auto-generated help from metadata
- Parameter validation catches type errors before dispatch
- Old and new commands coexist during migration

**Dependencies:** Step 2

---

### Step 4 — Remove Legacy Dispatch

Once all commands are migrated, remove the old `std::map<std::string, HandlerFn>` dispatch.

**Files to modify:**
```
tools/resource_editor/CommandSystem.h/.cpp
    # Remove old handler map and registerCommand(name, fn) method
    # All dispatch goes through CommandRegistry + CommandArgParser
tools/resource_editor/ResourceEditorScene.h/.cpp
    # Remove all former handler method declarations
    # Scene becomes a thin coordinator
```

**Verification:**
- All commands work through the new pipeline
- `ResourceEditorScene` is significantly smaller
- No compilation warnings about unused methods

**Dependencies:** Step 3 (all batches)

---

### Step 5 — Smoke Test & Scripted Validation

Create a command script that exercises every command and verify it runs cleanly.

**Files to create:**
```
tools/resource_editor/scripts/
    smoke_test.txt             # Exercises all commands in sequence
smoketests/
    resource_editor_smoke.ps1  # Launches editor with --input-script, verifies exit code
```

**Verification:**
- `vde_resource_editor.exe < smoke_test.txt` runs all commands without errors
- Exit code 0
- Can be added to CI smoke test suite

**Dependencies:** Step 4

---

## Phase 2: Editor Polish & New Features

**Goal:** Add features that were designed but not implemented in v1, leveraging the clean command architecture.

### Step 6 — Named Colors & Color Palette

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
    # namedColors map is already there; ensure persistent storage via StorageManager
tools/resource_editor/EditorPanels.cpp
    # Add color palette UI panel showing named colors
```

**Verification:**
- `define color skin #FFCC99` registers the color
- `fill skin` resolves to `#FFCC99FF`
- Color names appear in REPL autocomplete suggestions
- Colors persist across sessions via StorageManager

**Dependencies:** Phase 1 complete

---

### Step 7 — REPL Autocomplete & Parameter Hints

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

### Step 8 — Draw Image, Rehost & Copyhost

Implement cross-canvas operations.

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

---

## Phase 3: Canvas DSL

**Goal:** Implement the `.vdecanvas` language as described in [Canvas DSL](CANVAS_DSL.md).

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

## Phase 4: Advanced & Polish

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

Currently `ImageDocument` stores CPU pixels and uploads to a GPU texture on every change.

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
Step 1 ─── Step 2 ─── Step 3 (a,b,c,d) ─── Step 4 ─── Step 5
                                                          │
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
| **Phase 1** — Core Refactor | 1–5 | 5–7 sessions | 5–7 |
| **Phase 2** — Polish & Features | 6–9 | 4–5 sessions | 9–12 |
| **Phase 3** — Canvas DSL | 10–13 | 6–8 sessions | 15–20 |
| **Phase 4** — Advanced | 14–17 | 3–4 sessions | 18–24 |

A "session" is one focused working block. The estimates assume AI-assisted implementation.

---

## File Organization (Final State)

```
tools/resource_editor/
    main.cpp
    ResourceEditorScene.h/.cpp         # Thin scene coordinator
    ImageDocument.h/.cpp               # Pixel buffer, undo/redo, draw primitives
    CanvasRegistry.h/.cpp              # Canvas storage, cross-canvas access
    CommandSystem.h/.cpp               # Thin dispatch (registry + parser)
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

**Begin with Step 1.** The command infrastructure types are the foundation that unblocks everything else. They can be written and tested in isolation before touching any existing code.

Implementation order:
1. Create `commands/` directory structure
2. Implement `CommandTypes.h` — all enums, structs, descriptors
3. Implement `CommandBase.h` — base classes with virtual interface
4. Implement `CommandArgParser` — tokenizer + tuple parsing + validation
5. Implement `CommandRegistry` — singleton + macro + compound lookup
6. Write unit tests for parser and registry
7. Proceed to Step 2 (wiring EditorContext)
