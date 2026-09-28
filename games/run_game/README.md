# RunGame

A small endless runner inspired by the Parallax demo. Jump over obstacles while a
procedurally assembled landscape scrolls at several independent speeds.

The scene, player, obstacle course, and each parallax layer are implemented in
separate files. Every landscape layer updates its own motion independently.

## How to play

Stay on the road, jump over the obstacles, and run as far as you can. The scenery
loops continuously and the running speed gradually increases.

## Controls

| Key | Action |
|-----|--------|
| SPACE / UP / W | Jump |
| R | Restart |
| ESC | Exit |
| F1  | Toggle debug UI |

## Building

```
.\scripts\build.ps1
```

## Running

```
.\scripts\run-vlauncher.ps1
```

Select **RunGame** in the launcher, or run `vde_run_game` directly from the build output directory.
