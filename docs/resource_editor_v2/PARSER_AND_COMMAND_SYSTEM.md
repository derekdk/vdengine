# VDE Resource Editor v2 — Parser & Command System Design

> **Status:** Design  
> **Supersedes:** [v1 Command System Refactor Plan](../history/resource_editor_v1/COMMAND_SYSTEM_REFACTOR_PLAN.md)  
> **Related:** [Editor Design](EDITOR_DESIGN.md) · [Canvas DSL](CANVAS_DSL.md) · [Implementation Plan](IMPLEMENTATION_PLAN.md)

---

## 1. Overview

This document specifies the metadata-driven command system and the Canvas DSL parser that form the backbone of the Resource Editor v2. The v1 system suffered from:

- All handlers as methods on a god-class scene (~25+ methods, 1400+ lines)
- No structured metadata — freeform help strings, no machine-readable parameter info
- Ad-hoc `istringstream` parsing per handler with inconsistent patterns
- No parameter validation before dispatch

The v2 system introduces:

1. **One command = one file** — Self-contained class with structured metadata
2. **Typed parameter metadata** — Powers validation, auto-help, autocomplete, and tooltips
3. **Uniform argument parsing** — Shared `CommandArgParser` produces typed argument maps
4. **Tuple types** — Points `(x, y)`, Sizes `(w, h)`, Rects `((x,y),(w,h))` as first-class parameter types
5. **Two-pass DSL processing** — Parser builds AST, Executor walks it emitting commands

---

## 2. Command System Architecture

### Class Hierarchy

```
CommandBase                   (abstract — metadata + execute interface)
├── GlobalCommand             (not canvas-scoped; receives parsed args)
└── CanvasCommand             (canvas-scoped; receives canvasId + parsed args)
```

Virtual dispatch is used because commands are registered in a polymorphic registry. CRTP was considered and rejected.

### Key Types

```cpp
namespace vde::tools {

// ─── Parameter Types ───

enum class ParamType {
    Int,           // Single integer (coordinate, count)
    Float,         // Float (zoom level, opacity 0.0-1.0)
    String,        // Unquoted token (canvas name, tool name)
    QuotedString,  // Quoted string ("assets/hero.png")
    Color,         // #RRGGBB or #RRGGBBAA hex, or named color
    Bool,          // true/false, filled/outline, show/hide
    Keyword,       // Fixed separator (e.g., "to", "with", "as")
    Enum,          // One of a fixed set of string values
    Point,         // 2D coordinate pair: (x, y)
    Size,          // 2D dimensions: (w, h)
    Rect,          // Rectangle: ((x, y), (w, h)) or (x, y, w, h)
};

struct ParamDescriptor {
    std::string name;
    ParamType type = ParamType::String;
    bool required = true;
    std::string description;
    std::string defaultValue;
    std::vector<std::string> enumValues;  // For Enum type
};

// ─── Parsed Arguments ───

struct IntPair {
    int x = 0, y = 0;
};

struct IntRect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct ParsedArg {
    std::string raw;
    ParamType type;

    int asInt() const;
    float asFloat() const;
    bool asBool() const;
    const std::string& asString() const;
    RGBAColor asColor() const;
    IntPair asPoint() const;
    IntPair asSize() const;
    IntRect asRect() const;

private:
    friend class CommandArgParser;
    IntPair m_pair;
    IntRect m_rect;
};

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
    const std::string& remainder() const;

private:
    friend class CommandArgParser;
    std::map<std::string, ParsedArg> m_args;
    std::string m_remainder;
};

// ─── Command Metadata ───

enum class CommandScope {
    Global,   // Not canvas-scoped (help, list, exit, etc.)
    Canvas,   // Operates on a canvas (paint, fill, undo, etc.)
};

struct CommandMetadata {
    std::string name;                    // Primary name ("fill", "draw line")
    std::vector<std::string> aliases;    // Optional aliases ("line" for "draw line")
    std::string category;                // For help grouping: "File", "Drawing", "View"
    std::string summary;                 // One-line summary
    std::string description;             // Detailed multi-line
    CommandScope scope = CommandScope::Global;
    std::vector<ParamDescriptor> params;
    std::string syntaxExample;

    std::string formatHelp() const;
    std::string formatUsage() const;
};

// ─── Command Base ───

struct CommandResult {
    bool success = true;
    std::string message;
};

class CommandBase {
public:
    virtual ~CommandBase() = default;
    virtual const CommandMetadata& metadata() const = 0;
    virtual CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                                  class EditorContext& ctx) = 0;
    virtual bool usesCustomParsing() const { return false; }
};

class GlobalCommand : public CommandBase {
public:
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) final;
protected:
    virtual CommandResult executeGlobal(const CommandArgs& args) = 0;
};

class CanvasCommand : public CommandBase {
public:
    CommandResult execute(uint32_t canvasId, const CommandArgs& args,
                          EditorContext& ctx) final;
protected:
    virtual CommandResult executeCanvas(Canvas& canvas, const CommandArgs& args,
                                        EditorContext& ctx) = 0;
};

} // namespace vde::tools
```

