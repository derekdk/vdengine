# VDE Canvas Drawing DSL — Design

## Overview

A declarative, text-based language for procedurally creating 2D image resources in the VDE Resource Editor. A `.vdecanvas` script fully describes a canvas — its dimensions, color palette, layers, named geometry, and drawing operations — so that an image can be reproduced deterministically from source.

Scripts are intended to be:
- **Human-readable** — terse but unambiguous, one statement per line.
- **Replayable** — executing the same script always produces the same output.
- **Composable** — scripts can include other scripts and reference shared palettes.
- **Editable** — users can hand-author scripts or have them recorded from interactive editor actions.

### Relationship to the Command System

The Resource Editor's `CommandSystem` processes imperative commands at runtime (`paint 10 5 #FF0000FF`, `fill #000000FF`, etc.). The Canvas DSL is a **higher-level authoring language** that compiles down to sequences of those low-level commands. A DSL script can be:

1. **Loaded** into the editor as a new canvas (the editor replays it to produce the image).
2. **Exported** from a canvas — the editor serializes its operation history as a DSL script.
3. **Executed** in batch mode from the command line.

---

## File Format

- Extension: `.vdecanvas`
- Encoding: UTF-8
- One statement per line
- Blank lines are ignored
- Comments start with `//` (line comment) or `/* ... */` (block comment)

```
// hero_sprite.vdecanvas
// A 32x32 hero sprite with a simple body and head

create canvas hero 32 32
create color skin #FFCC99FF
create color outline #333333FF
create palette hero_pal skin, outline
...
```

---

## 1. Unified Object Model

Every named object in the DSL is created with the same syntax:

```
create <objecttype> <name> <parameters...>
```

All objects have a **name** (unique within their type namespace) and an implicit **id** (auto-assigned sequential integer). Objects are referenced by name in subsequent statements.

### Object Types

| Type | Create Syntax | Description |
|------|---------------|-------------|
| `canvas` | `create canvas <name> <w> <h>` | Image canvas with pixel buffer |
| `color` | `create color <name> <hex>` | Named RGBA color value |
| `palette` | `create palette <name> <color>, ...` | Ordered collection of color references |
| `point` | `create point <name> at <x>, <y>` | Named 2D coordinate |
| `area` | `create area <name> at <x1>,<y1> to <x2>,<y2>` | Named rectangular region |
| `layer` | `create layer <name> [above\|below] [<ref>]` | Canvas layer in the compositing stack |
| `gradient` | `create gradient <name> <params>` | Reusable gradient definition |
| `pattern` | `create pattern <name> <w> <h> { ... }` | Repeating tile pattern |
| `macro` | `create macro <name>(<params>) { ... }` | Reusable command sequence |

### Object References

Objects are referenced by name wherever their type is expected. For example, a color name can appear anywhere a color is expected in a draw command:

```
create color blue #0000FFFF
draw line 0, 0 to 31, 31 with blue            // reference by color name
draw line 0, 31 to 31, 0 with #FF0000FF       // raw hex literal is also allowed
```

### Object Properties

All objects expose properties via dot notation:

```
<objectname>.<property>
```

```
head.cx         // center x of area 'head'
mypal.count     // number of colors in palette 'mypal'
base.opacity    // opacity of layer 'base'
```

---

## 2. Canvas

Every script begins with a canvas declaration.

```
create canvas <name> <width> <height>
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `name`    | ident | Unique canvas name (also used as default filename stem) |
| `width`   | int  | Width in pixels |
| `height`  | int  | Height in pixels |

**Optional metadata directives** (must appear before the first drawing operation):

```
create canvas hero 32 32
origin topleft                  // Coordinate origin: topleft (default) | bottomleft
background bg                   // Fill with color 'bg' before any operations
```

### Canvas Properties

| Property | Type | Description |
|----------|------|-------------|
| `<canvas>.w` | int | Width |
| `<canvas>.h` | int | Height |
| `<canvas>.lb`, `.rb`, `.tb`, `.bb` | int | Bounds (see §4) |
| `<canvas>.cx`, `.cy` | int | Center |
| `<canvas>.name` | string | Canvas name |

---

## 3. Colors and Palettes

### 3.1 Colors

Colors are first-class objects. Create them individually, then reference them by name in drawing commands, palette definitions, and anywhere a color is expected.

```
create color <name> <hex>
```

The hex value is `#RRGGBB` (alpha defaults to `FF`) or `#RRGGBBAA`.

```
create color blue #0000FFFF
create color green #00FF00FF
create color red #FF0000FF
create color skin #FFCC99
create color transparent #00000000
```

