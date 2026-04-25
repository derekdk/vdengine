# Pong

Pong is a compact arcade game built as a first-class VDE game. It uses a 2D court, sprite paddles, a text HUD, and a simple CPU opponent so the project has a playable game target beyond the feature demos in `examples/`.

## How to play

Serve the ball from center court, defend the left paddle, and send the ball past the CPU paddle on the right. The match is first to five points, and each point resets the court for the next serve.

## Controls

| Key | Action |
|-----|--------|
| W / Up | Move paddle up |
| S / Down | Move paddle down |
| Space | Serve ball / start next round / rematch |
| R | Restart match |
| ESC | Exit |
| F1  | Toggle debug UI |
| F11 | Toggle fullscreen |

## Building

```
.\scripts\build.ps1
```

## Running

```
.\scripts\run-vlauncher.ps1
```

Select **Pong** in the launcher, or run `vde_pong` directly from the build output directory.
