# Parallax Scrolling Demo

This example shows how to build a convincing side-scrolling scene in VDE without using a tile map or a large background texture. The whole landscape is made from many simple `SpriteEntity` objects, and each group of sprites moves at a different speed to create the illusion of depth.

If you are new to parallax scrolling, the short version is this:

- Things that are far away move slowly.
- Things that are close to the camera move quickly.
- Repeating those layers in a loop makes the world feel endless.

The implementation for this example is in [main.cpp](main.cpp). It also relies on the shared example framework in [../ExampleBase.h](../ExampleBase.h).

## What You See On Screen

When the demo runs, you should see several visual layers stacked from back to front:

1. A huge rotating day-and-night sky backdrop.
2. A cloud layer drifting slowly.
3. A mountain range moving a little faster.
4. A lake with shimmering highlights.
5. Foreground foliage with bushes and trees that sway.
6. A road layer moving the fastest.

That ordering is the heart of the effect. The background changes slowly, while the foreground moves more aggressively, so your brain reads the scene as having depth even though it is a 2D composition.

## The Big Idea

The demo uses three simple rules.

### 1. Every layer has its own speed

Each layer stores a speed value in `LayerState`. The road moves fastest, the clouds move slowly, and the sky uses its speed to control rotation instead of horizontal scrolling.

### 2. Every layer is built from repeated pieces

The scene is not one giant image. It is a collection of many sprite pieces. The code creates enough copies of each layer so that when one copy scrolls off-screen, another copy is already visible.

### 3. Small animations make the scene feel alive

Clouds bob gently, water highlights shimmer, stars twinkle, and trees sway. Those motions come from sine and cosine waves described by `MotionSpec`.

## How The Program Starts

The startup path is intentionally small.

1. `main()` creates `ParallaxDemoGame`.
2. `main()` calls `vde::examples::runExample(...)`.
3. `runExample(...)` applies standard window settings and starts the game loop.
4. `BaseExampleGame` creates the input handler and the scene.
5. `ParallaxScene::onEnter()` builds the world.

That means most of the real work happens in the scene class, not in `main()`.

## The Main Types To Understand

If you are reading the source for the first time, these are the important data structures.

### `LayerId`

This enum names the logical layers:

- `Sky`
- `Clouds`
- `Mountains`
- `Lake`
- `Foliage`
- `Road`

Using an enum makes the code easier to read than passing raw indexes around.

### `LayerState`

This struct stores the scrolling state for one layer.

- `name`: label shown in the debug UI
- `speed`: current movement speed
- `defaultSpeed`: value used when reset is pressed
- `segmentWidth`: width of one repeating chunk
- `offset`: current scroll offset

This is the data that makes the parallax effect work frame to frame.

### `MotionSpec`

This struct describes a reusable animation pattern.

It can animate:

- horizontal movement
- vertical movement
- rotation
- width and height pulsing
- color pulsing

Instead of hand-writing a custom update rule for every cloud, tree, or shimmer streak, the demo stores a motion recipe in `MotionSpec` and applies it automatically.

### `ParallaxPiece`

This struct represents one visible sprite in the world.

It stores:

- which `SpriteEntity` belongs to the piece
- which layer the piece belongs to
- whether it is part of a wrapped scrolling layer or the rotating backdrop
- its local position and size
- its base color
- its optional motion settings

You can think of `ParallaxPiece` as the bridge between the logical scene description and the actual entity that gets rendered.

## How The Scene Is Built

Inside `ParallaxScene::onEnter()`, the example does four important things.

### 1. Set up a 2D view

The scene calls `setup2D(...)` with a fixed view width and height. That gives the example a clean 2D camera setup and a dark background color.

### 2. Cache DPI scale for UI

The demo reads the game DPI scale so its ImGui debug window can stay readable on high-DPI displays.

### 3. Reserve space for sprite pieces

`m_pieces.reserve(900)` avoids repeated vector reallocations while the scene is being assembled.

### 4. Create each layer

The actual scene is built by calling these helper functions in order:

- `createSkyLayer()`
- `createCloudLayer()`
- `createMountainLayer()`
- `createLakeLayer()`
- `createFoliageLayer()`
- `createRoadLayer()`

Each helper creates many colored `SpriteEntity` objects and stores matching metadata in `m_pieces`.

## Why There Are No Texture Files

One useful thing to notice is that this demo does not load artwork from disk. It builds everything from colored sprites with different sizes, positions, rotations, and alpha values.

That is why the example is a good teaching tool:

- you can focus on the scrolling logic
- you can see how layering works without an art pipeline
- you can tweak values directly in code and immediately understand the effect

In other words, the example is demonstrating scene construction and motion, not asset management.

## How Infinite Scrolling Works

The scrolling illusion comes from `advanceLayer(...)` and `applyPiece(...)`.

### Step 1: Move the layer offset

Every frame, the code subtracts this amount from the layer offset:

```text
offset -= speed * deltaTime
```

That moves the whole layer left over time.

### Step 2: Wrap when a segment is fully off-screen

When the offset becomes more negative than one full segment width, the code adds the segment width back.

```text
while (offset <= -segmentWidth) {
    offset += segmentWidth;
}
```

This is the wraparound step. It keeps the layer moving forever without the offset growing to an extreme value.

### Step 3: Draw repeated segments around the camera

Each piece belongs to a `segment` number. During placement, the final x-position becomes:

```text
finalX = localX + segment * segmentWidth + offset
```

That formula means the same piece can appear in many repeated copies across the world.

### Why `getSegmentRadius(...)` matters

Some layers are narrower than the visible camera width. In those cases, the demo creates extra repeated copies so you never see a gap between loops.

## How The Day And Night Backdrop Works

The sky layer is different from the others.