#### Color Properties

| Property | Type | Description |
|----------|------|-------------|
| `<color>.r` | int | Red channel (0–255) |
| `<color>.g` | int | Green channel (0–255) |
| `<color>.b` | int | Blue channel (0–255) |
| `<color>.a` | int | Alpha channel (0–255) |
| `<color>.hex` | string | `#RRGGBBAA` representation |

```
create color armor #4488CC
r = armor.r      // 68
```

#### Inline Color Literals

Where a color reference is expected, a raw `#RRGGBBAA` hex literal can always be used instead of a named color:

```
draw line 0, 0 to 31, 0 with #FF0000FF    // no need to create a named color
```

### 3.2 Palettes

A palette is an **ordered collection** of color references. Colors must already be defined (or defined inline) before being added to a palette.

```
create palette <name> <color1>, <color2>, ...
```

```
create color blue #0000FFFF
create color green #00FF00FF
create color red #FF0000FF
create palette mypal blue, green, red
```

#### Active Palette

The most recently created or selected palette is the **active palette**. The keyword `palette` in operations refers to the active palette. You can switch the active palette:

```
select palette mypal
```

#### Palette Operations

**Add colors:**

```
<palette_name> add <color1>, <color2>, ...
palette add <color1>, <color2>, ...           // active palette shorthand
```

```
create color purple #800080FF
create color yellow #FFFF00FF
mypal add purple, yellow
```

**Remove colors:**

```
<palette_name> drop <color>
palette drop <color>                          // active palette shorthand
```

```
mypal drop blue                // removes 'blue' from mypal; the color object still exists
```

**Index lookup:**

Get the 0-based index of a color within a palette:

```
<varname> = <palette_name>[<color_name>]
<varname> = palette[<color_name>]             // active palette shorthand
```

```
blueindex = mypal[blue]       // 0 (before drop), runtime error if not in palette
greenindex = palette[green]   // uses active palette
```

**Index access:**

Get the color at a given index:

```
<varname> = <palette_name>[<index>]
```

```
first = mypal[0]              // returns the color name at index 0
```

#### Palette Properties

| Property | Type | Description |
|----------|------|-------------|
| `<palette>.count` | int | Number of colors in the palette |
| `<palette>.colors` | list | Ordered list of color names |

```
n = mypal.count               // 4 (after adding purple & yellow, dropping blue)
```

#### Using Palette Colors in Drawing

Drawing operations reference colors by their **color name**, not by palette membership. Palettes organize colors but don't gate access:

```
create color outline #333333
create color armor #4488CC
// These colors are usable immediately, even without a palette:
draw line 0, 0 to 31, 31 with outline

// Palettes are useful for indexed access and organization:
create palette combat_colors outline, armor
draw rect 2, 2 to 10, 10 with combat_colors[0] filled   // same as 'outline'
```

### 3.3 Palette Files

To share color and palette definitions across scripts, use `.vdepalette` files:

```
include "shared/fantasy_colors.vdepalette"
```

A `.vdepalette` file contains `create color` and `create palette` statements:

```
// fantasy_colors.vdepalette
create color skin #FFCC99
create color outline #333333
create color armor #4488CC
create palette fantasy skin, outline, armor
```

---

## 4. Coordinate System and Bounds

### Canvas Bounds

Every canvas (and every layer) has four implicit bound variables:

| Variable | Meaning | Value (for a 32x32 canvas) |
|----------|---------|----------------------------|
| `lb` | Left bound (min x) | `0` |
| `rb` | Right bound (max x) | `31` |
| `tb` | Top bound (min y) | `0` |
| `bb` | Bottom bound (max y) | `31` |
| `cx` | Center x | `15` |
| `cy` | Center y | `15` |
| `w`  | Width | `32` |
| `h`  | Height | `32` |

When operating inside a **named area**, the bounds resolve to that area's extents instead of the full canvas.

### Bound Expressions

Coordinates can be specified as:

- **Literal**: `10`, `25`
- **Bound reference**: `lb`, `rb`, `tb`, `bb`, `cx`, `cy`
- **Bound arithmetic**: `lb+5`, `rb-5`, `tb+10`, `bb-10`, `cx-4`, `cy+4`
- **Percentage of dimension**: `50%w`, `25%h` (50% of width, 25% of height)
- **Fractional**: `w/3`, `h/4` (integer division)

Arithmetic is integer-only. Expressions are evaluated left-to-right with no operator precedence (parentheses are not supported — keep expressions simple).

### Points

A point is written as `x, y` — two expressions separated by a comma:

```
lb+5, tb+10
cx, cy
rb-1, bb-1
```