---

## 3. EditorContext — Shared State Façade

All shared state is exposed through a single façade struct. Commands never reference the scene class directly.

```cpp
struct EditorContext {
    CanvasRegistry& canvases;
    CommandSystem& commands;
    ToolPalette& palette;
    std::map<std::string, RGBAColor>& namedColors;

    Canvas* getActiveCanvas();
    bool resolveColor(const std::string& token, RGBAColor& out) const;

    vde::api::Game* game = nullptr;  // For exit command
};
```

---

## 4. CommandArgParser — Metadata-Driven Parsing

```cpp
class CommandArgParser {
public:
    struct ParseResult {
        bool success = false;
        CommandArgs args;
        std::string error;
    };

    static ParseResult parse(const std::string& argsString,
                             const std::vector<ParamDescriptor>& params,
                             const EditorContext& ctx);

private:
    /// Parenthesis-aware tokenizer.
    /// "(10, 5) to (20, 15) with red" → ["(10, 5)", "to", "(20, 15)", "with", "red"]
    static std::vector<std::string> tokenize(const std::string& input);

    static bool parseToken(const std::string& token, ParamType type, ParsedArg& out);
    static bool parseTuple(const std::string& token, ParsedArg& out, ParamType expected);
    static bool validateEnum(const std::string& value, const std::vector<std::string>& allowed);
};
```

### Tuple Parsing

Tuples use parenthesized, comma-separated values:

| Type | Syntax | Examples |
|------|--------|---------|
| Point | `(x, y)` | `(10, 5)`, `(0, 0)` |
| Size | `(w, h)` | `(32, 32)`, `(128, 64)` |
| Rect | `((x, y), (w, h))` or `(x, y, w, h)` | `((0, 0), (32, 32))` |

**Whitespace inside parentheses is optional.** `(10,5)` and `( 10 , 5 )` are both valid.

**Bare-pair fallback:** For backward compatibility, when `ParamType::Point` is expected and the next token doesn't start with `(`, the parser consumes two bare integers as an `IntPair`. Similarly for `Size` (two tokens) and `Rect` (four tokens).

### Recursive Tokenizer

The tokenizer is parenthesis-aware: when it encounters `(`, it collects everything up to the matching `)` as a single token (including nested parens).

```
Input:  "(10, 5) to (20, 15) with red"
Tokens: ["(10, 5)", "to", "(20, 15)", "with", "red"]

Input:  "((0, 0), (32, 32)) with blue"
Tokens: ["((0, 0), (32, 32))", "with", "blue"]
```

Unmatched parentheses produce a parse error.

### Tuple Parsing Rules

1. **Simple pair** — `(a, b)` → `IntPair{a, b}` (for Point and Size)
2. **Flat rect** — `(a, b, c, d)` → `IntRect{a, b, c, d}`
3. **Nested rect** — `((a, b), (c, d))` → `IntRect{a, b, c, d}`

---

## 5. CommandRegistry & Auto-Registration

```cpp
class CommandRegistry {
public:
    static CommandRegistry& instance();

    void registerCommand(std::unique_ptr<CommandBase> cmd);
    CommandBase* find(const std::string& name) const;
    std::vector<const CommandMetadata*> getAllMetadata() const;
    std::vector<const CommandMetadata*> getByCategory(const std::string& category) const;

private:
    std::map<std::string, std::unique_ptr<CommandBase>> m_commands;
    std::map<std::string, CommandBase*> m_aliasIndex;
};

/// Self-registration macro. Place in the .h or .cpp of each command.
#define REGISTER_COMMAND(CommandClass)                                    \
    static struct CommandClass##_Registrar {                              \
        CommandClass##_Registrar() {                                     \
            CommandRegistry::instance().registerCommand(                  \
                std::make_unique<CommandClass>());                        \
        }                                                                \
    } s_##CommandClass##_registrar;
```

