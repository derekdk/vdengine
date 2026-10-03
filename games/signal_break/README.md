# Signal Break

Signal Break is a single-player, side-scrolling run-and-gun game built with VDE.
Cross an industrial outpost, jump onto elevated platforms, exchange fire with
patrolling sentries, and defeat the commander guarding extraction at the far right.
Original pixel textures are generated in memory; no downloaded assets are required.

## How to play

Move right through three sectors. Hold fire for automatic shots, and aim diagonally
to reach enemies above or below you. Platforms stop bullets from both sides.
Jump across the two breaks in the floor. The teal relay flags mark checkpoints;
reaching a new checkpoint on solid ground restores all six health segments.

Damage grants one second of protection. Losing all health or falling ends the run.
Jump from the "SIGNAL LOST" screen to retry at the last relay, restoring enemies
and resetting the score. Defeat the gold-tinted commander and enter the green
extraction gate to win. Jump from the victory screen to start another run.

## Controls

| Action | Keyboard | Controller (standard Xbox mapping) |
|--------|----------|------------------------------------|
| Move | A / D or Left / Right | Left stick X or D-pad Left / Right |
| Aim diagonally | W / S or Up / Down | Left stick Y or D-pad Up / Down |
| Jump / retry / play again | Space | A (PlayStation Cross) |
| Fire (hold) | J or X | X (Square), right bumper, or right trigger |
| Pause / resume | P | Start / Options |
| Restart from the beginning | R | Y (Triangle) |
| Fullscreen | F11 | Keyboard only |
| Exit | Escape | Window close / keyboard |

Keyboard and controller can be used together. Standard GLFW-mapped gamepads are
detected by VDE, with stick dead zones and disconnect cleanup. Multiple controllers
feed the same single player. Controller button/axis behavior is unit-tested;
physical controller hardware is not required by the automated tests.

## Building

```powershell
.\scripts\build.ps1
```

## Running

```powershell
.\build_ninja\games\signal_break\vde_signal_break.exe
```

Alternatively launch VLauncher with `.\scripts\run-vlauncher.ps1` and select the
`vde_signal_break` game. Run commands from the repository root.

## Code Organization

| Files | Responsibility |
|-------|----------------|
| `main.cpp` | Standard VDE game entry point and scripted-input support |
| `Input.h`, `Input.cpp` | Keyboard and gamepad actions |
| `Level.h`, `Level.cpp` | Platform layout, enemy spawns, tuning constants |
| `Gameplay.h`, `Gameplay.cpp` | Player movement, enemy AI, combat, checkpoints and run state |
| `Artwork.h`, `Artwork.cpp` | Original character, sky, skyline and metal textures |
| `SignalBreakScene.h`, `SignalBreakScene.cpp` | VDE scene lifecycle and script diagnostics |
| `Presentation.cpp` | Camera, scenery, pooled effects, animation and HUD |

The gameplay model owns a VDE `PhysicsScene` and advances it explicitly, so pause
freezes the entire simulation and headless tests exercise the same game logic.
The scene renders that state using `Camera2D`, `SpriteEntity`, and `TextEntity`.
Do not also enable scene-scheduled physics: that would create a separate simulation.
Projectiles use swept VDE raycasts; character/platform collision uses VDE rigid bodies.

## Verification

```powershell
.\scripts\test.ps1 -Build -Filter "SignalBreak*" -ProblemsOnly
.\scripts\smoke-test.ps1 -Full -Filter "*signal_break*" -ProblemsOnly
```

The unit tests cover both input sources, jumping, pause, collision, damage,
checkpoints, and a complete successful run. The smoke script checks rendered
movement, jumping, firing, scrolling, pause/resume, and restarting. It saves startup
and combat screenshots under the executable's `render_verify_output/` directory.
