# VDE Canvas Drawing DSL — Language Specification

> **Status:** Design  
> **Related:** [Editor Design](EDITOR_DESIGN.md) · [Parser & Command System](PARSER_AND_COMMAND_SYSTEM.md) · [Implementation Plan](IMPLEMENTATION_PLAN.md)

---

## 1. Overview

The Canvas DSL is a declarative, text-based language for procedurally creating 2D image resources in the VDE Resource Editor. A `.vdecanvas` script fully describes a canvas — its dimensions, color palette, layers, named geometry, and drawing operations — so that an image can be reproduced deterministically from source.

### Design Goals

- **Human-readable** — Terse but unambiguous, one statement per line.
- **Replayable** — Executing the same script always produces the same output.
- **Composable** — Scripts can include other scripts and reference shared palettes.
- **Editable** — Users can hand-author scripts or have them recorded from interactive editor actions.

### Relationship to the Command System

The Resource Editor's `CommandSystem` processes imperative commands at runtime. The Canvas DSL is a **higher-level authoring language** that compiles down to sequences of those low-level commands. A DSL script can be:

1. **Loaded** into the editor as a new canvas (the editor replays it to produce the image).
2. **Exported** from a canvas — the editor serializes its operation history as a DSL script.
3. **Executed** in batch mode from the command line.

All DSL operations are fully logged, undoable, and scriptable — they are not a parallel mutation path.

### Canvas Operation History

Every canvas stores the ordered sequence of commands that produced its current state. This enables:
- **Deterministic recreation** — the same history always produces the same result.
- **Script export** — the operation log can be serialized as a `.vdecanvas` DSL script.
- **Undo/redo** — operations can be rolled back by trimming the history.

---

## 2. File Format

- **Extension:** `.vdecanvas`
- **Encoding:** UTF-8
- **Syntax:** One statement per line
- Blank lines are ignored
- Line comments: `//`
- Block comments: `/* ... */`

```
// hero_sprite.vdecanvas
// A 32x32 hero sprite with a simple body and head

create canvas hero 32 32
create color skin #FFCC99FF
create color outline #333333FF
create palette hero_pal skin, outline
```

---

## 3. Unified Object Model

Every named object is created with the same syntax:

```
create <objecttype> <name> <parameters...>
```

All objects have a **name** (unique within their type namespace) and an implicit **id** (auto-assigned sequential integer).

### Object Types

| Type | Create Syntax | Description |
|------|---------------|-------------|
| `canvas` | `create canvas <name> <w> <h>` | Image canvas with pixel buffer |
| `image` | `create image <name> <canvas>[layers...] <area>` | Composited image resource |
| `color` | `create color <name> <hex>` | Named RGBA color |
| `palette` | `create palette <name> <color>, ...` | Ordered color collection |
| `point` | `create point <name> at <x>, <y>` | Named 2D coordinate |
| `area` | `create area <name> [canvas] at <x>,<y> to <x2>,<y2>` | Named rectangular region |
| `layer` | `create layer <name> [canvas] [above\|below <ref> \| at <index>]` | Canvas layer |
| `gradient` | `create gradient <name> <params>` | Reusable gradient definition |
| `pattern` | `create pattern <name> <w> <h> { ... }` | Repeating tile pattern |
| `macro` | `create macro <name>(<params>) { ... }` | Reusable command sequence |

### Object References

Objects are referenced by name wherever their type is expected:

```
create color blue #0000FFFF
draw line 0, 0 to 31, 31 with blue            // reference by color name
draw line 0, 31 to 31, 0 with #FF0000FF       // raw hex literal also allowed
```

### Object Properties (Dot Notation)

All objects expose member properties via `<objectname>.<property>`:

```
head.cx         // center x of area 'head'
mypal.count     // number of colors in palette 'mypal'
base.opacity    // opacity of layer 'base'
```

Dot notation is **reserved for member properties only**.

### Cross-Resource Access (`::`)

Resources owned by a specific canvas use the double-colon accessor:

```
<canvasname>::<resourcename>
```

Within the active canvas, the prefix is optional — bare names resolve against the active canvas first.

```
create canvas hero 32 32
load hero "assets/face.png" face_img

create canvas sheet 128 128
draw hero::face_img sheet[0] 0, 0 32, 32
```

**Resolution order** for bare names:
1. Local scope (variables, loop counters)
2. Active canvas resources (images, areas, layers)
3. Global objects (colors, palettes, points, macros)

---

## 4. Canvas

```
create canvas <name> <width> <height>
```

A canvas owns:
- A pixel buffer (composited result of all layers)
- An ordered layer stack (at least one: `"base"` at index 0)
- A collection of named image resources
- An operation history

