# Command System Refactor — Design & Implementation Plan

## Problem Statement

The current command system suffers from several scaling issues:

1. **God-class `ResourceEditorScene`** — All ~25 command handlers are methods on the scene class (1400+ lines). Adding a command requires touching the `.h` (declaration), `.cpp` (implementation), and the registration block — three places for every new command.

2. **No structured metadata** — Help text is a freeform string. There's no machine-readable description of parameter names, types, or optionality. The editor UI cannot display parameter hints, validate arguments before dispatch, or offer contextual autocomplete.

3. **Ad-hoc argument parsing** — Each handler does its own `istringstream` parsing with inconsistent patterns. Some use keyword separators (`to`, `with`), some don't. Error messages vary in style and helpfulness.

4. **No object mobility** — Resources (images, colors, areas) are permanently bound to the canvas they were created in. There's no way to re-host an object into a different canvas or into the root scope, or to duplicate it between canvases. The `::` accessor provides read-only cross-canvas access but not ownership transfer.

## Goals

1. **One command = one file** — Each command is a self-contained class in its own header with structured metadata. Adding a new command means creating one file and adding it to CMake.

2. **Rich metadata** — Every command declares its parameters as typed descriptors. The system uses metadata for:
   - Tab-completion with parameter type hints in the REPL
   - Validation before handler execution
   - `help <command>` auto-generated from metadata
   - ImGui tooltip/autocomplete overlays

3. **Uniform argument parsing** — A shared `CommandArgs` parser extracts and validates arguments based on metadata, producing a typed argument map. Handlers receive pre-parsed, pre-validated arguments.

4. **Rehost / Copyhost** — New commands to transfer or duplicate any registered object between canvases (or to/from root scope).

---

## Architecture

### Class Hierarchy

```
CommandBase                  (abstract — defines metadata + execute interface)
├── GlobalCommand            (not canvas-scoped; receives parsed args)
└── CanvasCommand            (canvas-scoped; receives canvasId + parsed args)
```

`CommandBase` is a traditional abstract base class with virtual `execute()`. CRTP was considered but rejected: each command is registered in a polymorphic registry, so virtual dispatch is needed anyway, and the metadata reflection pattern doesn't benefit from compile-time polymorphism.

### Key Types

```cpp
namespace vde::tools {

// ─── Parameter metadata ───

enum class ParamType {
    Int,        // Integer (e.g., single coordinate, count)
    Float,      // Float (e.g., zoom level, opacity 0.0-1.0)
    String,     // Unquoted token (e.g., canvas name, tool name)
    QuotedString, // Quoted string (e.g., file paths with spaces: "assets/hero.png")
    Color,      // #RRGGBB or #RRGGBBAA hex, or named color
    Bool,       // true/false, filled/outline, show/hide
    Keyword,    // A fixed keyword separator (e.g., "to", "with", "as")
    Enum,       // One of a fixed set of string values
    Point,      // 2D coordinate pair: (x, y) — parsed from parenthesized tuple
    Size,       // 2D dimensions: (w, h) — parsed from parenthesized tuple
    Rect,       // Rectangle: ((x, y), (w, h)) or (x, y, w, h) — nested tuple
};

struct ParamDescriptor {
    std::string name;           // e.g., "x", "color", "direction"
    ParamType type = ParamType::String;
    bool required = true;
    std::string description;    // Short description for help/hints
    std::string defaultValue;   // Default if optional and omitted (empty = no default)
    std::vector<std::string> enumValues; // Valid values when type == Enum
};

// ─── Parsed arguments ───

/// A 2D integer point (coordinates or dimensions).
struct IntPair {
    int x = 0, y = 0;
};

/// A rectangle: origin point + size.
struct IntRect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct ParsedArg {
    std::string raw;            // Original string token(s)
    ParamType type;
    // Typed accessors (return default/throw on type mismatch):
    int asInt() const;
    float asFloat() const;
    bool asBool() const;
    const std::string& asString() const;
    RGBAColor asColor() const;  // Resolves named colors via registry
    IntPair asPoint() const;    // For ParamType::Point and ParamType::Size
    IntPair asSize() const;     // Alias — same storage, semantic distinction
    IntRect asRect() const;     // For ParamType::Rect

  private:
    friend class CommandArgParser;
    IntPair m_pair;             // Populated for Point/Size types
    IntRect m_rect;             // Populated for Rect type
};

/// Parsed argument map: parameter name → parsed value.
/// Provides safe typed access to pre-validated command arguments.
class CommandArgs {
  public:
    bool has(const std::string& name) const;
    const ParsedArg& get(const std::string& name) const;
    int getInt(const std::string& name) const;
    float getFloat(const std::string& name) const;
    bool getBool(const std::string& name) const;
    const std::string& getString(const std::string& name) const;
    RGBAColor getColor(const std::string& name) const;
    IntPair getPoint(const std::string& name) const;
    IntPair getSize(const std::string& name) const;
    IntRect getRect(const std::string& name) const;

    /// The raw arg string after all parsed params (for freeform trailing content).
    const std::string& remainder() const;

  private:
    friend class CommandArgParser;
    std::map<std::string, ParsedArg> m_args;
    std::string m_remainder;
};

// ─── Command metadata ───

enum class CommandScope {
    Global,     // Not canvas-scoped (help, list, exit, setcolor, etc.)
    Canvas,     // Operates on a canvas (paint, fill, undo, save, etc.)
};

struct CommandMetadata {
    std::string name;               // Primary command name (lowercase)
    std::vector<std::string> aliases; // Optional aliases (e.g., "q" for "exit")
    std::string category;           // For grouping in help: "File", "Drawing", "View", etc.
    std::string summary;            // One-line summary for command list
    std::string description;        // Detailed multi-line description
    CommandScope scope = CommandScope::Global;
    std::vector<ParamDescriptor> params;
    std::string syntaxExample;      // e.g., "draw line <x1> <y1> to <x2> <y2> with <color>"

    /// Generate a formatted help string from metadata.
    std::string formatHelp() const;
    /// Generate a usage line from params.
    std::string formatUsage() const;
};

// ─── Command base class ───

/// Result returned by command execution.
struct CommandResult {
    bool success = true;
    std::string message;            // Display message (empty = silent success)
};

class CommandBase {
  public:
    virtual ~CommandBase() = default;

    /// Return the command's static metadata.
    virtual const CommandMetadata& metadata() const = 0;

    /// Execute the command with pre-parsed arguments.
    /// For CanvasCommands, canvasId is the resolved target canvas.
    /// For GlobalCommands, canvasId is 0 and should be ignored.
    virtual CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                                  class EditorContext& ctx) = 0;
};

/// Convenience base for global commands that enforces scope.
class GlobalCommand : public CommandBase {
  public:
    // Final execute dispatches to executeGlobal after validating scope.
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) final;

  protected:
    virtual CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) = 0;
};

/// Convenience base for canvas-scoped commands that validates canvas existence.
class CanvasCommand : public CommandBase {
  public:
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) final;

  protected:
    /// Called with a guaranteed-valid canvas pointer.
    virtual CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                        EditorContext& ctx) = 0;
};

} // namespace vde::tools
```

