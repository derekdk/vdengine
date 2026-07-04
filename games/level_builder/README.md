# Level Builder

Phase 1 ports the tilemap runtime demo into a real `games/` target with the code split by concern.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.

## Current controls

| Input | Action |
|-------|--------|
| `A` / `D` or arrows | Move |
| `Space` / `W` / Up | Jump |
| `R` | Reset |
| Gamepad D-pad Left / Right | Move |
| Gamepad `A` or D-pad Up | Jump |
| Gamepad Back | Reset |
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Next milestones

- Add joystick-focused Development mode input.
- Add tile selection and cursor rendering.
- Add in-memory tile editing.