**Optional metadata** (must appear before the first drawing operation):

```
create canvas hero 32 32
origin topleft                  // topleft (default) | bottomleft
background bg                   // Fill with color before any operations
```

### Canvas Properties

| Property | Type | Description |
|----------|------|-------------|
| `.w` | int | Width |
| `.h` | int | Height |
| `.lb`, `.rb`, `.tb`, `.bb` | int | Bounds |
| `.cx`, `.cy` | int | Center |
| `.name` | string | Canvas name |

---

## 5. Colors and Palettes

### 5.1 Colors

```
create color <name> <hex>
```

Hex value: `#RRGGBB` (alpha defaults to `FF`) or `#RRGGBBAA`.

```
create color blue #0000FFFF
create color skin #FFCC99
create color transparent #00000000
```

**Properties:** `.r`, `.g`, `.b`, `.a` (int 0–255), `.hex` (string `#RRGGBBAA`)

**Inline hex literals** are always accepted where a color is expected:
```
draw line 0, 0 to 31, 0 with #FF0000FF
```

### 5.2 Palettes

Ordered collection of color references:

```
create palette <name> <color1>, <color2>, ...
```

**Active palette:** The most recently created or selected palette. Switch with `select palette <name>`.

**Operations:**
```
<palette> add <color1>, <color2>, ...      // Add colors
<palette> drop <color>                     // Remove color
idx = <palette>[<colorname>]               // Get 0-based index
clr = <palette>[<index>]                   // Get color at index
```

**Properties:** `.count` (int), `.colors` (list)

### 5.3 Palette Files (`.vdepalette`)

Shared color/palette definitions:
```
include "shared/fantasy_colors.vdepalette"
```

A `.vdepalette` file contains only `create color` and `create palette` statements.

---

## 6. Coordinate System and Bounds

### Canvas Bounds

| Variable | Meaning | Value (32×32 canvas) |
|----------|---------|----------------------|
| `lb` | Left bound (min x) | `0` |
| `rb` | Right bound (max x) | `31` |
| `tb` | Top bound (min y) | `0` |
| `bb` | Bottom bound (max y) | `31` |
| `cx` | Center x | `15` |
| `cy` | Center y | `15` |
| `w` | Width | `32` |
| `h` | Height | `32` |

Inside an `in <area>` block, these resolve to that area's extents.

### Bound Expressions

- **Literal:** `10`, `25`
- **Bound reference:** `lb`, `rb`, `cx`, etc.
- **Bound arithmetic:** `lb+5`, `rb-5`, `cx-4`
- **Percentage:** `50%w`, `25%h`
- **Fractional:** `w/3`, `h/4`

Integer-only. Left-to-right evaluation, no operator precedence.

### Points

Two expressions separated by a comma: `lb+5, tb+10`

---

## 7. Named Points

```
create point <name> at <x>, <y>
```

**Properties:** `.x`, `.y`

**Arithmetic:**
```
create point shifted at topleft_inset + 5, 0      // x+5, y+0
create point below_center at center + 0, 8         // same x, 8 down
```

---

## 8. Named Areas

### Corner-to-Corner
```
create area <name> [canvas] at <x1>, <y1> to <x2>, <y2>
```

### Position + Size
```
create area <name> [canvas] at <x>, <y> size <w>, <h>
```

**Properties:** `.lb`, `.rb`, `.tb`, `.bb`, `.cx`, `.cy`, `.w`, `.h`

### Scoped Operations

```
in head {
    // lb, rb, tb, bb resolve to head's extents
    fill skin
    draw line lb+2, cy to rb-2, cy with outline
}
```

---

## 9. Layers

Each canvas has an ordered layer stack. Default: layer 0 named `"base"`. Composited top-to-bottom (highest index on top).

### Creating Layers

```
create layer <name> [canvas] [above|below <ref> | at <index>]
```

- `above`/`below` — position relative to current or named layer
- `at <index>` — insert at position (existing layers shift up)
- Default: above current layer

### Layer Commands

```
select layer <name|index>
<layer>.rename <name>
<layer>.remove
<layer>.hide
<layer>.show
<layer>.opacity = <0-100>
<layer>.blend = <mode>              // normal, multiply, add, screen
<layer>.order = <position>
<layer>.bounds = lb+4, tb+4 to rb-4, bb-4
```

### Layer Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `.name` | string | auto | Display name |
| `.visible` | bool | true | Visibility |
| `.opacity` | int | 100 | Opacity 0–100 |
| `.blend` | enum | normal | Blend mode |
| `.lb`, `.rb`, `.tb`, `.bb` | int | canvas bounds | Layer bounds |

