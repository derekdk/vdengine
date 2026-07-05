# Level Builder

Phase 4 keeps the tilemap runtime baseline while adding Select Tile Mode on top of the Development submode controller.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.
- Adds a default Development Move Mode for free scene navigation with no collisions or gravity.
- Adds Select Tile Mode with nearest-tile acquisition, a white selection outline, and controller-driven tile navigation.

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
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Next milestones

- Add tile copy/paste and next/previous tile actions.
- Add in-memory tile editing.