---

## 5. Named Points

Give a name to a computed coordinate so it can be reused. Named points exist for the duration of the script and can be overwritten.

```
create point <name> at <x>, <y>
```

Examples:

```
create point topleft_inset at lb+10, tb+10
create point topright_inset at rb-10, tb+10
create point center at cx, cy
```

Named points are referenced by name wherever a point is expected:

```
draw line topleft_inset to topright_inset with outline
```

### Point Properties

| Property | Type | Description |
|----------|------|-------------|
| `<point>.x` | int | X coordinate |
| `<point>.y` | int | Y coordinate |

```
create point origin at lb+5, tb+5
ox = origin.x    // 5
```

### Point Arithmetic

Named points can be offset:

```
create point shifted at topleft_inset + 5, 0    // x+5, y+0
```

Or defined relative to another point:

```
create point below_center at center + 0, 8      // same x, 8 pixels down
```

---

## 6. Named Areas (Rectangles)

Define a named rectangular region. Like points, areas persist for the script duration and can be reused.

```
create area <name> at <x1>, <y1> to <x2>, <y2>
```

The two corners are the top-left and bottom-right (inclusive).

```
create area body at lb+10, tb+20 to rb-10, bb-10
create area head at lb+8, tb+2 to rb-8, tb+18
```

### Area Properties

Named areas expose their own bounds, accessible with dot notation:

| Property | Meaning |
|----------|---------|
| `<area>.lb` | Left bound (x1) |
| `<area>.rb` | Right bound (x2) |
| `<area>.tb` | Top bound (y1) |
| `<area>.bb` | Bottom bound (y2) |
| `<area>.cx` | Center x |
| `<area>.cy` | Center y |
| `<area>.w`  | Width |
| `<area>.h`  | Height |

```
create area body at lb+10, tb+20 to rb-10, bb-10
create point body_center at body.cx, body.cy
draw line body.lb, body.cy to body.rb, body.cy with outline   // horizontal line through body center
```

### Operations Scoped to an Area

When a drawing operation is prefixed with `in <area>`, the bound variables (`lb`, `rb`, `tb`, `bb`, `cx`, `cy`, `w`, `h`) resolve to that area's extents instead of the full canvas:

```
create area head at lb+8, tb+2 to rb-8, tb+18
in head {
    // lb=8, rb=23, tb=2, bb=18 inside this block
    fill skin
    draw line lb+2, cy to rb-2, cy with outline
    create point left_eye at cx-3, cy-2
    create point right_eye at cx+3, cy-2
    set left_eye eye
    set right_eye eye
}
```

---

## 7. Layers

### Layer Model

Each canvas has an ordered stack of layers. By default, a canvas starts with **layer 0** (named `"base"`). Layers are composited top-to-bottom (highest index on top) to produce the final image.

### Creating Layers

```
create layer <name> [above|below] [<ref_layer>]
```

- `above` / `below` — position relative to the current layer (default) or relative to `<ref_layer>`.
- If omitted, the layer is added above the current layer.

```
create layer outline_layer above
create layer background below base
create layer effects above outline_layer
```

### Layer Commands

```
select layer <name|index>        // Switch to a layer by name or index
<layer>.rename <name>            // Rename a layer
<layer>.remove                   // Remove a layer
<layer>.hide                     // Hide layer in preview
<layer>.show                     // Show layer
<layer>.opacity = <0-100>        // Set layer opacity
<layer>.blend = <mode>           // Set blend mode
<layer>.order = <position>       // Move layer to a new stack position
```

### Layer Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `<layer>.name` | string | `"base"`, auto-generated | Display name |
| `<layer>.visible` | bool | `true` | Whether layer is visible in preview |
| `<layer>.opacity` | int | `100` | Opacity 0–100 |
| `<layer>.blend` | enum | `normal` | Blend mode: `normal`, `multiply`, `add`, `screen` |
| `<layer>.bounds` | rect | canvas bounds | Layer can have its own draw bounds |
| `<layer>.lb`, `.rb`, `.tb`, `.bb` | int | canvas bounds | Layer bound edges |

```
create layer outline_layer above
outline_layer.opacity = 80
select layer outline_layer
```

### Layer Draw Bounds

By default, a layer's draw bounds match the canvas. They can be overridden:

```
<layer>.bounds = lb+4, tb+4 to rb-4, bb-4
```

When drawing on a layer with custom bounds, the layer's bounds become the implicit `lb`, `rb`, `tb`, `bb` for expressions within that layer.

---

## 8. Drawing Operations

All drawing operations execute on the **current layer** of the **active canvas**.

