# Level Builder

The current post-Phase-6 build keeps the tilemap runtime baseline while adding palette-driven painting, undo and redo, persisted ground-layer overlays, and end-to-end authoring verification.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.
- Adds a default Development Move Mode for free scene navigation with no collisions or gravity.
- Adds Select Tile Mode with nearest-tile acquisition, a white selection outline, and controller-driven tile navigation.
- Uses the clipboard as an explicit paint palette so next/previous actions choose a brush tile before painting.
- Supports undo and redo for ground-layer edits during the current authoring session.
- Saves and reloads the editable `ground` layer as a VDE-native overlay snapshot.
- Surfaces selected-tile IDs, palette state, undo/redo depth, and overlay save status in the HUD and debug overlay.

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
| `Z` / `X` or Gamepad `B` / `A` | Previous / next palette tile in Select Tile Mode |
| `C` / `V` or Gamepad `X` / `Y` | Copy the selected tile into the palette or paint with the palette |
| `U` / `I` or Gamepad LT / RT | Undo / redo the last tile edit |
| `F5` / `F9` or Gamepad `L3` / `R3` | Save / reload the editable ground-layer overlay |
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Persistence

- The current persistence strategy is a VDE-native overlay snapshot for the imported `ground` layer.
- The overlay is written beside the executable as `level_builder_ground.overlay.json`.
- Loading restores the checked-in imported map first, then reapplies the saved overlay if the file exists.
- Undo and redo operate on the in-memory editable ground layer for the current session; loading an overlay resets that history.
- The palette is the same visible value used for copy and paint, so controller users only need to learn one brush concept.
- Persistence still covers only the editable ground layer. Other layers, object edits, and export back to Tiled remain future work.

## Next milestones

- Add region tools for multi-tile paint, fill, or marquee workflows.
- Extend authoring support beyond the ground layer into object or gameplay-marker editing.