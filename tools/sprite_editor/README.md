# VDE Sprite Editor

A visual tool for creating, previewing, and exporting spritesheet assets for the VDE engine.

## Features

- **Load source images** — PNG/JPG via `ImageLoader`
- **Define sprite regions** — Auto-slice with `grid` command or manual `add`
- **Build animations** — Create named animation sequences with per-frame timing
- **Preview animations** — Real-time playback in the viewport
- **Export `.vdesheet` metadata** — TOML format consumed by VDE's runtime API
- **Script mode** — Batch creation from command-line scripts

## Usage

### Interactive Mode (GUI)
```
vde_sprite_editor
```

### Script Mode (Batch)
```
vde_sprite_editor myscript.txt
```

### Script Example
```
# Load image and create spritesheet
load characters.png
grid 64 64
anim create idle
anim addframe idle sprite_0 0.15
anim addframe idle sprite_1 0.15
save characters.vdesheet
```

## Commands

| Command | Description |
|---------|-------------|
| `load <path>` | Load a source image (PNG/JPG) |
| `save <path.vdesheet>` | Export spritesheet metadata |
| `open <path.vdesheet>` | Open existing spritesheet |
| `grid <w> <h> [sx sy] [ox oy]` | Auto-slice into grid |
| `add <name> <x> <y> <w> <h>` | Add named sprite region |
| `remove <name>` | Remove sprite region |
| `rename <old> <new>` | Rename sprite region |
| `anchor <name> <x> <y>` | Set anchor point (0-1) |
| `list` | List all sprites |
| `select <name>` | Select sprite |
| `anim create <name> [loop\|noloop]` | Create animation |
| `anim delete <name>` | Delete animation |
| `anim addframe <anim> <sprite> [dur]` | Add frame |
| `anim removeframe <anim> <idx>` | Remove frame |
| `anim setduration <anim> <idx> <sec>` | Set frame duration |
| `anim list` | List animations |
| `anim play <name>` | Preview animation |
| `anim stop` | Stop playback |
| `zoom <level>` | Set zoom (1.0=fit) |
| `help` | Show help |
| `clear` | Clear console |
| `info` | Document summary |

## File Format

The `.vdesheet` format uses TOML. See `docs/design/SPRITE_EDITOR_PLAN.md` for the full specification.