### 8.1 Set Pixel

```
set <x>, <y> <color>
```

```
set 10, 5 skin
set lb, tb outline
set center eye
```

### 8.2 Fill

Fill the entire layer (or a named area) with a solid color.

```
fill <color>
fill <area> <color>
```

```
fill bg
fill body armor
fill head skin
```

### 8.3 Draw Line

```
draw line <point1> to <point2> with <color> [width <n>]
```

```
draw line lb+5, tb to lb+5, bb with outline
draw line topleft_inset to topright_inset with outline width 2
draw line center to rb, cy with armor
```

### 8.4 Draw Rectangle

```
draw rect <point1> to <point2> with <color> [filled]
draw rect <point1> to <point2> with <color> outline [width <n>]
```

- `filled` — fill the rectangle (default if neither `filled` nor `outline` is specified).
- `outline` — draw only the border.

```
draw rect lb, tb to rb, bb with outline outline      // outline rect with 'outline' color
draw rect body.lb, body.tb to body.rb, body.bb with armor filled
draw rect lb+2, tb+2 to rb-2, bb-2 with skin outline width 2
```

### 8.5 Draw Circle / Ellipse

```
draw circle <center_point> radius <r> with <color> [filled|outline [width <n>]]
draw ellipse <center_point> radius <rx>, <ry> with <color> [filled|outline [width <n>]]
```

```
draw circle cx, cy radius 8 with skin filled
draw circle left_eye radius 2 with eye filled
draw ellipse cx, cy radius 12, 8 with armor outline
```

### 8.6 Draw Arc

```
draw arc <center_point> radius <r> from <angle1> to <angle2> with <color> [width <n>]
```

Angles in degrees, 0 = right, counter-clockwise.

```
draw arc cx, cy+4 radius 6 from 200 to 340 with outline   // smile
```

### 8.7 Flood Fill

```
floodfill <point> with <color>
```

Fills all contiguous pixels of the same color starting from the given point.

```
floodfill cx, cy with skin
```

### 8.8 Draw Bezier Curve

```
draw bezier <p1> <p2> <p3> [<p4>] with <color> [width <n>]
```

3 control points = quadratic, 4 = cubic.

```
draw bezier lb+4,cy  cx,tb+2  rb-4,cy with outline
```

---

## 9. Area Operations

### 9.1 Copy Area

Copy the pixels of a named area to a new position (top-left corner of the destination).

```
copy <area> to <x>, <y>
```

```
create area badge at lb+2, tb+2 to lb+8, tb+8
// ... draw badge contents ...
copy badge to rb-8, tb+2     // mirror badge to top-right
```

### 9.2 Copy Area With Transform

```
copy <area> to <x>, <y> [fliph] [flipv] [rotate <degrees>]
```

```
copy badge to rb-8, tb+2 fliph
copy badge to lb+2, bb-8 flipv
copy badge to rb-8, bb-8 fliph flipv
```

### 9.3 Tile Area

Repeat an area across a region.

```
tile <source_area> over <dest_area>
tile <source_area> over <x1>, <y1> to <x2>, <y2>
```

```
create area brick at 0, 0 to 7, 3
// ... draw one brick ...
create area wall at 0, 0 to 31, 31
tile brick over wall
```

### 9.4 Move Area

```
move <area> to <x>, <y>
```

Moves the pixels and updates the area definition. The source region is filled with transparent.

### 9.5 Clear Area

```
clear <area>
clear <area> with <color>
```

Fill area with transparent (default) or a specified color.

---

## 10. Gradients

### 10.1 Gradient Definition

Define a reusable gradient with a direction vector, color stops, and their positions.

```
create gradient <name> vector <vx>, <vy> stops <position> <color> [, <position> <color>] ...
```

- **`vector`**: A unit direction. `0,1` = top-to-bottom, `1,0` = left-to-right, `1,1` = diagonal, `0.5,0.866` = arbitrary angle.
- **`stops`**: Each stop has a `position` (0.0 = start, 1.0 = end) and a color reference.

```
create gradient sky vector 0, 1 stops 0.0 sky_top, 0.4 sky_mid, 1.0 sky_bottom
create gradient sunset vector 1, 0 stops 0.0 orange, 0.5 pink, 1.0 purple
create gradient metal vector 0, 1 stops 0.0 light_metal, 0.3 dark_metal, 0.5 light_metal, 0.7 dark_metal, 1.0 light_metal
```

### 10.2 Gradient Fill

Apply a gradient to the entire layer or a named area.

```
fill gradient <name>
fill <area> gradient <name>
```