---

## 10. Drawing Operations

All operations execute on the **current layer** of the **active canvas**.

### 10.1 Set Pixel

```
set <x>, <y> <color>
```

### 10.2 Fill

```
fill <color>
fill <area> <color>
```

### 10.3 Draw Line

```
draw line <p1> to <p2> with <color> [width <n>]
```

### 10.4 Draw Rectangle

```
draw rect <p1> to <p2> with <color> [filled|outline [width <n>]]
```

### 10.5 Draw Circle / Ellipse

```
draw circle <center> radius <r> with <color> [filled|outline [width <n>]]
draw ellipse <center> radius <rx>, <ry> with <color> [filled|outline [width <n>]]
```

### 10.6 Draw Arc

```
draw arc <center> radius <r> from <angle1> to <angle2> with <color> [width <n>]
```

Angles in degrees, 0 = right, counter-clockwise.

### 10.7 Flood Fill

```
floodfill <point> with <color>
```

### 10.8 Draw Bezier Curve

```
draw bezier <p1> <p2> <p3> [<p4>] with <color> [width <n>]
```

3 control points = quadratic, 4 = cubic.

### 10.9 Draw Image (Blit)

```
draw <imagename> <canvasname>[layer] <position> <size>
draw <imagename> [layer] <position> <size>
```

**Disambiguation:** `draw` dispatches based on the second token. If it's a shape keyword (`line`, `rect`, `circle`, etc.), it's a shape draw. Otherwise, it's an image blit.

```
draw face hero[0] 0, 0 32, 32
draw hero::face sheet[0] 100, 200 50, 50
```

---

## 11. Area Operations

### Copy
```
copy <area> to <x>, <y> [fliph] [flipv] [rotate <degrees>]
```

### Tile
```
tile <source_area> over <dest_area>
tile <source_area> over <x1>, <y1> to <x2>, <y2>
```

### Move
```
move <area> to <x>, <y>
```

### Clear
```
clear <area>
clear <area> with <color>
```

---

## 12. Gradients

### Linear Gradient
```
create gradient <name> vector <vx>, <vy> stops <pos> <color> [, <pos> <color>] ...
```

### Radial Gradient
```
create gradient <name> radial <center> radius <r> stops <pos> <color>, ...
```

### Gradient Fill
```
fill gradient <name>
fill <area> gradient <name>
```

---

## 13. Patterns

```
create pattern <name> <w> <h> {
    <row_data>      // color names or '.' for transparent, space-separated
    ...
}

fill pattern <name>
fill <area> pattern <name>
```

---

## 14. Image Operations

### 14.1 Load Image

```
load <canvasname> "<filepath>" [imagename]
```

- If `canvasname` doesn't exist → create new canvas with image dimensions, auto-display at (0,0).
- If `canvasname` exists → add as undisplayed resource. Use `draw` to place it.
- The load operation is recorded in the canvas's operation history.

### 14.2 Create Image (Compositing)

```
create image <name> <canvasname>[layer1, layer2, ...] <areaname>
create image <name> <canvasname> <areaname>
```

Composites specified layers within an area into a new undisplayed image resource.

### 14.3 Resize

```
resize <width> <height> [nearest|bilinear]
```

### 14.4 Crop

```
crop <x1>, <y1> to <x2>, <y2>
crop <area>
```

### 14.5 Flip / Rotate

```
flip horizontal
flip vertical
rotate <degrees>          // 90, 180, 270 only
```

---

## 15. Variables

```
<name> = <expression>
```

```
margin = 4
body_top = tb + 20
center_x = head.cx
half_w = w / 2
```

Variables are expanded at parse time.

---

## 16. Loops

### Repeat
```
repeat <n> as <var> {
    ...
}
```

Variable goes from `0` to `n-1`.

### For
```
for <var> from <start> to <end> [step <s>] {
    ...
}
```

---

## 17. Conditionals

```
if <condition> {
    ...
} else {
    ...
}
```

Conditions: `==`, `!=`, `<`, `>`, `<=`, `>=`, `and`, `or`, `not`

---

## 18. Includes and Macros

### Include
```
include "shared/draw_border.vdecanvas"
```

### Macro Definition
```
create macro <name>(<param1>, <param2>, ...) {
    ...
}
```

### Macro With Defaults
```
create macro bordered_rect(p1, p2, fill_color, border_color, border_width = 1) {
    draw rect p1 to p2 with fill_color filled
    draw rect p1 to p2 with border_color outline width border_width
}

bordered_rect(lb, tb to rb, bb, armor, outline)
```

---

