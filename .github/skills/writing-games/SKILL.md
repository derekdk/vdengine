---
name: writing-games
description: Guide for creating larger game applications in VDE. Use this when building multi-file playable games that should integrate with the same build, smoke, and launcher workflows as other runnable app categories.
---

# Writing Games in VDE

This skill defines the standard pattern for creating VDE games that are larger than examples but are still runnable Game API applications rather than asset-authoring tools. Games live in their own top-level `games/` directory, use multi-file organization by concern, and participate in scripted smoke coverage.

## When to use this skill

- Creating a new playable game under `games/`
- Moving an overgrown example into a first-class game application
- Adding smoke coverage or launcher integration for a game target
- Defining the file layout and CMake wiring for a multi-file VDE game

## Games vs Examples vs Tools

**Examples:**
- Demonstrate one feature or a small cluster of APIs
- Prefer a single-file implementation when practical
- Use `examples/ExampleBase.h`

**Games:**
- Playable applications with more involved game-state logic
- Usually split across multiple files (scene, input, systems, content helpers)
- Live in `games/`
- Use `games/GameBase.h`
- Support input-script driven smoke tests via `vde.toml`

**Tools:**
- Authoring workflows, editors, REPLs, or content pipeline utilities
- Use `tools/ToolBase.h`

If the artifact is meant to be played, put it in `games/`. If it is meant to demonstrate an API, keep it in `examples/`. If it is meant to author or inspect content, use `tools/`.

## Scaffold Script

Always start a new game with the scaffold script — it creates all required files and wires everything into the build and smoke systems automatically:

```powershell
.\scripts\new-game.ps1 -Name my_game
# Optional overrides:
.\scripts\new-game.ps1 -Name my_game -Title "My Game" -Sections entity,input,physics
```

Or run the VS Code task: **Terminal → Run Task → scripts: new-game** (prompts for the name).

The script generates:
- `games/my_game/main.cpp` — entry point using `runGame(...)`
- `games/my_game/MyGameScene.h` / `.cpp` — scene stub with `update`, metadata methods
- `games/my_game/Input.h` — input handler with `KeyStateTracker`
- `games/my_game/CMakeLists.txt` — uses `add_vde_game(...)`
- `games/my_game/vde.toml` — smoke metadata
- `games/my_game/README.md`
- `smoketests/scripts/smoke_my_game.vdescript`
- Appends `add_subdirectory(my_game)` to `games/CMakeLists.txt`

After running the script, implement the game by editing the generated scene and input files.

## Standard Game Structure

```text
games/
  GameBase.h
  my_game/
    CMakeLists.txt
    main.cpp
    MyGameScene.h
    MyGameScene.cpp
    Input.h
    README.md
    vde.toml
```

## CMake integration

- Register the category in root `CMakeLists.txt` with `VDE_BUILD_GAMES` and `add_subdirectory(games)`
- Add each game in `games/CMakeLists.txt`
- Use `add_vde_game(...)` inside the per-game `CMakeLists.txt`

## Entry point pattern

```cpp
#include "../GameBase.h"
#include "MyGameScene.h"
#include "Input.h"

class MyGame : public vde::games::BaseGame<MyInputHandler, MyGameScene> {};

int main(int argc, char** argv) {
    MyGame game;
    return vde::games::runGame(game, "VDE My Game", 1280, 720, argc, argv);
}
```

## Smoke support

Games use the same input-script system as examples.

Requirements:

- Use `runGame(...)` so `--input-script` works automatically
- Add `games/<game_name>/vde.toml`
- Add at least one `smoketests/scripts/smoke_<name>.vdescript`

Example metadata:

```toml
[smoke]
scripts = ["smoke_my_game.vdescript"]
priority = 2
sections = ["entity", "input", "text"]
```

## Verification

After adding or changing a game:

1. Build with the repo build script.
2. Run unit tests.
3. Run smoke tests for the changed game at minimum.
4. Run the required subagent review before declaring completion.

Follow `adding-features`, `smoke-testing`, `ai-verification`, and `completing-work` for the exact workflow.