```
fill gradient sky
fill head gradient metal
```

### 10.3 Radial Gradient

```
create gradient <name> radial <center_point> radius <r> stops <position> <color>, ...
```

```
create gradient glow radial cx, cy radius 12 stops 0.0 bright, 0.6 mid, 1.0 dark
fill gradient glow
```

---

## 11. Patterns

### 11.1 Pattern Definition


Define a repeating pattern from a small pixel grid.

```
create pattern <name> <width> <height> {
    <row_data>
    ...
}
```

Each row is a sequence of color names or `.` for transparent, separated by spaces:

```
create pattern checkerboard 2 2 {
    dark light
    light dark
}

create pattern bricks 8 4 {
    outline outline outline outline outline outline outline outline
    armor   armor   armor   outline armor   armor   armor   outline
    armor   armor   armor   outline armor   armor   armor   outline
    outline outline outline outline outline outline outline outline
}
```

### 11.2 Pattern Fill

```
fill pattern <name>
fill <area> pattern <name>
```

```
fill body pattern bricks
```

---

## 12. Image Operations

### 12.1 Load Image

Load an external image file onto the current layer.

```
load <filepath>
load <filepath> at <x>, <y>
load <filepath> into <area>
```

- Plain `load` resizes the layer/canvas to the image dimensions.
- `at` places the image at a specific position (top-left corner).
- `into` scales the image to fit the named area.

```
load "assets/face_template.png"
load "assets/badge.png" at rb-16, tb+2
load "assets/texture.png" into body
```

### 12.2 Resize

```
resize <width> <height> [nearest|bilinear]
```

Resize the canvas. Default interpolation is `nearest` (good for pixel art).

```
resize 64 64
resize 128 128 bilinear
```

### 12.3 Crop

```
crop <x1>, <y1> to <x2>, <y2>
crop <area>
```

```
crop lb+4, tb+4 to rb-4, bb-4
crop body
```

### 12.4 Flip / Rotate

```
flip horizontal
flip vertical
rotate <degrees>          // 90, 180, 270 only for lossless pixel rotation
```

---

## 13. Variables

Simple integer or string variables for reuse across the script. Assignment uses `=` syntax.

```
<name> = <expression>
```

```
margin = 4
body_top = tb + 20
body_left = lb + margin

create area body at body_left, body_top to rb - margin, bb - margin
```

Variables are expanded at parse time wherever an integer value or bound expression is expected.

Variables can also capture object properties and palette lookups:

```
center_x = head.cx
armor_idx = mypal[armor]
half_w = w / 2
```

---

## 14. Loops

Repeat operations with a counter variable.

```
repeat <n> as <var> {
    ...
}
```

The variable goes from `0` to `n-1`.

```
repeat 4 as i {
    set lb + i * 8 + 4, tb + 4 outline
}
```

### For-Each Over Range

```
for <var> from <start> to <end> [step <s>] {
    ...
}
```

```
for x from lb to rb step 2 {
    set x, tb outline
    set x, bb outline
}
```

---

## 15. Conditionals

Basic control flow for parametric scripts.

```
if <condition> {
    ...
}

if <condition> {
    ...
} else {
    ...
}
```

Conditions support:
- Comparison: `<expr> == <expr>`, `<expr> != <expr>`, `<expr> < <expr>`, `<expr> > <expr>`, `<expr> <= <expr>`, `<expr> >= <expr>`
- Logical: `and`, `or`, `not`

```
size = w
if size > 16 {
    draw rect lb+1, tb+1 to rb-1, bb-1 with outline outline
}
```

---

## 16. Includes and Macros

### 16.1 Include

Insert another script inline.

```
include "shared/draw_border.vdecanvas"
```

### 16.2 Macro Definition

Define reusable command sequences.

```
create macro <name>(<param1>, <param2>, ...) {
    ...
}
```

```
create macro draw_eye(pos, size) {
    draw circle pos radius size with eye filled
    draw circle pos radius size - 1 with skin filled
    set pos eye
}

draw_eye(left_eye, 3)
draw_eye(right_eye, 3)
```

### 16.3 Macro With Default Parameters

```
create macro bordered_rect(p1, p2, fill_color, border_color, border_width = 1) {
    draw rect p1 to p2 with fill_color filled
    draw rect p1 to p2 with border_color outline width border_width
}

bordered_rect(lb, tb to rb, bb, armor, outline)
bordered_rect(lb+4, tb+4 to rb-4, bb-4, skin, outline, 2)
```

---

## 17. Export

Directives for how the canvas should be saved.