## 19. Export

```
export png "output/hero_sprite.png"
export bmp "output/hero_sprite.bmp"
export tga "output/hero_sprite.tga"
```

---

## 20. Full Example

```
// hero_sprite.vdecanvas
// 32x32 hero character sprite

create canvas hero 32 32
origin topleft
background bg

// --- Colors ---
create color bg #00000000
create color skin #FFCC99
create color outline #333333
create color armor #4488CC
create color eye #000000
create color hair #8B4513
create color belt #8B6914
create color buckle #FFD700
create color boot #5C3317
create color highlight #66AADD
create color shadow #336699

// --- Palette ---
create palette hero_pal bg, skin, outline, armor, eye, hair, belt, buckle, boot, highlight, shadow

// --- Variables ---
margin = 2
head_height = 12
body_height = 12

// --- Areas ---
create area head at lb+6, tb+margin to rb-6, tb+margin+head_height
create area body at lb+8, head.bb+1 to rb-8, head.bb+body_height
create area left_leg at body.lb, body.bb+1 to body.cx-1, bb-margin
create area right_leg at body.cx, body.bb+1 to body.rb, bb-margin

// --- Layer 0: Base ---
in head {
    fill skin
    draw rect lb, tb to rb, bb with outline outline
    draw line lb, tb to rb, tb with hair width 2
    draw line lb, tb+1 to lb, tb+4 with hair
    create point left_eye at cx-3, cy
    create point right_eye at cx+3, cy
    set left_eye eye
    set right_eye eye
    draw line cx-2, cy+3 to cx+2, cy+3 with outline
}

in body {
    fill armor
    draw rect lb, tb to rb, bb with outline outline
    draw line lb, tb+1 to rb, tb+1 with highlight
    draw line lb, bb-1 to rb, bb-1 with shadow
    draw line lb, bb-3 to rb, bb-3 with belt width 2
    set cx, bb-3 buckle
}

fill left_leg boot
fill right_leg boot
draw rect left_leg.lb, left_leg.tb to left_leg.rb, left_leg.bb with outline outline
draw rect right_leg.lb, right_leg.tb to right_leg.rb, right_leg.bb with outline outline

// --- Layer 1: Armor highlight ---
create layer highlights above
highlights.opacity = 40

create gradient armor_shine vector 0, 1 stops 0.0 highlight, 0.5 #FFFFFF00, 1.0 shadow
fill body gradient armor_shine

// --- Export ---
export png "output/hero_sprite.png"
```

---

## 21. Grammar Summary (EBNF)