**Compound command lookup:** For commands like `"draw line ..."`, the registry tries `"draw line"` first (longest match), then falls back to `"draw"` (shortest).

---

## 6. CommandSystem — Thin Dispatch Layer

The `CommandSystem` delegates parsing and dispatch to `CommandRegistry` and `CommandArgParser`:

```cpp
class CommandSystem {
public:
    void initialize(CommandRegistry& registry, EditorContext& ctx);
    bool execute(const std::string& commandLine);
    bool executeScript(const std::string& filePath);
    bool saveLogRange(size_t start, size_t end, const std::string& filePath);
    bool saveFullLog(const std::string& filePath);
    const std::vector<CommandLogEntry>& getLog() const;
    void clear();

    void setActiveCanvasId(uint32_t id);
    uint32_t getActiveCanvasId() const;

private:
    CommandRegistry* m_registry = nullptr;
    EditorContext* m_ctx = nullptr;
    std::vector<CommandLogEntry> m_log;
    uint32_t m_activeCanvasId = 0;
};
```

**Execution flow:**

```
commandLine
    → resolveCommand() extracts @canvas prefix, command name, raw args
    → CommandRegistry::find(name) returns CommandBase*
    → CommandArgParser::parse(rawArgs, cmd->metadata().params, ctx)
        → validates types, resolves keywords, fills CommandArgs map
        → returns error if validation fails (before handler runs)
    → cmd->execute(canvasId, parsedArgs, ctx)
        → handler operates on typed, pre-validated data
        → returns CommandResult
    → log entry recorded
```

---

## 7. Example Command Implementations

### Simple Canvas Command: FillCommand

```cpp
// commands/canvas/FillCommand.h
#pragma once
#include "../CommandBase.h"

namespace vde::tools::commands {

class FillCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "fill",
            .category = "Drawing",
            .summary = "Fill entire canvas with a color",
            .description = "Fills all pixels with the specified color.",
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

### Tuple Points: DrawLineCommand

```cpp
// commands/canvas/DrawLineCommand.h
#pragma once
#include "../CommandBase.h"