### EditorContext — Shared State Façade

Instead of each command capturing `this` (the scene), all shared state is exposed through a single façade struct. This decouples commands from the scene class.

```cpp
/// Shared editor state accessible to all commands.
/// Commands receive this by reference. It provides safe access to all
/// subsystems without requiring knowledge of the scene class.
struct EditorContext {
    CanvasRegistry& canvases;
    CommandSystem& commands;        // For nested execution (e.g., "run" command)
    ToolPalette& palette;
    std::map<std::string, RGBAColor>& namedColors;

    // Convenience methods
    Canvas* getActiveCanvas();
    bool resolveColor(const std::string& token, RGBAColor& out) const;

    // For commands that need the Game (e.g., exit)
    vde::api::Game* game = nullptr;
};
```

### CommandArgParser — Metadata-Driven Parsing

```cpp
/// Parses a raw argument string using a command's ParamDescriptors.
/// Returns a CommandArgs map or a parse error.
class CommandArgParser {
  public:
    struct ParseResult {
        bool success = false;
        CommandArgs args;
        std::string error;      // Non-empty on failure
    };

    /// Parse the argument string according to the command's parameter list.
    /// Keywords are matched and consumed but not stored as named args.
    static ParseResult parse(const std::string& argsString,
                             const std::vector<ParamDescriptor>& params,
                             const EditorContext& ctx);

  private:
    /// Tokenize input, handling parenthesized groups as single tokens.
    /// e.g., "(10, 5) to (20, 15) with red" → ["(10, 5)", "to", "(20, 15)", "with", "red"]
    static std::vector<std::string> tokenize(const std::string& input);

    /// Parse a single token or parenthesized group according to its declared type.
    static bool parseToken(const std::string& token, ParamType type, ParsedArg& out);

    /// Parse a parenthesized tuple: "(a, b)" → IntPair, "((a,b),(c,d))" or "(a,b,c,d)" → IntRect.
    static bool parseTuple(const std::string& token, ParsedArg& out, ParamType expected);

    static bool validateEnum(const std::string& value, const std::vector<std::string>& allowed);
};
```

### Command Registry & Auto-Registration

```cpp
/// Singleton registry that owns all command instances.
/// Commands self-register via a static initializer macro.
class CommandRegistry {
  public:
    static CommandRegistry& instance();

    void registerCommand(std::unique_ptr<CommandBase> cmd);
    CommandBase* find(const std::string& name) const;
    std::vector<const CommandMetadata*> getAllMetadata() const;
    std::vector<const CommandMetadata*> getByCategory(const std::string& category) const;

  private:
    std::map<std::string, std::unique_ptr<CommandBase>> m_commands;
    std::map<std::string, CommandBase*> m_aliasIndex; // alias → command
};

/// Macro for self-registering commands. Place in the command's .cpp file.
/// Usage: REGISTER_COMMAND(MyPaintCommand)
#define REGISTER_COMMAND(CommandClass)                                    \
    static struct CommandClass##_Registrar {                              \
        CommandClass##_Registrar() {                                     \
            CommandRegistry::instance().registerCommand(                  \
                std::make_unique<CommandClass>());                        \
        }                                                                \
    } s_##CommandClass##_registrar;
```