```
export png "output/hero_sprite.png"
export bmp "output/hero_sprite.bmp"
export tga "output/hero_sprite.tga"
```

---

## 18. Full Example

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
// Head
in head {
    fill skin
    draw rect lb, tb to rb, bb with outline outline

    // Hair
    draw line lb, tb to rb, tb with hair width 2
    draw line lb, tb+1 to lb, tb+4 with hair

    // Eyes
    create point left_eye at cx-3, cy
    create point right_eye at cx+3, cy
    set left_eye eye
    set right_eye eye

    // Mouth
    draw line cx-2, cy+3 to cx+2, cy+3 with outline
}

// Body
in body {
    fill armor
    draw rect lb, tb to rb, bb with outline outline
    draw line lb, tb+1 to rb, tb+1 with highlight
    draw line lb, bb-1 to rb, bb-1 with shadow

    // Belt
    draw line lb, bb-3 to rb, bb-3 with belt width 2
    set cx, bb-3 buckle
}

// Legs
fill left_leg boot
fill right_leg boot
draw rect left_leg.lb, left_leg.tb to left_leg.rb, left_leg.bb with outline outline
draw rect right_leg.lb, right_leg.tb to right_leg.rb, right_leg.bb with outline outline

// --- Layer 1: Armor highlight ---
create layer highlights above
highlights.opacity = 40

create gradient armor_shine vector 0, 1 stops 0.0 highlight, 0.5 #FFFFFF00, 1.0 shadow
fill body gradient armor_shine

// --- Palette lookup example ---
armor_idx = hero_pal[armor]      // 3
first_color = hero_pal[0]        // bg

// --- Export ---
export png "output/hero_sprite.png"
```

---

## 19. Grammar Summary (EBNF-like)

```ebnf
script          = { statement NEWLINE } ;
statement       = create_stmt | metadata | select_stmt | palette_op | layer_prop
                | variable_def | draw_cmd | fill_cmd | set_cmd | floodfill_cmd
                | copy_cmd | move_cmd | clear_cmd | tile_cmd
                | load_cmd | resize_cmd | crop_cmd | flip_cmd | rotate_cmd
                | control_flow | scoped_block | include | macro_call
                | export_cmd | comment ;

(* --- Object Creation: unified 'create' keyword --- *)
create_stmt     = "create" ( canvas_def | color_def | palette_def | point_def
                           | area_def | layer_def | gradient_def | pattern_def
                           | macro_def ) ;

canvas_def      = "canvas" IDENT INT INT ;
color_def       = "color" IDENT COLOR_LITERAL ;
palette_def     = "palette" IDENT IDENT { "," IDENT } ;
point_def       = "point" IDENT "at" expr "," expr ;
area_def        = "area" IDENT "at" expr "," expr "to" expr "," expr ;
layer_def       = "layer" IDENT [DIRECTION] [IDENT] ;
gradient_def    = "gradient" IDENT ( "vector" expr "," expr | "radial" point "radius" expr )
                  "stops" stop { "," stop } ;
pattern_def     = "pattern" IDENT INT INT "{" { pattern_row } "}" ;
macro_def       = "macro" IDENT "(" [param { "," param }] ")" "{" { statement NEWLINE } "}" ;

(* --- Metadata (applies to active canvas) --- *)
metadata        = "origin" ORIGIN | "background" COLOR_REF ;

(* --- Selection --- *)
select_stmt     = "select" ( "palette" IDENT | "layer" (IDENT | INT) ) ;

(* --- Palette Operations --- *)
palette_op      = palette_ref ( "add" IDENT { "," IDENT }
                              | "drop" IDENT ) ;
palette_ref     = IDENT | "palette" ;                             (* named palette or active *)

(* --- Layer Property Assignment --- *)
layer_prop      = IDENT "." LAYER_PROP "=" expr ;

(* --- Variable Assignment --- *)
variable_def    = IDENT "=" expr
                | IDENT "=" palette_ref "[" (IDENT | INT) "]" ;   (* palette index lookup *)

(* --- Drawing --- *)
draw_cmd        = "draw" ( "line" point "to" point "with" COLOR_REF ["width" INT]
                         | "rect" point "to" point "with" COLOR_REF [FILL_MODE ["width" INT]]
                         | "circle" point "radius" expr "with" COLOR_REF [FILL_MODE ["width" INT]]
                         | "ellipse" point "radius" expr "," expr "with" COLOR_REF [FILL_MODE ["width" INT]]
                         | "arc" point "radius" expr "from" expr "to" expr "with" COLOR_REF ["width" INT]
                         | "bezier" point point point [point] "with" COLOR_REF ["width" INT] ) ;