Most layers scroll horizontally. The sky does not. Instead, the demo builds a very large circular-style composition out of sprite pieces and rotates it slowly.

### What is in the rotating backdrop

`createSkyLayer()` adds:

- a bright daytime half
- a darker nighttime half
- glow bands and gradients
- the sun
- the moon
- stars

All of those pieces are added with `addRotatingBackdropPiece(...)`.

### How rotation is calculated

The backdrop angle is based on playback time:

```text
rotationDegrees = playbackTime * skySpeed * 360 / cycleDuration
```

Because the cycle duration is long, the transition feels gradual instead of abrupt.

### How daylight affects the rest of the scene

The method `getBackdropDaylight()` converts the current sky rotation into a value between 0 and 1. The rest of the scene uses that value to darken clouds, mountains, water, foliage, and the road at night.

That is an important design choice. The demo does not only rotate the sky. It also shifts the colors of the foreground so the whole scene feels connected.

## How Small Motions Are Added

Every `ParallaxPiece` can optionally have a `MotionSpec`.

During `applyPiece(...)`, the code evaluates a sine wave and a cosine wave using the current playback time. Those values are used to adjust:

- x-position
- y-position
- width
- height
- roll
- color blending

This is why the demo feels more natural than a rigid sliding background.

### Examples

- Clouds drift and gently bob.
- Lake highlights pulse and shimmer.
- Trees and bushes sway.
- Stars twinkle.
- The sun and moon glow slightly.

The nice part of this design is that the update code stays centralized. The scene does not need a custom animation loop for every object type.

## How Input Works

The input class is `ParallaxInputHandler`, which extends `BaseExampleInputHandler`.

It keeps the standard example controls from the base class and adds a few demo-specific actions.

### Base controls from `ExampleBase`

- `ESC` exits early
- `F` marks the example as failed
- `F11` toggles fullscreen
- `F1` toggles the debug UI

### Extra controls added by this demo

- `SPACE` or `P`: pause or resume playback
- `UP`, `]`, or `=`: speed up playback
- `DOWN`, `[` or `-`: slow down playback
- `R`: reset speeds to defaults

The input handler does not directly change the scene. It stores one-frame button flags, and `ParallaxScene::handlePlaybackControls()` consumes those flags during `update()`.

That is a clean pattern for beginners to copy because it separates event capture from game logic.

## What Happens Every Frame

The frame update in `ParallaxScene::update(float deltaTime)` is straightforward.

1. Call the base scene update so standard example behavior still works.
2. Read input and update pause or speed state.
3. If playback is not paused, advance the timer and move each scrolling layer.
4. Re-apply transforms and colors to every piece.

That last step is important. The demo does not move entities only when something changes. It recomputes their current placement every frame based on the latest time, layer offsets, and motion settings.

## Why `applyAllPieces()` Is Central

`applyAllPieces()` loops over every stored piece and calls `applyPiece(...)`.

`applyPiece(...)` is where the final rendered values are decided.

It combines:

- the piece's base position
- the layer scroll offset
- the repeating segment index
- optional wave-based motion
- backdrop rotation if the piece belongs to the sky
- night-time color adjustments

After that, it writes the final values into the actual `SpriteEntity` by calling:

- `setPosition(...)`
- `setScale(...)`
- `setRotation(...)`
- `setColor(...)`

This means the scene data is declarative. The stored piece describes what an object is, and `applyPiece(...)` decides where it should appear right now.

## The Debug UI

The demo overrides `drawDebugUI()` to show an ImGui panel named `Parallax Controls`.

That panel lets you:

- pause scrolling
- speed up or slow down the whole scene
- reset to defaults
- tune the speed of each individual layer
- see FPS, frame count, playback time, and current state

For a novice, this is one of the best parts of the example because you can change values live and immediately understand how parallax depends on relative motion.

## A Good Reading Order For The Source

If the file looks large at first, read it in this order:

1. `main()`
2. `ParallaxDemoGame`
3. `ParallaxScene::onEnter()`
4. `ParallaxScene::update()`
5. `advanceLayer(...)`
6. `applyPiece(...)`
7. The layer creation helpers

That order lets you understand the flow before diving into the large blocks of scene-building data.

## What This Example Is Teaching

This example is really teaching four techniques at once.

1. How to build a scene from many simple sprites.
2. How to create parallax depth with independent layer speeds.
3. How to loop a scrolling world without visible seams.
4. How to add life with small procedural animations.

If you understand those four ideas, you understand the core of the demo.

## Ways To Experiment

If you want to learn by changing the code, these are good beginner experiments.

### Make the effect more dramatic

Increase the road speed and decrease the cloud speed.

### Make the scene calmer

Reduce the motion amplitudes in the foliage and lake shimmer specs.

### Change the mood

Edit the colors in `createSkyLayer()` and `createLakeLayer()`.

### Make the loop tighter or wider

Change a layer's `segmentWidth` in `makeDefaultLayers()`.

### Break it on purpose to understand it

Temporarily remove the wrapping logic in `advanceLayer(...)` and watch what happens when the background scrolls too far.

That kind of experiment is useful because parallax math becomes much easier to understand once you see the failure mode.

## Running The Example

From the repository root, use the project scripts:

```powershell
.\scripts\build.ps1
```

Then run the example from your generated build output, or launch it through the project launcher script:

```powershell
.\scripts\run-vlauncher.ps1
```

The example is also registered for smoke testing through [vde.toml](vde.toml).

## Final Takeaway

The parallax demo is not built around a complex engine feature. It is built around a strong scene organization pattern.

- Define layers.
- Give each layer its own speed.
- Repeat each layer across segments.
- Recompute piece transforms every frame.
- Add subtle motion to avoid stiffness.

That combination is enough to create a scene that feels deep, animated, and continuous.