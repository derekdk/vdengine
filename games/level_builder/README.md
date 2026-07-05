# Level Builder

Phase 2 keeps the tilemap runtime baseline while moving input onto named keyboard/gamepad actions and adding a Development mode toggle.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.

## Current controls

| Input | Action |
|-------|--------|
| `A` / `D` or arrows | Move |
| Gamepad D-pad Left / Right or left stick | Move |
| `Space` / `W` / Up | Jump |
| Gamepad `A` or D-pad Up | Jump |
| `R` | Reset |
| Gamepad Back | Reset |
| `Enter` or Gamepad Start | Toggle Development mode |
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Next milestones

- Add tile selection and cursor rendering.
- Add in-memory tile editing.