fill_cmd        = "fill" [IDENT] ( COLOR_REF | "gradient" IDENT | "pattern" IDENT ) ;
set_cmd         = "set" point COLOR_REF ;
floodfill_cmd   = "floodfill" point "with" COLOR_REF ;

(* --- Area Operations --- *)
copy_cmd        = "copy" IDENT "to" expr "," expr { "fliph" | "flipv" | "rotate" INT } ;
move_cmd        = "move" IDENT "to" expr "," expr ;
clear_cmd       = "clear" IDENT ["with" COLOR_REF] ;
tile_cmd        = "tile" IDENT "over" ( IDENT | expr "," expr "to" expr "," expr ) ;

(* --- Image Operations --- *)
load_cmd        = "load" STRING ["at" expr "," expr | "into" IDENT] ;
resize_cmd      = "resize" INT INT [INTERP] ;
crop_cmd        = "crop" ( expr "," expr "to" expr "," expr | IDENT ) ;
flip_cmd        = "flip" ( "horizontal" | "vertical" ) ;
rotate_cmd      = "rotate" INT ;

(* --- Control Flow --- *)
scoped_block    = "in" IDENT "{" { statement NEWLINE } "}" ;
control_flow    = repeat_loop | for_loop | if_stmt ;
repeat_loop     = "repeat" INT "as" IDENT "{" { statement NEWLINE } "}" ;
for_loop        = "for" IDENT "from" expr "to" expr ["step" expr] "{" { statement NEWLINE } "}" ;
if_stmt         = "if" condition "{" { statement NEWLINE } "}" ["else" "{" { statement NEWLINE } "}"] ;
condition       = expr COMP_OP expr { LOGIC_OP condition } ;

param           = IDENT ["=" expr] ;
macro_call      = IDENT "(" [arg { "," arg }] ")" ;

include         = "include" STRING ;

export_cmd      = "export" FORMAT STRING ;

comment         = "//" TEXT | "/*" TEXT "*/" ;

(* --- Supporting Rules --- *)
stop            = FLOAT COLOR_REF ;
pattern_row     = { ( IDENT | "." ) } ;
point           = expr "," expr | IDENT ;
expr            = INT | FLOAT | IDENT | BOUND
                | IDENT "." PROPERTY
                | palette_ref "[" ( IDENT | INT ) "]"
                | expr ( "+" | "-" | "*" | "/" ) expr
                | INT "%" ( "w" | "h" ) ;

(* --- Terminals --- *)
COLOR_REF       = IDENT | COLOR_LITERAL | palette_ref "[" INT "]" ;
COLOR_LITERAL   = "#" HEX{6} | "#" HEX{8} ;
BOUND           = "lb" | "rb" | "tb" | "bb" | "cx" | "cy" | "w" | "h" ;
PROPERTY        = "lb" | "rb" | "tb" | "bb" | "cx" | "cy" | "w" | "h"
                | "x" | "y" | "r" | "g" | "b" | "a" | "hex"
                | "count" | "colors" | "name" | "visible" | "opacity" | "blend" ;
LAYER_PROP      = "opacity" | "blend" | "visible" | "bounds" | "order" ;
DIRECTION       = "above" | "below" ;
FILL_MODE       = "filled" | "outline" ;
BLEND_MODE      = "normal" | "multiply" | "add" | "screen" ;
ORIGIN          = "topleft" | "bottomleft" ;
INTERP          = "nearest" | "bilinear" ;
FORMAT          = "png" | "bmp" | "tga" ;
COMP_OP         = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
LOGIC_OP        = "and" | "or" ;
IDENT           = LETTER { LETTER | DIGIT | "_" } ;
STRING          = '"' { CHAR } '"' ;
INT             = ["-"] DIGIT { DIGIT } ;
FLOAT           = ["-"] DIGIT { DIGIT } "." DIGIT { DIGIT } ;
```

---

## 20. Implementation Notes

### Parser Architecture

The DSL parser should be a **two-pass** system:

1. **Pass 1 — Parse & Validate**: Tokenize each line, build an AST of statements, resolve `include` directives, validate palette references, check that named points/areas are defined before use.
2. **Pass 2 — Execute**: Walk the AST, evaluate expressions against current bounds context, and emit low-level `CommandSystem` calls (`paint`, `fill`, `line`, `rect`, etc.).

### Expression Evaluator

A simple stack-based evaluator handles bound arithmetic:

- Maintains a scope stack: canvas bounds at the bottom, `in <area>` blocks push new scopes.
- Named points, areas, and variables are stored in a symbol table.
- `<area>.property` lookups resolve against the area definition.

### Integration With the Command System

Each DSL drawing operation maps to one or more `CommandSystem` commands:

| DSL Statement | Command(s) Emitted |
|---|---|
| `create color skin #FFCC99` | Registers color in symbol table (no command emitted) |
| `create palette mypal skin, outline` | Registers palette (no command emitted) |
| `set 10, 5 skin` | `paint 10 5 #FFCC99FF 1` |
| `fill body armor` | Sequence of `paint` or a bulk `fill` within area bounds |
| `draw line ...` | `line x1 y1 x2 y2 #color thickness` |
| `draw rect ...` | `rect x y w h #color filled` |
| `draw circle ...` | `circle cx cy r #color filled` |
| `fill gradient sky` | Per-pixel `paint` commands (or a bulk gradient operation) |
| `select layer highlights` | `layer select highlights` |
| `mypal[armor]` | Resolves to integer index at parse time |
| `copy badge to rb-8, tb+2` | Internal pixel buffer copy (no direct command — becomes a batch of `paint` for log fidelity, or a single `copy_area` command) |