### Revised CommandSystem

The `CommandSystem` becomes thinner — it delegates parsing and dispatch to `CommandRegistry` and `CommandArgParser`:

```cpp
class CommandSystem {
  public:
    /// Initialize: wire registry and context.
    void initialize(CommandRegistry& registry, EditorContext& ctx);

    /// Execute a command line. Resolves @canvas prefix, finds command,
    /// parses args via metadata, and dispatches.
    bool execute(const std::string& commandLine);

    // ... (log, script I/O, etc. — unchanged)

  private:
    CommandRegistry* m_registry = nullptr;
    EditorContext* m_ctx = nullptr;
    // ... existing log, active canvas, etc.
};
```

Execution flow becomes:

```
commandLine
    → resolveCommand() extracts @canvas prefix, command name, raw args
    → CommandRegistry::find(name) returns CommandBase*
    → CommandArgParser::parse(rawArgs, cmd->metadata().params, ctx)
        → validates types, resolves keywords, fills CommandArgs map
        → returns error if validation fails (before handler runs)
    → cmd->execute(canvasId, parsedArgs, ctx)
        → handler operates on typed, validated data
        → returns CommandResult
    → log entry recorded with result
```

---

## File Organization

```
tools/resource_editor/
    commands/
        CommandBase.h           # CommandBase, GlobalCommand, CanvasCommand, CommandResult
        CommandArgs.h           # CommandArgs, ParsedArg
        CommandArgs.cpp         # Typed accessor implementations
        CommandArgParser.h      # Metadata-driven argument parser
        CommandArgParser.cpp
        CommandMetadata.h       # CommandMetadata, ParamDescriptor, ParamType
        CommandRegistry.h       # CommandRegistry singleton, REGISTER_COMMAND macro
        CommandRegistry.cpp
        EditorContext.h         # EditorContext façade struct

        # ── Global commands ──
        global/
            HelpCommand.h
            ListCommand.h
            CreateCommand.h       # create canvas | create color | create image (dispatches internally)
            LoadCommand.h
            LoadCommand.cpp       # Complex parsing (quoted paths) warrants a .cpp
            SelectCommand.h
            SetColorCommand.h
            SetToolCommand.h
            SetSizeCommand.h
            LogCommand.h
            RunCommand.h
            ExitCommand.h
            RehostCommand.h       # NEW: rehost <type> <name> [from <canvas>] to <canvas>
            RehostCommand.cpp
            CopyhostCommand.h     # NEW: copyhost <type> <name> [from <canvas>] to <canvas> [as <newname>]
            CopyhostCommand.cpp

        # ── Canvas commands ──
        canvas/
            SetPixelCommand.h     # "set (x, y) <color>"
            FillCommand.h
            FloodFillCommand.h
            DrawLineCommand.h
            DrawRectCommand.h
            DrawCircleCommand.h
            DrawImageCommand.h
            DrawImageCommand.cpp
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
```

Simple commands are header-only. Complex commands (those with non-trivial parsing like `load`, `draw image`, `rehost`, `copyhost`) have `.cpp` files.

---

## Example Command Implementation

### Simple Canvas Command: `FillCommand`

```cpp
// tools/resource_editor/commands/canvas/FillCommand.h
#pragma once

#include "../CommandBase.h"

namespace vde::tools::commands {

class FillCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "fill",
            .aliases = {},
            .category = "Drawing",
            .summary = "Fill entire canvas with a color",
            .description = "Fills all pixels of the target canvas with the specified color. "
                           "Supports hex literals (#RRGGBB, #RRGGBBAA) and named colors.",
            .scope = CommandScope::Canvas,
            .params = {
                {.name = "color", .type = ParamType::Color, .required = true,
                 .description = "Fill color (hex or named)"},
            },
            .syntaxExample = "fill <color>",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& ctx) override {
        RGBAColor color = args.getColor("color");
        canvas.document->snapshotForUndo();
        canvas.document->fill(color);
        return {true, "OK"};
    }
};

REGISTER_COMMAND(FillCommand)

} // namespace vde::tools::commands
```

### Canvas Command with Tuples: `DrawLineCommand`

Demonstrates `ParamType::Point` — the parser consumes `(x, y)` parenthesized groups
and maps them to `IntPair` values. The handler accesses `.x` and `.y` directly.

