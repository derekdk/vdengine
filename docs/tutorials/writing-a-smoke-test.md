# Writing a Smoke Test for a VDE Game

This tutorial walks you through creating a scripted smoke test for a VDE game. Smoke tests are
lightweight automated checks that verify your game launches, responds to input, and exits cleanly
— without requiring human interaction. They run in CI and with the project's `smoke-test.ps1`
script.

---

## Prerequisites

- A working VDE game (follow [writing-a-game.md](writing-a-game.md) first)
- The game built: `.\scripts\build.ps1`

---

## 1. What Is a Smoke Test?

A smoke test in VDE is a `.vdescript` file that drives your game executable with a sequence of
timed commands — key presses, waits, and an `exit`. The test passes when the executable exits
with code `0`. It fails when the executable crashes, hangs, or exits with a non-zero code.

Smoke tests are **not** unit tests. They do not assert specific pixel values or game states (that
is the role of render-verify tests). A smoke test answers a single question:

> **Does the game start, accept input, and exit cleanly?**

---

## 2. How Scripts Work

VDE's input script system reads a `.vdescript` file and replays the commands at the right times.
Every game built with `runGame()` accepts a script path via the `--input-script` flag:

```powershell
.\build_ninja\games\my_game\vde_my_game.exe --input-script smoketests\scripts\smoke_my_game.vdescript
```

The script runs inside the normal game loop — commands are dispatched through the same input
system as a real keyboard, so they trigger the same code paths.

---

## 3. Script Format

Scripts are plain text files with one command per line. Lines starting with `#` or `//` are
comments. Empty lines are ignored.

### Essential commands

| Command | Description |
|---------|-------------|
| `wait startup` | Wait for the first frame to render before doing anything |
| `wait <ms>` | Wait N milliseconds (e.g. `wait 500`) |
| `wait <N>s` | Wait N seconds (e.g. `wait 2s`) |
| `press <KEY>` | Tap a key once (one-shot) |
| `keydown <KEY>` | Hold a key down |
| `keyup <KEY>` | Release a held key |
| `hold <KEY> <duration>` | Press, hold for duration, then release |
| `exit` | Quit the application |

Key names are uppercase: `SPACE`, `ENTER`, `ESC`, `LEFT`, `RIGHT`, `UP`, `DOWN`, `A`–`Z`, `0`–`9`.

### Minimal smoke test

```vdescript
# smoke_my_game.vdescript - minimal smoke test
wait startup
wait 100
exit
```

This is enough to confirm the game launches and that the render loop runs at least once.

---

## 4. Where to Put the Script

Smoke scripts live in `smoketests/scripts/` and follow the naming convention
`smoke_<game_name>.vdescript`. The scaffold creates one automatically.

```
smoketests/
  scripts/
    smoke_pac_man.vdescript
    smoke_pong.vdescript
    ...
```

---

## 5. Register the Script in vde.toml

Your game's `vde.toml` tells the smoke runner which scripts to execute:

```toml
[smoke]
scripts   = ["smoke_my_game.vdescript"]
priority  = 2
sections  = ["entity", "input", "text"]
```

- **scripts** — list of `.vdescript` filenames to run (relative to `smoketests/scripts/`)
- **priority** — lower number runs first (1 = highest priority)
- **sections** — VDE subsystems exercised (`entity`, `input`, `text`, `physics`, `audio`, …)

The scaffold sets this up for you. Review and update `sections` to match what your game actually
uses.

---

## 6. Write a Meaningful Smoke Test

A good smoke test goes beyond "start and exit" — it exercises the main input paths your game
supports. Think of it as a one-minute automated QA pass.

### Typical structure

```vdescript
# smoke_my_game.vdescript
wait startup       # Always start with this
wait 200           # Brief pause for any deferred initialisation

# --- Exercise core gameplay ---
keydown RIGHT      # Start moving
wait 500
keyup RIGHT

keydown DOWN
wait 400
keyup DOWN

keydown LEFT
wait 400
keyup LEFT

# Let the game run naturally
wait 500

exit
```

