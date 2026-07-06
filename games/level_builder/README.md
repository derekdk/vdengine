# Level Builder

Phase 5 keeps the tilemap runtime baseline while adding controller-driven tile editing on top of Select Tile Mode.

## Current scope

- Imports the checked-in Tiled sample map.
- Runs the playable side-view movement baseline.
- Splits map ownership, player control, and scene orchestration into separate files.
- Supports left-stick movement and Start-button Development mode toggling.
- Adds a default Development Move Mode for free scene navigation with no collisions or gravity.
- Adds Select Tile Mode with nearest-tile acquisition, a white selection outline, and controller-driven tile navigation.
- Edits the imported `ground` layer with next/previous tile cycling plus copy/paste clipboard actions.
- Surfaces selected-tile IDs and clipboard state in the HUD and debug overlay.

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
| `Z` / `X` or Gamepad `B` / `A` | Previous / next tile in Select Tile Mode |
| `C` / `V` or Gamepad `X` / `Y` | Copy / paste the selected ground-layer tile |
| `F1` | Toggle debug UI |
| `F11` | Toggle fullscreen |
| `Esc` | Quit |

## Next milestones

- Persist edited maps or overlays.
- Expand smoke and render verification around tile-mutation workflows.