```cpp
// tools/resource_editor/commands/canvas/DrawLineCommand.h
#pragma once

#include "../CommandBase.h"

namespace vde::tools::commands {

class DrawLineCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw line",       // Space-separated compound command
            .aliases = {"line"},
            .category = "Drawing",
            .summary = "Draw a line between two points",
            .description = "Draws a line from start to end in the specified color. "
                           "Points are expressed as (x, y) tuples. "
                           "Optional width parameter controls line thickness.",
            .scope = CommandScope::Canvas,
            .params = {
                {.name = "start", .type = ParamType::Point, .required = true,
                 .description = "Start point (x, y)"},
                {.name = "to", .type = ParamType::Keyword, .required = true},
                {.name = "end", .type = ParamType::Point, .required = true,
                 .description = "End point (x, y)"},
                {.name = "with", .type = ParamType::Keyword, .required = true},
                {.name = "color", .type = ParamType::Color, .required = true,
                 .description = "Line color"},
                {.name = "width", .type = ParamType::Keyword, .required = false},
                {.name = "thickness", .type = ParamType::Int, .required = false,
                 .description = "Line thickness", .defaultValue = "1"},
            },
            .syntaxExample = "draw line (x1, y1) to (x2, y2) with <color> [width <n>]",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& ctx) override {
        auto [x1, y1] = args.getPoint("start");
        auto [x2, y2] = args.getPoint("end");
        RGBAColor color = args.getColor("color");
        int thickness = args.has("thickness") ? args.getInt("thickness") : 1;

        canvas.document->snapshotForUndo();
        canvas.document->drawLine(x1, y1, x2, y2, color, thickness);
        return {true, "OK"};
    }
};

REGISTER_COMMAND(DrawLineCommand)

} // namespace vde::tools::commands
```

### Canvas Command with Size Tuple: `DrawRectCommand`

Shows two `Point` params and an `Enum` for fill style.

```cpp
// tools/resource_editor/commands/canvas/DrawRectCommand.h
#pragma once

#include "../CommandBase.h"

namespace vde::tools::commands {

class DrawRectCommand final : public CanvasCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw rect",
            .aliases = {"rect"},
            .category = "Drawing",
            .summary = "Draw a rectangle",
            .description = "Draws a rectangle between two corner points. "
                           "Points are (x, y) tuples. Default style is filled.",
            .scope = CommandScope::Canvas,
            .params = {
                {.name = "start", .type = ParamType::Point, .required = true,
                 .description = "Top-left corner (x, y)"},
                {.name = "to", .type = ParamType::Keyword, .required = true},
                {.name = "end", .type = ParamType::Point, .required = true,
                 .description = "Bottom-right corner (x, y)"},
                {.name = "with", .type = ParamType::Keyword, .required = true},
                {.name = "color", .type = ParamType::Color, .required = true,
                 .description = "Fill/stroke color"},
                {.name = "style", .type = ParamType::Enum, .required = false,
                 .description = "filled or outline", .defaultValue = "filled",
                 .enumValues = {"filled", "outline"}},
            },
            .syntaxExample = "draw rect (x1, y1) to (x2, y2) with <color> [filled|outline]",
        };
        return meta;
    }

  protected:
    CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                EditorContext& ctx) override {
        auto [x1, y1] = args.getPoint("start");
        auto [x2, y2] = args.getPoint("end");
        RGBAColor color = args.getColor("color");
        bool filled = !args.has("style") || args.getString("style") != "outline";

        int x = std::min(x1, x2);
        int y = std::min(y1, y2);
        int w = std::abs(x2 - x1) + 1;
        int h = std::abs(y2 - y1) + 1;

        canvas.document->snapshotForUndo();
        canvas.document->drawRect(x, y, w, h, color, filled);
        return {true, "OK"};
    }
};

REGISTER_COMMAND(DrawRectCommand)

} // namespace vde::tools::commands
```

### Global Command with Complex Parsing: `LoadCommand`

Commands with non-standard parsing (e.g., quoted strings, multi-part dispatch) override a `customParse` hook or use a `.cpp` file with manual parsing that still populates `CommandArgs`:

```cpp
// tools/resource_editor/commands/global/LoadCommand.h
#pragma once

#include "../CommandBase.h"

namespace vde::tools::commands {

class LoadCommand final : public GlobalCommand {
  public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "load",
            .aliases = {},
            .category = "File",
            .summary = "Load an image file into a canvas",
            .description =
                "Load an image file as a named resource in a canvas. "
                "If the canvas doesn't exist, creates one sized to the image. "
                "If no arguments are given, opens a file dialog.",
            .scope = CommandScope::Global,
            .params = {
                {.name = "canvasname", .type = ParamType::String, .required = false,
                 .description = "Target canvas name (created if absent)"},
                {.name = "filepath", .type = ParamType::QuotedString, .required = false,
                 .description = "Path to the image file"},
                {.name = "imagename", .type = ParamType::String, .required = false,
                 .description = "Resource name (defaults to filename stem)"},
            },
            .syntaxExample = "load <canvasname> \"<filepath>\" [imagename]",
        };
        return meta;
    }

    /// Load uses custom parsing because of the optional dialog flow.
    bool usesCustomParsing() const override { return true; }

  protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override;
    // Implementation in LoadCommand.cpp
};

REGISTER_COMMAND(LoadCommand)

} // namespace vde::tools::commands
```

---

## Tuple Types — Points, Sizes, and Rectangles

### Motivation

The editor constantly works with coordinate pairs and dimension pairs. In the original system, these are passed as separate `Int` parameters (`<x1> <y1> ... <x2> <y2>`), which is:

- **Verbose** — `draw rect 0 0 to 32 32 with red` requires the parser to associate four separate ints with two logical points.
- **Fragile** — If a keyword is missing or misordered, the parser can't tell which int belongs to which logical group.
- **Unstructured** — Metadata says "four ints" but doesn't express "two points" or "a rectangle".

