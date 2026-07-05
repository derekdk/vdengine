# Level Builder

Phase 3 keeps the tilemap runtime baseline while adding a Development submode controller and a default no-collision Move Mode.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.
- Adds a default Development Move Mode for free scene navigation with no collisions or gravity.

## Current controls

| Input | Action |
|-------|--------|
| `A` / `D` or arrows | Move |
| `W` / Up | Jump in Play mode, move up in Development Move Mode |
| `S` / Down | Move down in Development Move Mode |
| Gamepad D-pad Left / Right or left stick | Move |
| Gamepad D-pad Up / Down or left stick Y | Vertical move in Development Move Mode |
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

- Add Select Tile Mode with a white tile outline.
- Add tile copy/paste and next/previous tile actions.
- Add in-memory tile editing.