```ebnf
script          = { statement NEWLINE } ;
statement       = create_stmt | metadata | select_stmt | palette_op | layer_prop
                | variable_def | draw_cmd | fill_cmd | set_cmd | floodfill_cmd
                | copy_cmd | move_cmd | clear_cmd | tile_cmd
                | load_cmd | resize_cmd | crop_cmd | flip_cmd | rotate_cmd
                | control_flow | scoped_block | include | macro_call
                | export_cmd | comment ;

create_stmt     = "create" ( canvas_def | image_def | color_def | palette_def
                           | point_def | area_def | layer_def | gradient_def
                           | pattern_def | macro_def ) ;

canvas_def      = "canvas" IDENT INT INT ;
image_def       = "image" IDENT IDENT ["[" IDENT { "," IDENT } "]"] IDENT ;
color_def       = "color" IDENT COLOR_LITERAL ;
palette_def     = "palette" IDENT IDENT { "," IDENT } ;
point_def       = "point" IDENT "at" expr "," expr ;
area_def        = "area" IDENT [IDENT] "at" expr "," expr
                  ( "to" expr "," expr | "size" expr "," expr ) ;
layer_def       = "layer" IDENT [IDENT] [DIRECTION [IDENT] | "at" INT] ;
gradient_def    = "gradient" IDENT ( "vector" expr "," expr | "radial" point "radius" expr )
                  "stops" stop { "," stop } ;
pattern_def     = "pattern" IDENT INT INT "{" { pattern_row } "}" ;
macro_def       = "macro" IDENT "(" [param { "," param }] ")" "{" { statement NL } "}" ;

metadata        = "origin" ORIGIN | "background" COLOR_REF ;
select_stmt     = "select" ( "palette" IDENT | "layer" (IDENT | INT) ) ;
palette_op      = palette_ref ( "add" IDENT { "," IDENT } | "drop" IDENT ) ;
layer_prop      = IDENT "." LAYER_PROP "=" expr ;
variable_def    = IDENT "=" expr | IDENT "=" palette_ref "[" (IDENT | INT) "]" ;

draw_cmd        = "draw" ( shape_draw | image_draw ) ;
shape_draw      = "line" point "to" point "with" COLOR_REF ["width" INT]
                | "rect" point "to" point "with" COLOR_REF [FILL_MODE ["width" INT]]
                | "circle" point "radius" expr "with" COLOR_REF [FILL_MODE ["width" INT]]
                | "ellipse" point "radius" expr "," expr "with" COLOR_REF [FILL_MODE ["width" INT]]
                | "arc" point "radius" expr "from" expr "to" expr "with" COLOR_REF ["width" INT]
                | "bezier" point point point [point] "with" COLOR_REF ["width" INT] ;
image_draw      = resource_ref [IDENT] ["[" INT "]"] point point ;

fill_cmd        = "fill" [IDENT] ( COLOR_REF | "gradient" IDENT | "pattern" IDENT ) ;
set_cmd         = "set" point COLOR_REF ;
floodfill_cmd   = "floodfill" point "with" COLOR_REF ;

copy_cmd        = "copy" IDENT "to" expr "," expr { "fliph" | "flipv" | "rotate" INT } ;
move_cmd        = "move" IDENT "to" expr "," expr ;
clear_cmd       = "clear" IDENT ["with" COLOR_REF] ;
tile_cmd        = "tile" IDENT "over" ( IDENT | expr "," expr "to" expr "," expr ) ;

load_cmd        = "load" IDENT STRING [IDENT] ;
resize_cmd      = "resize" INT INT [INTERP] ;
crop_cmd        = "crop" ( expr "," expr "to" expr "," expr | IDENT ) ;
flip_cmd        = "flip" ( "horizontal" | "vertical" ) ;
rotate_cmd      = "rotate" INT ;

scoped_block    = "in" IDENT "{" { statement NEWLINE } "}" ;
repeat_loop     = "repeat" INT "as" IDENT "{" { statement NEWLINE } "}" ;
for_loop        = "for" IDENT "from" expr "to" expr ["step" expr] "{" { statement NL } "}" ;
if_stmt         = "if" condition "{" { statement NL } "}" ["else" "{" { statement NL } "}"] ;
condition       = expr COMP_OP expr { LOGIC_OP condition } ;

macro_call      = IDENT "(" [arg { "," arg }] ")" ;
include         = "include" STRING ;
export_cmd      = "export" FORMAT STRING ;
comment         = "//" TEXT | "/*" TEXT "*/" ;

point           = expr "," expr | IDENT ;
resource_ref    = IDENT | IDENT "::" IDENT ;
expr            = INT | FLOAT | IDENT | BOUND
                | IDENT "." PROPERTY
                | IDENT "::" IDENT
                | palette_ref "[" ( IDENT | INT ) "]"
                | expr ( "+" | "-" | "*" | "/" ) expr
                | INT "%" ( "w" | "h" ) ;

COLOR_REF       = IDENT | COLOR_LITERAL | palette_ref "[" INT "]" ;
COLOR_LITERAL   = "#" HEX{6} | "#" HEX{8} ;
BOUND           = "lb" | "rb" | "tb" | "bb" | "cx" | "cy" | "w" | "h" ;
FILL_MODE       = "filled" | "outline" ;
ORIGIN          = "topleft" | "bottomleft" ;
INTERP          = "nearest" | "bilinear" ;
FORMAT          = "png" | "bmp" | "tga" ;
COMP_OP         = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
LOGIC_OP        = "and" | "or" ;
```

---

## 22. Design Rationale

| Decision | Rationale |
|----------|-----------|
| Unified `create <type> <name>` | Consistent syntax — one pattern for all objects |
| Colors as first-class objects | Reusable across palettes, redefinable, palette-independent |
| Palettes as ordered collections | Group/order for indexed access without gating usage |
| `=` assignment (no `let`) | Terse and familiar |
| Bound variables (lb, rb, etc.) | Scripts adapt to canvas resizes automatically |
| Named points and areas | Reduce magic numbers, enable area-scoped ops |
| `in <area>` scoping | Draw relative to sub-region without manual offset math |
| Dot notation for properties | Uniform state access, reserved exclusively for members |
| `::` for cross-canvas access | Clearly separated from dot notation (property vs resource) |
| Canvas operation history | Enables deterministic recreation, script export, undo/redo |
| No operator precedence | Keeps grammar simple; use variables for complex math |
| Macros over functions | Text-expansion is simpler — no stack frames needed |
| `.vdecanvas` extension | Distinguishes from `.vdescript` (input automation) and `.vdepalette` |
| Two-pass parse/execute | Better error reporting, forward reference support, validation before mutation |