Tuple types let commands express their intent directly: `draw rect (0, 0) to (32, 32) with red`.

### Syntax

Tuples use parenthesized, comma-separated values:

| Type | Syntax | Examples |
|------|--------|----------|
| **Point** | `(x, y)` | `(10, 5)`, `(0, 0)` |
| **Size** | `(w, h)` | `(32, 32)`, `(128, 64)` |
| **Rect** | `((x, y), (w, h))` or `(x, y, w, h)` | `((0, 0), (32, 32))`, `(0, 0, 32, 32)` |

**Whitespace inside parentheses is optional:** `(10,5)` and `( 10 , 5 )` are both valid.

**Backward compatibility:** For commands that previously accepted bare ints (e.g., `set 10 5 #FF0000`), the parser also accepts bare `x y` pairs as a Point when the metadata declares `ParamType::Point`. The parenthesized form is preferred and shown in help, but bare pairs are accepted for script compatibility. See *Bare-pair fallback* below.

### Recursive Tokenizer

The `CommandArgParser::tokenize()` method is parenthesis-aware. When it encounters an opening `(`, it collects all characters (including nested parens and commas) until the matching `)`, and emits the entire group as a single token.

```
Input:  "(10, 5) to (20, 15) with red width 2"
Tokens: ["(10, 5)", "to", "(20, 15)", "with", "red", "width", "2"]

Input:  "((0, 0), (32, 32)) with blue"
Tokens: ["((0, 0), (32, 32))", "with", "blue"]
```

**Unmatched parentheses** produce a parse error with the position of the offending `(` or `)`.

### Tuple Parsing Rules

`CommandArgParser::parseTuple()` handles three cases:

1. **Simple pair** — `(a, b)` where `a` and `b` are integers → `IntPair{a, b}`. Used for `Point` and `Size`.

2. **Flat rect** — `(a, b, c, d)` where all four are integers → `IntRect{a, b, c, d}`. Convenient shorthand.

3. **Nested rect** — `((a, b), (c, d))` where each inner group is a pair → `IntRect{a, b, c, d}`. Structurally explicit form that mirrors the "origin + size" semantics.

The parser strips outer parentheses, then checks for nested `(` at the start:
- If found → split on `), (` boundary → parse two inner pairs → compose `IntRect`.
- If not → split on commas → 2 values = `IntPair`, 4 values = `IntRect`, else error.

### Bare-Pair Fallback

For backward compatibility with existing scripts that use bare integers:

```
set 10 5 #FF0000       ← legacy (bare ints)
set (10, 5) #FF0000    ← new (tuple)
```

When a `ParamType::Point` or `ParamType::Size` parameter is expected and the next token does **not** start with `(`, the parser consumes the next **two** tokens as integers and constructs an `IntPair`. Similarly, `ParamType::Rect` without a leading `(` consumes **four** tokens.

This fallback is transparent: the handler receives an `IntPair`/`IntRect` regardless of which syntax the user wrote. Help output shows the parenthesized form.

### Updated Command Examples

| Command | Old syntax | New syntax (preferred) |
|---------|-----------|------------------------|
| `set` | `set <x> <y> <color>` | `set (x, y) <color>` |
| `draw line` | `draw line <x1> <y1> to <x2> <y2> with <color>` | `draw line (x1, y1) to (x2, y2) with <color>` |
| `draw rect` | `draw rect <x1> <y1> to <x2> <y2> with <color>` | `draw rect (x1, y1) to (x2, y2) with <color>` |
| `draw circle` | `draw circle <cx> <cy> radius <r> with <color>` | `draw circle (cx, cy) radius <r> with <color>` |
| `crop` | `crop <x1> <y1> to <x2> <y2>` | `crop (x1, y1) to (x2, y2)` |
| `pan` | `pan <dx> <dy>` | `pan (dx, dy)` |
| `floodfill` | `floodfill <x> <y> with <color>` | `floodfill (x, y) with <color>` |
| `pick` | `pick <x> <y>` | `pick (x, y)` |
| `resize` | `resize <w> <h>` | `resize (w, h)` |
| `create canvas` | `create canvas <name> <w> <h>` | `create canvas <name> (w, h)` |
| `draw <img>` | `draw <img> [layer] <x> <y> <w> <h>` | `draw <img> [layer] (x, y) (w, h)` |

### Help Output with Tuples

```
> help draw rect
  draw rect — Draw a rectangle
  Category: Drawing
  Scope: Canvas (operates on active canvas or @target)

  Usage: draw rect (x1, y1) to (x2, y2) with <color> [filled|outline]

  Parameters:
    start       Point     Top-left corner (x, y)
    end         Point     Bottom-right corner (x, y)
    color       Color     Fill/stroke color (hex #RRGGBB[AA] or named)
    style       Enum      filled or outline (default: filled)

  Example: draw rect (0, 0) to (31, 31) with #FF0000FF filled
```

```
> help resize
  resize — Resize canvas dimensions
  Category: Canvas
  Scope: Canvas

  Usage: resize (w, h)

  Parameters:
    dimensions  Size      New canvas dimensions (w, h)

  Example: resize (64, 64)
```