### Tips

1. **Always begin with `wait startup`** — commands issued before the first frame is rendered are
   ignored.

2. **Add short waits after each input** — give the game loop time to process state changes.

3. **Cover each core action once** — movement in all directions, the primary action button, and
   the restart/pause flow if your game has one.

4. **Keep the total runtime short** — aim for under 5 seconds. Long smoke tests slow down the
   full suite.

5. **Use `keydown` + `keyup` pairs for held input** — `press` is a single-frame tap; movement
   usually needs `keydown`/`keyup` to travel any meaningful distance.

---

## 7. Run the Smoke Test Manually

Run the executable directly with `--input-script`:

```powershell
.\build_ninja\games\my_game\vde_my_game.exe --input-script smoketests\scripts\smoke_my_game.vdescript
```

Check the exit code:

```powershell
echo "Exit code: $LASTEXITCODE"
```

`0` = pass. Anything else = fail.

You can also watch the output — the script runner prints each command as it executes:

```
[VDE:InputScript] Loaded 12 commands from smoke_my_game.vdescript
[VDE:InputScript] exit
```

---

## 8. Run All Smoke Tests

Use the project's smoke-test runner to execute every registered smoke test:

```powershell
.\scripts\smoke-test.ps1
```

To run only your game's tests:

```powershell
.\scripts\smoke-test.ps1 -Filter pac_man
```

The runner reports pass/fail for each executable and prints a summary table.

---

## 9. Full Example — Pac-Man Smoke Test

Here is the complete smoke test for the Pac-Man game developed in the companion tutorial:

```vdescript
# smoke_pac_man.vdescript
# Verify the game starts, Pac-Man moves, and direction changes work.
wait startup
wait 200

# Move right (default direction at start)
keydown RIGHT
wait 500
keyup RIGHT

# Turn down
keydown DOWN
wait 400
keyup DOWN

# Turn left
keydown LEFT
wait 400
keyup LEFT

# Turn up
keydown UP
wait 400
keyup UP

# Let the game run for a moment, then exit
wait 500
exit
```

### What this tests

- The game window opens and the first frame renders (`wait startup`)
- The input handler binds directional keys correctly
- `keydown`/`keyup` events are delivered to the `KeyStateTracker`
- The game loop runs for several seconds without crashing
- The `exit` command terminates the process cleanly (exit code 0)

---

## 10. Debugging Failing Tests

### The game hangs and the script never finishes

- Confirm the game was built with `runGame()` (not a raw `game.run()` without script support)
- Confirm the script path is correct relative to where you run the command
- Look for `[VDE:InputScript] Loaded N commands` in the output — if missing, the script was not found

### The game crashes

- Run without `--input-script` first to confirm the game starts manually
- Add `wait 2s` before complex inputs to give more time for initialisation
- Check for null-pointer errors in your `onEnter()` — if entities aren't created before `update()` runs, scripted key presses may access uninitialised state

### Wrong exit code

- Non-zero exit codes usually mean the game called `std::exit()` with an error, threw an
  unhandled exception, or hit an assertion failure
- Build in Debug mode (`.\scripts\build.ps1`) and watch the console output for error messages

---

## Summary

| Step | Action |
|------|--------|
| 1 | Create `smoketests/scripts/smoke_<name>.vdescript` (scaffold does this) |
| 2 | Start with `wait startup`, end with `exit` |
| 3 | Add `keydown`/`keyup` pairs for each core input action |
| 4 | Verify `games/<name>/vde.toml` lists your script |
| 5 | Run: `.\build_ninja\games\<name>\vde_<name>.exe --input-script smoketests\scripts\smoke_<name>.vdescript` |
| 6 | Check exit code is `0` |
| 7 | Run full suite: `.\scripts\smoke-test.ps1` |

## See Also

- [docs/tutorials/writing-a-game.md](writing-a-game.md) — Creating a game to test
- [API-DOC.md](../../API-DOC.md) — Full VDE API reference
- [smoketests/scripts/](../../smoketests/scripts/) — All existing smoke scripts