### File Organization

```
tools/resource_editor/
    dsl/
        CanvasDSLParser.h          // Tokenizer and AST builder
        CanvasDSLParser.cpp
        CanvasDSLExprEval.h        // Expression evaluator with bound context
        CanvasDSLExprEval.cpp
        CanvasDSLExecutor.h        // AST walker that emits CommandSystem calls
        CanvasDSLExecutor.cpp
        CanvasDSLTypes.h           // AST node types, symbol table, scope
```

### Error Reporting

All errors should include the script file path, line number, and a clear message:

```
hero_sprite.vdecanvas:14: error: undefined color 'armorr' (did you mean 'armor'?)
hero_sprite.vdecanvas:18: error: palette 'mypal' references undefined color 'blu' (did you mean 'blue'?)
hero_sprite.vdecanvas:22: error: area 'torso' used before definition
hero_sprite.vdecanvas:30: error: gradient 'sky' has no stops defined
hero_sprite.vdecanvas:35: error: duplicate name 'body' — an area with this name already exists
```

---

## 21. Design Decisions & Rationale

| Decision | Rationale |
|---|---|
| **Unified `create <type> <name>` syntax** | Every object has a name and id. Consistent syntax makes the language predictable and easy to learn — one pattern to remember, one way to introduce any object. |
| **Colors as first-class objects** | Separating color definition from palette membership means colors can be reused across multiple palettes, redefined independently, and referenced without needing a palette at all. |
| **Palettes as ordered color collections** | Palettes group and order colors for indexed access, theming, and pattern definitions — but don't gate color usage. |
| **Palette `add`/`drop`/`[]` operations** | Makes palettes mutable and queryable. Index lookup (`mypal[blue]`) enables programmatic pattern generation and loop-driven drawing. |
| **`=` assignment (no `let` keyword)** | Terse and familiar. Reduces noise in scripts. |
| **Named colors instead of raw hex** | Readability, re-theming, fewer typos. Raw hex still allowed as fallback via `#RRGGBBAA`. |
| **Bound variables (lb, rb, tb, bb)** | Makes scripts resolution-independent. Resize the canvas and the drawing adapts. |
| **Named points and areas** | Reduces magic numbers, makes intent clear, enables area-scoped operations. |
| **`in <area>` scoping** | Lets you write drawing code relative to a sub-region without manual offset math. |
| **Layers as ordered stack with names** | Matches artist mental model. Named layers are easier to script than indices. |
| **Dot-notation properties** | Uniform access to object state (`head.cx`, `mypal.count`, `base.opacity`). No special functions needed. |
| **No operator precedence** | Keeps the expression grammar trivial to parse. Complex math belongs in variables. |
| **Macros over functions** | Macros are text-expansion — simpler to implement, no stack frames or return values needed. |
| **`.vdecanvas` extension** | Distinguishes from `.vdescript` (input automation) and `.vdepalette` (shared palettes). |
| **Two-pass parse then execute** | Allows forward references in some cases, better error reporting, and script validation before any pixels are modified. |

---

## 22. Future Extensions

- **Animation timeline** — extend the DSL to describe frame sequences for sprite sheets.
- **Parametric scripts** — accept arguments from the command line (`canvas 32 32 --color_scheme=dark`).
- **Visual debugger** — step through DSL execution line-by-line in the editor, highlighting the affected pixels.
- **Autocomplete** — in the editor REPL, offer palette color names, point/area names, and macro names.
- **Tilemap DSL** — a companion language for describing tile-based maps using canvases as tile sources.
