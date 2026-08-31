# Level Builder

The current build keeps the tilemap runtime baseline while adding multi-layer authoring, palette-driven painting, layer-aware undo and redo, and persisted layer overlays.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.
- Adds a default Development Move Mode for free scene navigation with no collisions or gravity.
- Adds Select Tile Mode with nearest-tile acquisition, a white selection outline, and controller-driven tile navigation.
- Uses the clipboard as an explicit paint palette so next/previous actions choose a brush tile before painting.
- Shows a floating tile palette in Select Tile Mode with every tileset tile and a highlighted current cut tile.
- Supports layer creation, selection, visibility, depth, and scroll-preset authoring in Development Move Mode.
- Paints only the active layer and restores the correct layer during undo or redo.
- Limits gameplay collision to layers marked `collisionEnabled`; new decorative layers start disabled.
- Saves and reloads the complete layer stack as a VDE-native overlay snapshot.
- Surfaces active-layer state, selected-tile IDs, palette state, undo/redo depth, and overlay save status in the HUD and debug overlay.

## Current controls

| Input | Action |
|-------|--------|
| `A` / `D` or arrows | Move |
| `W` / Up | Jump in Play mode, move up in Development Move Mode, or move tile selection up |
| `S` / Down | Move down in Development Move Mode or move tile selection down |
| Gamepad D-pad / left stick | Move player in Move Mode or tile selection in Select Tile Mode |
| `Space` | Jump |
| Gamepad `A` or D-pad Up | Jump |
| `R` | Reset |
| Gamepad Back | Reset |
| `Enter` or Gamepad Start | Toggle Development mode |
| `Q` / `E` or Gamepad LB / RB | Cycle Development submodes |
| `N` or Gamepad `A` in Development Move Mode | Add and select a new layer |
| `J` / `K` or Gamepad `X` / `Y` in Development Move Mode | Select previous / next layer |
| `H` or Gamepad `B` in Development Move Mode | Toggle active-layer visibility |
| `O` / `P` or Gamepad LT / RT in Development Move Mode | Adjust active-layer depth |
| `[` / `]` or Gamepad right stick in Development Move Mode | Cycle active-layer scroll presets |
| `Z` / `X` or Gamepad `B` / `A` | Previous / next palette tile in Select Tile Mode |
| `C` / `V` or Gamepad `X` / `Y` | Copy the selected tile into the palette or paint with the palette |
| `U` / `I` or Gamepad LT / RT | Undo / redo the last tile edit |
| `F5` / `F9` or Gamepad `L3` / `R3` | Save / reload the layer-stack overlay |
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Persistence

- The persistence strategy is a VDE-native overlay snapshot for the complete authorable layer stack.
- The overlay is written beside the executable as `level_builder_ground.overlay.json`.
- Loading restores the checked-in imported map first, then reapplies the saved overlay if the file exists.
- Undo and redo operate across in-memory edits on all authorable layers; loading an overlay resets that history.
- The palette is the same visible value used for copy and paint, so controller users only need to learn one brush concept.
- Overlay entries preserve layer name, tile payload, depth, visibility, collision participation, and scrolling metadata. Legacy one-layer overlays load as a ground layer plus imported supporting layers.
- Object edits and export back to Tiled remain future work.

### Overlay schema and compatibility

New overlays use version 2 of the `vde.level_builder.ground_overlay` format. The top-level
object contains `format`, `version`, `base_map`, and a `layers` array. Each layer records:

- `id` and `name` for identity and display;
- `depth_z`, `visible`, and `collision_enabled` for rendering and gameplay participation;
- `follow_factor_x`, `follow_factor_y`, `scroll_velocity_x`, `scroll_velocity_y`,
  `scroll_offset_x`, and `scroll_offset_y` for camera-relative movement; and
- `columns`, `rows`, and row-major `tiles` for the tile payload.

Version 1 overlays with the former `editable_layer` object remain supported. Their ground-layer
tiles are loaded into layer zero, while any other imported map layers retain their source-map
definitions. New saves are always written in version 2.

## Next milestones

- Add region tools for multi-tile paint, fill, or marquee workflows.
- Extend authoring support to object or gameplay-marker editing.