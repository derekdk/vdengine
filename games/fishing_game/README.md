# Pond Fisher

`Pond Fisher` is the canonical example game for the new `games/` category.

It demonstrates:

- A multi-file layout under `games/`
- `games/GameBase.h` for game startup and debug UI integration
- `KeyStateTracker`-based controls
- A scripted smoke-test path via `games/fishing_game/vde.toml`

## Controls

- `LEFT/RIGHT` - Move the boat
- `UP/DOWN` - Raise or lower the bobber while the line is in the water
- `SPACE` - Cast, reel in, or land a hooked fish
- `R` - Reset the round
- `F1` - Toggle debug UI
- `F11` - Toggle fullscreen
- `ESC` - Quit