namespace vde::tools::commands {

class DrawLineCommand final : public CanvasCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "draw line",
            .aliases = {"line"},
            .category = "Drawing",
            .summary = "Draw a line between two points",
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

### Complex Global Command: LoadCommand

```cpp
// commands/global/LoadCommand.h
#pragma once
#include "../CommandBase.h"

namespace vde::tools::commands {

class LoadCommand final : public GlobalCommand {
public:
    const CommandMetadata& metadata() const override {
        static const CommandMetadata meta{
            .name = "load",
            .category = "File",
            .summary = "Load an image file into a canvas",
            .description =
                "Load an image file as a named resource in a canvas. "
                "If the canvas doesn't exist, creates one sized to the image.",
            .scope = CommandScope::Global,
            .params = {
                {.name = "canvasname", .type = ParamType::String, .required = false,
                 .description = "Target canvas name"},
                {.name = "filepath", .type = ParamType::QuotedString, .required = false,
                 .description = "Path to image file"},
                {.name = "imagename", .type = ParamType::String, .required = false,
                 .description = "Resource name (defaults to filename stem)"},
            },
            .syntaxExample = "load <canvas> \"<filepath>\" [imagename]",
        };
        return meta;
    }

    bool usesCustomParsing() const override { return true; }

protected:
    CommandResult executeGlobal(const CommandArgs& args, EditorContext& ctx) override;
    // Implementation in LoadCommand.cpp — complex dialog flow
};

REGISTER_COMMAND(LoadCommand)

} // namespace vde::tools::commands
```

---

## 8. Rehost & Copyhost Commands

### Rehost — Transfer Object Ownership

```
rehost <type> <name> [from <source>] to <destination>
```

Moves an object from one canvas to another. Source reference becomes invalid.

| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | Enum | `image`, `color`, `area` |
| `name` | String | Object name in source |
| `source` | String (optional) | Source canvas (defaults to active) |
| `destination` | String | Target canvas or `root` |

### Copyhost — Duplicate Object

```
copyhost <type> <name> [from <source>] to <destination> [as <newname>]
```

Copies an object. Original remains. Copy can be renamed.

**Validation:** Source must exist, destination must exist, no name conflicts (use `as` to rename).

---

## 9. Canvas DSL Parser Architecture

The DSL parser bridges `.vdecanvas` scripts with the command system. It's a **two-pass** system:

### Pass 1: Parse & Validate

Tokenize each line, build an AST, resolve includes, validate references.

```cpp
namespace vde::tools::dsl {

enum class ObjectType {
    Canvas, Image, Color, Palette, Point, Area, Layer, Gradient, Pattern, Macro
};

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

struct ASTNode {
    NodeType type;
    size_t lineNumber;
    std::string sourceFile;
    std::vector<std::string> tokens;
    std::vector<std::unique_ptr<ASTNode>> children;
};

struct Symbol {
    ObjectType objectType;
    std::string name;
    std::map<std::string, std::string> properties;
};

struct BoundScope {
    int lb, rb, tb, bb;
    int cx, cy;
    int w, h;
};

struct ParseResult {
    bool success = false;
    std::vector<std::unique_ptr<ASTNode>> statements;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class CanvasDSLParser {
public:
    ParseResult parse(const std::string& filePath);
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
    std::set<std::string> m_includedFiles;  // Circular include prevention
};

} // namespace vde::tools::dsl
```

### Pass 2: Evaluate & Execute

Walk the AST, evaluate expressions against bound context, emit `CommandSystem` calls.

```cpp
namespace vde::tools::dsl {

class CanvasDSLExprEval {
public:
    int evaluate(const std::string& expr);
    void pushScope(const BoundScope& scope);
    void popScope();
    void setVariable(const std::string& name, int value);

private:
    std::vector<BoundScope> m_scopeStack;
    std::map<std::string, int> m_variables;
    std::map<std::string, Symbol>* m_symbols = nullptr;
};

class CanvasDSLExecutor {
public:
    struct ExecuteResult {
        bool success = false;
        size_t commandsEmitted = 0;
        std::vector<std::string> errors;
    };

    ExecuteResult execute(const ParseResult& parsed, CommandSystem& cmd);

private:
    void executeNode(const ASTNode& node, CommandSystem& cmd);
    int evaluateExpr(const std::string& expr);
    std::string resolveColor(const std::string& ref);

    CanvasDSLExprEval m_exprEval;
    std::map<std::string, Symbol> m_symbols;
};

} // namespace vde::tools::dsl
```

---

## 10. DSL-to-Command Mapping

| DSL Statement | Command(s) Emitted |
|---|---|
| `create canvas hero 32 32` | `create canvas hero (32, 32)` |
| `create color skin #FFCC99` | *(symbol table only — no command)* |
| `create palette mypal ...` | *(symbol table only — no command)* |
| `load hero "face.png" face` | `load hero "face.png" face` |
| `create image torso hero[base] body` | `create image torso hero[base] body` |
| `background bg` | `fill #00000000` (resolved) |
| `set 10, 5 skin` | `set (10, 5) #FFCC99FF` |
| `fill body armor` | `fill` within area bounds |
| `draw line p1 to p2 with c` | `draw line (x1,y1) to (x2,y2) with #color` |
| `draw rect p1 to p2 with c filled` | `draw rect (x,y) to (x2,y2) with #color filled` |
| `draw circle cx, cy radius r with c` | `draw circle (cx,cy) radius r with #color filled` |
| `draw face hero[0] 0, 0 32, 32` | `draw face [0] (0,0) (32,32)` |
| `fill gradient sky` | Per-pixel `set` commands (or bulk gradient op) |
| `create layer highlights above` | `layer add highlights above` |
| `select layer highlights` | `layer select highlights` |
| `export png "output/hero.png"` | `export output/hero.png png` |

---

## 11. Error Reporting

### Command System Errors

```
> fill blurple
  Error: Parameter 'color' — invalid color 'blurple' (expected #RRGGBB[AA] or named color)

> draw rect (0, 0) with #FF0000
  Error: Missing required parameter 'end' — expected Point (x, y) after keyword 'to'
  Usage: draw rect (x1, y1) to (x2, y2) with <color> [filled|outline]

> draw line (10, abc) to (20, 15) with red
  Error: Parameter 'start' — invalid Point tuple: expected (int, int), got '(10, abc)'

> crop (10 to (20, 30)
  Error: Unmatched parenthesis at position 5 — expected ')'
```

### DSL Parser Errors

```
hero_sprite.vdecanvas:14: error: undefined color 'armorr' (did you mean 'armor'?)
hero_sprite.vdecanvas:22: error: area 'torso' used before definition
hero_sprite.vdecanvas:35: error: duplicate name 'body' — an area with this name already exists
```

Parse errors fail-fast before any commands are emitted.

---

## 12. Metadata-Powered UI Features

### REPL Autocomplete & Hints

1. **Command name completion** — Prefix matching from `CommandRegistry::getAllMetadata()`
2. **Parameter hints** — After command recognized, show ghost text for next expected param + type
3. **Enum value completion** — Offer `enumValues` for `ParamType::Enum` params
4. **Color name completion** — Offer named colors from `EditorContext::namedColors`
5. **Tuple bracket matching** — Highlight matching parentheses during input

### Auto-Generated Help

```
> help draw line
  draw line — Draw a line between two points
  Category: Drawing
  Scope: Canvas (operates on active canvas or @target)

  Usage: draw line (x1, y1) to (x2, y2) with <color> [width <n>]

  Parameters:
    start       Point     Start point (x, y)
    end         Point     End point (x, y)
    color       Color     Line color (hex #RRGGBB[AA] or named)
    thickness   Int       Line thickness (default: 1)

  Example: draw line (0, 0) to (15, 15) with #FF0000FF width 2
```

---

## 13. Command Syntax Evolution

Commands support both legacy bare-int syntax and the new tuple syntax:

| Command | Legacy | New (preferred) |
|---------|--------|-----------------|
| `set` | `set <x> <y> <color>` | `set (x, y) <color>` |
| `draw line` | `draw line <x1> <y1> to <x2> <y2>` | `draw line (x1, y1) to (x2, y2)` |
| `resize` | `resize <w> <h>` | `resize (w, h)` |
| `draw <img>` | `draw <img> [l] <x> <y> <w> <h>` | `draw <img> [l] (x, y) (w, h)` |

The bare-pair fallback is transparent to handlers — they always receive `IntPair`/`IntRect`.

---

## 14. `draw` Command Disambiguation

Compound commands use space-separated names:

| Registration | Handler |
|---|---|
| `draw line` | `DrawLineCommand` |
| `draw rect` | `DrawRectCommand` |
| `draw circle` | `DrawCircleCommand` |
| `draw ellipse` | `DrawEllipseCommand` |
| `draw arc` | `DrawArcCommand` |
| `draw bezier` | `DrawBezierCommand` |
| `draw` (fallback) | `DrawImageCommand` |

The registry tries the longest match first (`"draw line"` before `"draw"`).

---

## 15. DSL File Organization

```
tools/resource_editor/dsl/
    CanvasDSLTypes.h           # AST node types, symbol table, scope
    CanvasDSLParser.h/.cpp     # Tokenizer and AST builder
    CanvasDSLExprEval.h/.cpp   # Expression evaluator with bound context
    CanvasDSLExecutor.h/.cpp   # AST walker that emits CommandSystem calls
```

---

## 16. Integration Points

### Loading DSL into the Editor

The `dsl_load` command:
1. Parses a `.vdecanvas` file → AST with validation
2. Executor walks AST, emitting commands through `CommandSystem`
3. All emitted commands appear in the log
4. The resulting canvas is editable like any other

### Exporting Canvas as DSL

The `dsl_export` command reconstructs a `.vdecanvas` script by mapping the canvas's operation history back to DSL statements (best-effort reconstruction).

### Batch DSL Execution

```bash
vde_resource_editor.exe --dsl hero_sprite.vdecanvas
```

Parses, executes, writes output via `export` directives, exits with code 0 on success.

### REPL DSL Snippets

```
> dsl create color red #FF0000FF
  [dsl] Registered color 'red'
> dsl draw circle 8, 8 radius 5 with red filled
  [test] draw circle (8,8) radius 5 with #FF0000FF filled
```

---

## 17. Open Considerations

1. **Area rehost/copyhost** — Deferred until the area/layer system is fully implemented.

2. **Expression evaluation in tuples** — Phase 1 tuples accept only integer literals. The architecture supports plugging in expression evaluation later.

3. **Named point/area references in tuples** — Future: `draw line p1 to p2 with red` where `p1` resolves from the DSL symbol table. Requires `parseTuple` to check the symbol table as a fallback.

4. **Undo for rehost** — Rehost affects two canvases. For Phase 1, rehost/copyhost are non-undoable. A transaction model may be added later.

5. **Root scope formalization** — Named colors currently live in `m_namedColors` on the scene. With rehost supporting `root`, this should be modeled as a `GlobalObjectStore` in `EditorContext`.

6. **Compound command depth** — Currently two-word compounds suffice (`draw line`, `layer add`). If deeper nesting is needed later, the compound matcher supports 3+ words.
