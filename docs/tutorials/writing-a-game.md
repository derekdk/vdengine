# Writing a Game in VDE

This tutorial walks you through creating a complete 2D game using the VDE Game API. By the end
you will have a runnable game that participates in the project's build, smoke-test, and launcher
workflows.

The tutorial uses a simple top-down arcade game as its running example. The same steps apply to
any game you want to build.

---

## Prerequisites

- The VDE project built at least once with `.\scripts\build.ps1`
- A working C++ toolchain (MSVC or Clang on Windows)
- Basic familiarity with C++17

---

## 1. Overview — What a VDE Game Is

A **game** in VDE is a self-contained executable that lives under `games/`. It differs from an
**example** (which demonstrates a single API feature) by having real game-state logic and multiple
interacting systems. It uses `games/GameBase.h` rather than the example base.

Every game is made of three pieces:

| Piece | File | Role |
|-------|------|------|
| Input handler | `Input.h` | Translates raw key/mouse events into named actions |
| Scene | `*Scene.h/.cpp` | Owns all entities, runs game logic each frame |
| Entry point | `main.cpp` | Wires the two together and calls `runGame()` |

---

## 2. Scaffold the Game

Always start from the scaffold script — it creates every required file and registers the game
with the build system automatically.

```powershell
.\scripts\new-game.ps1 -Name my_game -Title "My Game"
```

This generates:

```
games/my_game/
  Input.h
  MyGameScene.h
  MyGameScene.cpp
  main.cpp
  CMakeLists.txt
  vde.toml
  README.md
smoketests/scripts/
  smoke_my_game.vdescript
```

It also appends `add_subdirectory(my_game)` to `games/CMakeLists.txt`.

---

## 3. Understand the Entry Point

Open `games/my_game/main.cpp`. It will look like this:

```cpp
#include "../GameBase.h"
#include "MyGameScene.h"
#include "Input.h"

namespace mygame {

class MyGameGame : public vde::games::BaseGame<MyGameInput, MyGameScene> {};

}  // namespace mygame

int main(int argc, char** argv) {
    mygame::MyGameGame game;
    return vde::games::runGame(game, "VDE My Game", 1280, 720, argc, argv);
}
```

`runGame()` handles:
- Setting the working directory to the executable's directory
- Parsing `--input-script` from the command line (for smoke tests)
- Initialising the window and Vulkan renderer
- Returning a POSIX exit code (`0` = success)

You rarely need to modify `main.cpp`.

---

## 4. Define Input Actions

Open `Input.h`. The scaffold gives you a `BaseGameInputHandler` subclass with a
`KeyStateTracker` member called `keys`.

Add your game's actions in the constructor using two binding types:

| Method | When to use |
|--------|-------------|
| `keys.bindHeld(key, "action")` | Continuous input — moving a character, holding a button |
| `keys.bindOneShot(key, "action")` | Single-press input — firing, pausing, selecting |

Multiple keys can map to the same action (e.g., WASD and arrow keys):

```cpp
PacManInput() {
    // Held — player holds a direction
    keys.bindHeld(vde::KEY_LEFT,  "left");
    keys.bindHeld(vde::KEY_A,     "left");
    keys.bindHeld(vde::KEY_RIGHT, "right");
    keys.bindHeld(vde::KEY_D,     "right");
    keys.bindHeld(vde::KEY_UP,    "up");
    keys.bindHeld(vde::KEY_W,     "up");
    keys.bindHeld(vde::KEY_DOWN,  "down");
    keys.bindHeld(vde::KEY_S,     "down");

    // One-shot — restart after game over
    keys.bindOneShot(vde::KEY_SPACE, "restart");
}

void onKeyPress(int key) override {
    BaseGameInputHandler::onKeyPress(key);  // handles ESC / F11 / F1
    keys.handlePress(key);
}

void onKeyRelease(int key) override {
    keys.handleRelease(key);
}
```

Query actions in `update()`:

```cpp
if (input->keys.isHeld("left"))    { /* continuous movement */ }
if (input->keys.consume("restart")) { /* one-shot action, cleared after read */ }
```

---

## 5. Build the Scene

### 5.1 Class declaration

Your scene inherits from `vde::games::BaseGameScene`, which already handles ESC-to-quit,
F11 fullscreen, and F1 debug UI. You override four lifecycle methods and four metadata methods:

```cpp
class MyGameScene : public vde::games::BaseGameScene {
public:
    void onEnter() override;          // called once when scene becomes active
    void update(float dt) override;   // called every frame

protected:
    // Metadata shown in the launcher and debug header
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;
};
```