### Validation Error Messages

```
> draw line (10, abc) to (20, 15) with red
  Error: Parameter 'start' — invalid Point tuple: expected (int, int), got '(10, abc)'

> draw rect ((0, 0), (32)) with blue
  Error: Parameter 'start' — invalid Rect tuple: expected ((x,y),(w,h)) or (x,y,w,h), got '((0, 0), (32))'

> crop (10 to (20, 30)
  Error: Unmatched parenthesis at position 5 — expected ')'
```

---

## Rehost and Copyhost Commands

### Motivation

Currently, objects (images, colors, areas) are permanently bound to the canvas in which they were created. The `::` accessor provides read-only cross-canvas access (`hero::face_img`), but there's no way to:

- Move an image resource from one canvas to another
- Copy a resource into a different canvas so it can be independently modified
- Promote a canvas-local resource to root scope or another canvas
- Share a color definition from one canvas to another

The **rehost** and **copyhost** commands fill this gap.

### Command Definitions

#### `rehost` — Transfer Object Ownership

Moves an object from its current host (canvas or root) to a new host. The object is removed from the source and added to the destination. Existing references to the old location (`oldcanvas::name`) become invalid.

```
rehost <type> <name> [from <source>] to <destination>
```

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `type` | Enum | Yes | Object type: `image`, `color`, `area` |
| `name` | String | Yes | Object name in the source |
| `from` | Keyword | No | Keyword separator |
| `source` | String | No | Source canvas name (defaults to active canvas) |
| `to` | Keyword | Yes | Keyword separator |
| `destination` | String | Yes | Destination canvas name, or `root` for root scope |

**Examples:**
```
# Move image "badge" from active canvas to canvas "sheet"
rehost image badge to sheet

# Move image "face" from canvas "hero" to canvas "enemies"
rehost image face from hero to enemies

# Promote color "skin" from canvas "hero" to root (global scope)
rehost color skin from hero to root
```

**Metadata:**
```cpp
static const CommandMetadata meta{
    .name = "rehost",
    .category = "Organization",
    .summary = "Transfer an object from one canvas to another",
    .description = "Moves an object (image, color, area) from its current host to a "
                   "new canvas or root scope. The object is removed from the source. "
                   "Use 'root' as destination for global scope.",
    .scope = CommandScope::Global,
    .params = {
        {.name = "type", .type = ParamType::Enum, .required = true,
         .description = "Object type", .enumValues = {"image", "color", "area"}},
        {.name = "name", .type = ParamType::String, .required = true,
         .description = "Object name in the source"},
        {.name = "from", .type = ParamType::Keyword, .required = false},
        {.name = "source", .type = ParamType::String, .required = false,
         .description = "Source canvas (defaults to active canvas)"},
        {.name = "to", .type = ParamType::Keyword, .required = true},
        {.name = "destination", .type = ParamType::String, .required = true,
         .description = "Destination canvas name, or 'root'"},
    },
    .syntaxExample = "rehost <type> <name> [from <canvas>] to <canvas|root>",
};
```

#### `copyhost` — Duplicate Object to Another Host

Copies an object from its current host to a new host. The original remains in the source. The copy can be given a new name.

```
copyhost <type> <name> [from <source>] to <destination> [as <newname>]
```

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `type` | Enum | Yes | Object type: `image`, `color`, `area` |
| `name` | String | Yes | Object name in the source |
| `from` | Keyword | No | Keyword separator |
| `source` | String | No | Source canvas name (defaults to active canvas) |
| `to` | Keyword | Yes | Keyword separator |
| `destination` | String | Yes | Destination canvas name, or `root` for root scope |
| `as` | Keyword | No | Keyword separator |
| `newname` | String | No | Name for the copy (defaults to original name) |

**Examples:**
```
# Copy image "badge" from active canvas to canvas "sheet"
copyhost image badge to sheet

# Copy with a new name
copyhost image face from hero to enemies as enemy_face

# Copy a color definition from hero to global scope
copyhost color skin from hero to root

# Copy image from one canvas to another, keeping the name
copyhost image grass_tile from tileset to level1
```

### Implementation Details

**Image rehost/copy:** The `ImageDocument` is either moved (`std::move`) or deep-copied (pixel buffer copy). Both update the canvas `resources` map.

**Color rehost/copy:** Named colors in the source canvas (or root `m_namedColors` map) are moved/copied to the destination. Root scope uses `EditorContext::namedColors`.

**Area rehost/copy:** When the area/layer system is implemented, area definitions will be transferable. Until then, the command validates that the type is supported and returns an informative error for unimplemented types.

**Validation:**
- Source object must exist
- Destination canvas must exist (or be `root`)
- Name must not conflict at destination (error on conflict; use `as` to rename)
- Cannot rehost to the same canvas (warn, no-op)

**Operation history:** Both commands are recorded in the canvas operation history of the affected canvases to preserve deterministic recreation.

---

## `draw` Command Disambiguation

The current `cmdDraw` handler uses a chain of if/else to dispatch sub-commands (`line`, `rect`, `circle`, image-blit). With the new system, compound commands use space-separated names:

