# 2D Sidescroller Example

A complete 2D platformer game demonstrating VDE's capabilities for sidescroller/platformer games.

## Features Demonstrated

- **Player Character**: Controllable character with simulated sprite animation
- **Physics System**: Basic 2D physics with gravity, jumping, and friction
- **Platform Collision**: Simple collision detection with platforms
- **Enemy AI**: Patrolling enemies with basic behavior
- **Camera Following**: Smooth camera that tracks the player
- **Layered Backgrounds**: Multiple depth layers for parallax-style backgrounds

## Controls

- **A/D or Left/Right Arrows** - Move player left/right
- **Space/W/Up Arrow** - Jump
- **ESC** - Exit
- **F** - Fail test (for testing purposes)

## Implementation Highlights

### Player Entity
- Custom `PlayerEntity` class with 2D physics
- Simulated animation using UV rectangles
- Jump mechanics with ground detection
- Horizontal movement with acceleration and friction

### Camera System
- Uses `Camera2D` for orthographic 2D view
- Smooth lerp-based following
- Centered around player position

### Platforms
- Custom `PlatformEntity` with bounding boxes
- Basic top-collision detection
- Positioned at various heights for gameplay variety

### Enemies
- `EnemyEntity` with patrol AI
- Automatic turnaround at patrol boundaries
- Visual sprite flipping (simulated)

## Code Structure

The example uses:
- `AnimatedSpriteEntity` - Base class for sprites with frame animation
- `PlayerEntity` - Main player character
- `PlatformEntity` - Collidable platform objects
- `EnemyEntity` - Patrolling enemy objects
- `SidescrollerScene` - Main game scene
- `SidescrollerInputHandler` - Keyboard input handling

## Running the Example

```bash
# From build directory
cmake --build . --target vde_sidescroller --config Debug

# Run
.\examples\Debug\vde_sidescroller.exe
```

## Limitations & Future Improvements

This example works with the current VDE API but would benefit from additional features. See [SIDESCROLLER_API_SUGGESTIONS.md](../../SIDESCROLLER_API_SUGGESTIONS.md) for detailed proposals including:

1. **SpriteSheet Management** - Helper classes for loading and managing sprite atlases
2. **Sprite Animation System** - Built-in animation state machine
3. **TileMap Support** - Efficient tile-based background rendering
4. **Sprite Flipping** - Native horizontal/vertical sprite flipping
5. **2D Collision Helpers** - AABB and circle collision utilities
6. **Enhanced Camera2D** - Camera bounds, deadzone, shake effects
7. **Particle System** - 2D particle effects for polish

## Current Workarounds

Without the suggested API improvements, this example uses:

- **Manual UV Calculation** - Simulates spritesheet frames manually
- **Individual Sprites for Background** - Creates many sprite entities instead of using a tilemap
- **Basic Physics** - Simple gravity/velocity simulation instead of a physics engine
- **Simplified Collision** - Basic AABB checks instead of a proper collision system
- **Simulated Flipping** - Notes where sprite flipping would be used

These workarounds demonstrate what's currently possible but also highlight where the API could be improved for better 2D game development workflows.