### 5.2 onEnter — set up the scene

`onEnter()` is where you create the camera, lighting, entities, and initial game state.

For a 2D game, use the `setup2D()` helper:

```cpp
void MyGameScene::onEnter() {
    printGameHeader();   // prints the metadata to stdout

    // Sets up Camera2D, white ambient light, and a background colour in one call.
    // Arguments are the visible width and height in world units.
    setup2D(21.0f, 17.0f, vde::Color(0.0f, 0.0f, 0.0f));

    m_input = dynamic_cast<MyGameInput*>(getInputHandler());

    createLevel();
    createHud();
}
```

### 5.3 Entities

All entities are owned by the scene and created with `addEntity<T>()`. Store the returned
`shared_ptr` or just the entity's `EntityId` for later access.

**SpriteEntity** — a coloured rectangle (or textured quad):

```cpp
auto wall = addEntity<vde::SpriteEntity>();
wall->setPosition(x, y, 0.0f);
wall->setScale(1.0f, 1.0f, 1.0f);   // width, height, depth
wall->setColor(vde::Color(0.2f, 0.2f, 0.8f));
```

To use a texture:

```cpp
auto& rm = getGame()->getResourceManager();
auto tex  = rm.load<vde::Texture>("assets/textures/sprite.png");
if (auto* ctx = getGame()->getVulkanContext()) {
    tex->uploadToGPU(ctx);
}
sprite->setTexture(tex);
```

**TextEntity** — bitmap font text for HUD elements:

```cpp
auto label = addEntity<vde::TextEntity>();
label->setFont(vde::BitmapFont::large());
label->setText("Score: 0");
label->setWorldHeight(0.5f);         // text height in world units
label->setPosition(-9.0f, 8.0f, 0.0f);
label->setColor(vde::Color::white());
```

### 5.4 Coordinate system

`Camera2D(width, height)` maps world coordinates so that (0, 0) is the centre of the screen.
Positive Y is **up**. A typical 21×17 view gives:

- Left edge  ≈ −10.5
- Right edge ≈  10.5
- Top edge   ≈   8.5
- Bottom edge ≈ −8.5

### 5.5 update() — game logic

`update(float deltaTime)` is called every frame. Always call the base class first so that
fullscreen / ESC handling runs:

```cpp
void MyGameScene::update(float deltaTime) {
    BaseGameScene::update(deltaTime);  // MUST call this

    if (!m_input) return;

    updatePlayer(deltaTime);
    updateEnemies(deltaTime);
    checkCollisions();
    updateHud();
}
```

`deltaTime` is elapsed seconds since the last frame (typically ~0.016 at 60 fps). Multiply
velocities by `deltaTime` to get frame-rate-independent movement:

```cpp
m_x += speed * dirX * deltaTime;
```

---

## 6. Game State

Use an enum class to track game phases:

```cpp
enum class GameState { Playing, GameOver, Win };
GameState m_state = GameState::Playing;
```

Check state in `update()` and branch accordingly:

```cpp
switch (m_state) {
    case GameState::Playing:
        updateGameplay(deltaTime);
        break;
    case GameState::GameOver:
        if (m_input->keys.consume("restart")) resetGame();
        break;
    case GameState::Win:
        if (m_input->keys.consume("restart")) resetGame();
        break;
}
```

---

## 7. Collision Detection

VDE provides a 2D physics engine, but for grid-based or simple arcade games, AABB (axis-aligned
bounding box) checks in plain C++ are often simpler and faster:

```cpp
static bool overlapsAabb(float ax, float ay, float aw, float ah,
                         float bx, float by, float bw, float bh) {
    return ax + aw * 0.5f >= bx - bw * 0.5f &&
           ax - aw * 0.5f <= bx + bw * 0.5f &&
           ay + ah * 0.5f >= by - bh * 0.5f &&
           ay - ah * 0.5f <= by + bh * 0.5f;
}
```

For grid-based games, tile-coordinate checks are even simpler — just compare integer column and
row values rather than floating-point positions.

---

## 8. Build the Game

After implementing your scene, build the project:

```powershell
.\scripts\build.ps1
```

On success the executable lands in `build_ninja\games\my_game\vde_my_game.exe`.

Run it directly to verify it works:

```powershell
.\build_ninja\games\my_game\vde_my_game.exe
```

---

## 9. The vde.toml Metadata File

`games/my_game/vde.toml` describes your game to the build and smoke-test infrastructure:

```toml
[smoke]
scripts   = ["smoke_my_game.vdescript"]
priority  = 2
sections  = ["entity", "input", "text"]
```

`sections` lists the VDE subsystems your game exercises (`entity`, `input`, `text`, `physics`,
`audio`, etc.). Update it to match what your game actually uses.

---

## 10. CMakeLists.txt

The scaffold generates this for you. It looks like:

```cmake
add_vde_game(vde_my_game
    main.cpp
    MyGameScene.cpp
)
```

If you add more `.cpp` files, append them to this list. Header-only files do not need to be
listed.

---

## 11. README.md

Each game has a `README.md` that the launcher can display. Fill in:

- A one-line description
- Controls
- Rules / scoring
- Any known limitations

---

## Full Example Walkthrough — A Minimal Moving Sprite

Here is the smallest possible game that satisfies all the patterns described above:

**Input.h**

```cpp
#pragma once
#include "../GameBase.h"

namespace mover {

class MoverInput : public vde::games::BaseGameInputHandler {
public:
    MoverInput() {
        keys.bindHeld(vde::KEY_LEFT,  "left");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindHeld(vde::KEY_UP,    "up");
        keys.bindHeld(vde::KEY_DOWN,  "down");
    }
    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }
    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace mover
```

**MoverScene.h**

```cpp
#pragma once
#include <memory>
#include "../GameBase.h"

namespace mover {
class MoverInput;

class MoverScene : public vde::games::BaseGameScene {
public:
    void onEnter() override;
    void update(float dt) override;

protected:
    std::string getGameName() const override { return "Mover"; }
    std::vector<std::string> getGameplaySummary() const override {
        return {"Move the box with arrow keys."};
    }
    std::vector<std::string> getGoals() const override { return {"Stay on screen."}; }
    std::vector<std::string> getControls() const override {
        return {"Arrow keys — move"};
    }

private:
    static constexpr float kSpeed = 5.0f;
    std::shared_ptr<vde::SpriteEntity> m_box;
    float m_x = 0.0f, m_y = 0.0f;
    MoverInput* m_input = nullptr;
};
}  // namespace mover
```

**MoverScene.cpp**

```cpp
#include "MoverScene.h"
#include "Input.h"
#include <algorithm>

namespace mover {
using namespace vde;

void MoverScene::onEnter() {
    printGameHeader();
    setup2D(20.0f, 15.0f, Color(0.05f, 0.05f, 0.1f));
    m_input = dynamic_cast<MoverInput*>(getInputHandler());

    m_box = addEntity<SpriteEntity>();
    m_box->setScale(1.0f, 1.0f, 1.0f);
    m_box->setColor(Color(0.2f, 0.8f, 0.4f));
}

void MoverScene::update(float dt) {
    BaseGameScene::update(dt);
    if (!m_input) return;

    float dx = 0.0f, dy = 0.0f;
    if (m_input->keys.isHeld("left"))  dx -= 1.0f;
    if (m_input->keys.isHeld("right")) dx += 1.0f;
    if (m_input->keys.isHeld("up"))    dy += 1.0f;
    if (m_input->keys.isHeld("down"))  dy -= 1.0f;

    m_x = std::clamp(m_x + dx * kSpeed * dt, -9.0f, 9.0f);
    m_y = std::clamp(m_y + dy * kSpeed * dt, -6.5f, 6.5f);

    if (m_box) m_box->setPosition(m_x, m_y, 0.0f);
}
}  // namespace mover
```

**main.cpp**

```cpp
#include "../GameBase.h"
#include "MoverScene.h"
#include "Input.h"

namespace mover {
class MoverGame : public vde::games::BaseGame<MoverInput, MoverScene> {};
}

int main(int argc, char** argv) {
    mover::MoverGame game;
    return vde::games::runGame(game, "VDE Mover", 1280, 720, argc, argv);
}
```

---

## Summary

| Step | Command / Action |
|------|-----------------|
| Scaffold | `.\scripts\new-game.ps1 -Name my_game` |
| Edit input | `games/my_game/Input.h` — bind keys |
| Edit scene | `games/my_game/MyGameScene.cpp` — game logic |
| Build | `.\scripts\build.ps1` |
| Run | `.\build_ninja\games\my_game\vde_my_game.exe` |

## See Also

- [API-DOC.md](../../API-DOC.md) — Full VDE API reference
- [games/pong/](../../games/pong/) — A complete worked example
- [games/fishing_game/](../../games/fishing_game/) — A more complex worked example
- [docs/tutorials/writing-a-smoke-test.md](writing-a-smoke-test.md) — Adding automated tests