| Registration Name | Dispatched As |
|---|---|
| `draw line` | `DrawLineCommand` |
| `draw rect` | `DrawRectCommand` |
| `draw circle` | `DrawCircleCommand` |
| `draw` (fallback) | `DrawImageCommand` |

The `CommandRegistry` supports compound command lookup: when resolving `"draw line 0 0 to 10 10 with red"`, it tries `"draw line"` first (longest match), then falls back to `"draw"`.

---

## Metadata-Powered UI Features

### REPL Autocomplete & Hints

When the user types in the command console:

1. **Command name completion** — `CommandRegistry::getAllMetadata()` provides the list for prefix matching.
2. **Parameter hints** — After a command name is recognized, the metadata `params` list is displayed as ghost text or a tooltip showing the next expected parameter and its type. Tuple types show `(x, y)` or `(w, h)` as placeholder shapes.
3. **Enum value completion** — For `ParamType::Enum` parameters, the `enumValues` list is offered as completions.
4. **Color completion** — For `ParamType::Color` parameters, named colors from `EditorContext::namedColors` are offered alongside the `#hex` hint.
5. **Tuple bracket matching** — When the cursor is inside a parenthesized group, the console highlights the matching open/close parenthesis. If the user types `(10, ` the hint shows the remaining expected element (e.g., `y)`).

### Enhanced Help Output

`help` auto-generates structured output from metadata:

```
> help draw line
  draw line — Draw a line between two points
  Category: Drawing
  Scope: Canvas (operates on active canvas or @target)

  Usage: draw line <x1> <y1> to <x2> <y2> with <color> [width <n>]

  Parameters:
    x1          Int       Start X coordinate
    y1          Int       Start Y coordinate
    x2          Int       End X coordinate
    y2          Int       End Y coordinate
    color       Color     Line color (hex #RRGGBB[AA] or named)
    thickness   Int       Line thickness (default: 1)

  Example: draw line 0 0 to 15 15 with #FF0000FF width 2
```

### Validation Error Messages

When parsing fails, the system reports which parameter failed and what was expected:

```
> fill blurple
  Error: Parameter 'color' — invalid color 'blurple' (expected #RRGGBB[AA] or named color)

> draw rect (0, 0) with #FF0000
  Error: Missing required parameter 'end' — expected Point (x, y) after keyword 'to'
  Usage: draw rect (x1, y1) to (x2, y2) with <color> [filled|outline]

> draw line (10, abc) to (20, 15) with red
  Error: Parameter 'start' — invalid Point tuple: expected (int, int), got '(10, abc)'
```

---

## Migration Strategy

The refactor is designed to be incremental — the old and new systems can coexist during the transition.

### Step 1: Infrastructure (no behavioral changes)

1. Create `commands/` directory structure
2. Implement `CommandMetadata.h`, `ParamDescriptor`, `ParamType`
3. Implement `CommandArgs.h/.cpp`, `ParsedArg`, `IntPair`, `IntRect`
4. Implement `CommandArgParser.h/.cpp` — including parenthesis-aware tokenizer and recursive `parseTuple()`
5. Implement `CommandBase.h` with `GlobalCommand` and `CanvasCommand` bases
6. Implement `EditorContext.h` façade
7. Implement `CommandRegistry.h/.cpp` with `REGISTER_COMMAND` macro
8. Add new files to `CMakeLists.txt`
9. Write unit tests for `CommandArgParser` with various metadata patterns — including Point/Size/Rect tuple parsing, nested tuples, bare-pair fallback, unmatched parentheses
10. Write unit tests for `CommandRegistry` (registration, lookup, compound names)

### Step 2: Dual Registration

1. Modify `CommandSystem` to check `CommandRegistry` as a fallback when the old handler maps don't match. This allows both old lambda-handlers and new `CommandBase` classes to coexist.
2. Port one simple command (`FillCommand`) to the new system as a proof-of-concept.
3. Verify that the old and new command both work identically.
4. Verify that `help fill` outputs auto-generated metadata.

### Step 3: Port Simple Commands

Port commands one at a time, removing the old handler method from `ResourceEditorScene` each time:

**Phase A — Trivial commands (header-only, few params):**
- `UndoCommand`, `RedoCommand`, `CloseCommand`
- `SetPixelCommand` (Point), `FillCommand`, `FloodFillCommand` (Point)
- `PickCommand` (Point), `ZoomCommand`, `PanCommand` (Point/Size)
- `FlipCommand`, `ResizeCommand` (Size), `CropCommand` (Point + Point)
- `SetColorCommand`, `SetToolCommand`, `SetSizeCommand`
- `ExitCommand`, `ListCommand`, `HelpCommand`

**Phase B — Medium commands (need `.cpp`, custom parsing):**
- `DrawLineCommand`, `DrawRectCommand`, `DrawCircleCommand`
- `SaveCommand`, `SaveAsCommand`, `ExportCommand`
- `SelectCommand`, `LogCommand`, `RunCommand`

**Phase C — Complex commands (multi-mode, file dialogs, resource management):**
- `CreateCommand` (dispatches `create canvas`, `create color`, `create image`)
- `LoadCommand` (file dialog flow, canvas creation)
- `DrawImageCommand` (resource resolution, blitting)

