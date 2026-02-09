# Geometry REPL Tool

An interactive tool for creating and exporting custom 3D geometry using a command-line interface within VDE.

## Overview

The Geometry REPL (Read-Eval-Print Loop) Tool provides an ImGui-based console interface where users can:

- Create custom geometry by defining points in 3D space
- Set colors for geometry objects
- Visualize geometry in real-time
- Export geometry to OBJ format for use in other tools

## Building

The tool is built automatically with the VDE examples:

```powershell
.\scripts\build.ps1
```

The executable will be located at:
- `build_ninja\examples\vde_geometry_repl.exe` (Ninja)
- `build\examples\Debug\vde_geometry_repl.exe` (Visual Studio)

## Running

```powershell
.\build_ninja\examples\vde_geometry_repl.exe
```

## Commands

### create <name> <type>
Create a new geometry object.

**Types:**
- `polygon` - Creates a filled polygon (requires 3+ points)
- `line` - Creates a line path (requires 2+ points)

**Example:**
```
create triangle polygon
create path line
```

### addpoint <name> <x> <y> <z>
Add a point to a geometry object. Points should be specified in 3D coordinates.

**Example:**
```
addpoint triangle 0 0 0
addpoint triangle 1 0 0
addpoint triangle 0.5 1 0
```

### setcolor <name> <r> <g> <b>
Set the fill color for a geometry object. Colors are specified in RGB format with values from 0 to 1.

**Example:**
```
setcolor triangle 1.0 0.0 0.0    # Red
setcolor path 0.0 1.0 0.0         # Green
```

### setvisible <name>
Make a geometry object visible in the 3D scene. The geometry must have enough points defined:
- Polygons require at least 3 points
- Lines require at least 2 points

**Example:**
```
setvisible triangle
```

### hide <name>
Hide a geometry object from the scene.

**Example:**
```
hide triangle
```

### export <name> <filename>
Export a geometry object to OBJ format.

**Example:**
```
export triangle triangle.obj
export path mypath.obj
```

### list
List all geometry objects with their properties.

**Example:**
```
list
```

### clear <name>
Delete a geometry object.

**Example:**
```
clear triangle
```

### help
Display command reference in the console.

## User Interface

The tool provides multiple windows:

### REPL Console
- Command input field at the bottom
- Command history and output above
- Menu bar with quick access to common commands

### Geometry Inspector
- Shows all created geometry objects
- Collapsible panels for each object showing:
  - Object type (polygon/line)
  - Number of points
  - Color picker
  - Visibility toggle
  - Export button

### Stats
- FPS counter
- Number of geometry objects
- Number of visible objects

### 3D Viewport
- Shows the rendered scene with:
  - Ground plane (gray grid)
  - Coordinate axes (X=red, Y=green, Z=blue)
  - User-created geometry (when visible)
- Camera controls:
  - **Left mouse drag** - Orbit camera around the origin
  - **Mouse wheel** - Zoom in/out
  - Camera respects ImGui windows (no rotation when dragging over UI)

## Controls

- **Left Mouse Drag** - Rotate camera (when not over UI windows)
- **Mouse Wheel** - Zoom camera in/out
- **Mouse** - Interact with UI panels
- **F1** - Toggle debug UI visibility
- **F11** - Toggle fullscreen
- **ESC** - Exit the tool

## Example Workflow

Create a simple triangle:

```
create mytri polygon
addpoint mytri 0 0 0
addpoint mytri 1 0 0
addpoint mytri 0.5 1 0
setcolor mytri 1.0 0.0 0.0
setvisible mytri
export mytri triangle.obj
```

Create a line path:

```
create myline line
addpoint myline -1 0 -1
addpoint myline 0 1 0
addpoint myline 1 0 1
setcolor myline 0.0 0.0 1.0
setvisible myline
```

## Implementation Details

### Geometry Types

**Polygon:**
- Creates double-sided triangle fan from points
- Assumes convex polygon
- Best for simple shapes like triangles, quads, pentagons

**Line:**
- Creates thin rectangular tubes between consecutive points
- Line width is fixed at 0.02 units
- Useful for paths and wireframe-style drawings

### OBJ Export Format

Exported OBJ files include:
- Comments with geometry name and type
- Vertex positions (`v` lines)
- Face definitions for polygons (`f` lines)
- Line definitions for line geometry (`l` lines)

The exported files can be imported into Blender, Maya, or other 3D tools.

## Technical Notes

- Uses VDE's Game API for scene management
- ImGui provides the REPL interface
- Geometry is created dynamically using `Mesh::setData()`
- Coordinate axes use VDE's primitive mesh generators
- Double-sided rendering for polygons (reverse face is automatically created)
- **Mouse camera controls:**
  - Left mouse button drag rotates the OrbitCamera
  - Mouse wheel zooms the camera in/out
  - Camera only responds to mouse input when not over ImGui windows (uses `ImGui::GetIO().WantCaptureMouse`)
  - Rotation sensitivity: 0.2 degrees per pixel
  - Zoom sensitivity: 0.8 units per scroll tick