### Step 4: New Commands

1. Implement `RehostCommand` and `CopyhostCommand`
2. Add unit tests for both (image, color, area types; root scope; conflict handling)
3. Update `help` output to include the new commands

### Step 5: Remove Old System

1. Remove all `cmd*()` methods from `ResourceEditorScene`
2. Remove `registerGlobalCommands()` / `registerCanvasCommands()` methods
3. Remove the old `std::map<std::string, GlobalHandler>` / `CanvasHandler` maps from `CommandSystem`
4. `CommandSystem::execute()` now only uses `CommandRegistry`
5. `ResourceEditorScene` becomes slim: owns subsystems, calls panels, manages GPU textures

### Step 6: Enhanced UI

1. Implement parameter hint display in `EditorPanels::drawCommandConsole()`
2. Implement enum value autocomplete
3. Implement color name autocomplete
4. Update `help` to use auto-generated output from metadata

---

## Testing Plan

### Unit Tests

| Test File | Coverage |
|-----------|----------|
| `test_CommandArgs.cpp` | `CommandArgs` typed accessors, missing key handling |
| `test_CommandArgParser.cpp` | Parsing against metadata: all `ParamType` variants, optional params, keywords, defaults, error cases, tuple parsing (Point/Size/Rect), nested tuples, bare-pair fallback, unmatched parentheses |
| `test_CommandRegistry.cpp` | Registration, lookup by name/alias, compound command resolution, category filtering |
| `test_CommandBase.cpp` | `GlobalCommand` / `CanvasCommand` scope enforcement, `EditorContext` wiring |
| `test_FillCommand.cpp` | Fill with hex color, named color, missing arg, invalid canvas |
| `test_DrawLineCommand.cpp` | Line with tuple points, optional width, bare-pair fallback, bad keyword order |
| `test_RehostCommand.cpp` | Image/color rehost: success, missing source, name conflict, root scope, same-canvas warning |
| `test_CopyhostCommand.cpp` | Image/color copy: success, rename via `as`, deep copy verification, conflict handling |

### Integration Tests

- Port each existing command and verify that all existing command scripts still produce identical log output.
- Run `test_basic.txt` and `test_multi_canvas.txt` scripts against the new system and diff results.

---

## Summary of Changes to Existing Files

| File | Change |
|------|--------|
| `CommandSystem.h` | Add `CommandRegistry*` pointer, dual dispatch path, then replace old maps |
| `CommandSystem.cpp` | `execute()` delegates to registry after parser; old maps removed post-migration |
| `ResourceEditorScene.h` | Remove ~25 `cmd*()` method declarations; add `EditorContext` member |
| `ResourceEditorScene.cpp` | Remove all handler implementations + registration; slim down to ~200 lines |
| `CanvasRegistry.h` | No changes (used via `EditorContext`) |
| `EditorPanels.h/cpp` | Add parameter hint rendering in command console |
| `CMakeLists.txt` | Add all new files under `commands/` |

---

## Open Questions / Future Considerations

1. **Area objects** — The area/layer system is Phase 2/3 in the original design. `rehost` and `copyhost` should support areas once implemented.

2. **Compound command parsing depth** — Currently we only need two-word compounds (`draw line`, `draw rect`). If deeper nesting is needed (`layer add above`), the compound matcher needs to handle 3+ words. For now, `layer add` can be the compound name with `above`/`below`/`at` as params.

3. **Custom parsing escape hatch** — Some commands (like `load` with its dialog flow, or `create` with its multi-type dispatch) don't fit purely metadata-driven parsing. The `usesCustomParsing()` hook lets these commands receive the raw arg string while still providing metadata for help/hints. This should be used sparingly — most commands should rely on the standard parser.

4. **Undo integration for rehost** — Rehosting/copying could be undoable at the canvas level. Since rehost affects two canvases, this may need a transaction model (`snapshotForUndo` on both source and destination). For Phase 1, rehost/copyhost are non-undoable operations (like `close` or `new`).

5. **Root scope formalization** — Currently "root" scope for named colors is the `m_namedColors` map on the scene. With `rehost`/`copyhost` supporting `root`, this should be modeled more explicitly — perhaps a virtual "root canvas" or a separate `GlobalObjectStore` in `EditorContext`.

6. **Tuple expression evaluation** — In Phase 3 (Canvas DSL), tuples could contain expressions instead of bare integers: `(lb + 5, tb + 5)` or `(50%w, 50%h)`. The DSL executor already has an expression evaluator; the question is whether the command-level parser should also support arithmetic in tuples, or whether that remains DSL-only. For Phase 1, tuples accept only integer literals. The parser architecture (recursive `parseTuple`) is designed to allow plugging in expression evaluation later without changing the `ParamType` definitions.

7. **Named point/area references in tuples** — DSL scripts define named points (`create point p1 10, 5`) and areas. In the future, tuple parameters could accept named references: `draw line p1 to p2 with red`, where `p1` resolves to an `IntPair` from the symbol table. This would require the `parseTuple` path to check the symbol table before attempting numeric parsing. The metadata system already supports this — `ParamType::Point` parsers would check named points as a fallback after tuple literal parsing fails.
