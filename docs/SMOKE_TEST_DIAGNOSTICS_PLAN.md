# Smoke Test Diagnostics Enhancement Plan

## Problem Statement

Current smoke tests can verify basic rendering state (scene rendered, entity count > 0, viewport dimensions) but lack:

1. **Entity-level diagnostics** — No way to confirm specific entity creation, query entity counts by type, or gather entity statistics from scripts
2. **Scene lifecycle diagnostics** — No way to verify scene creation events, focus changes, or scene lifecycle state from scripts
3. **Script-side comparisons** — Limited ability to perform conditional checks; `assert` only supports numeric comparisons on a fixed set of fields

## Current State

### What exists today

| Capability | How it works | Limitation |
|---|---|---|
| `assert scene "name" entities_drawn == N` | Returns `getEntities().size()` | Count only; no type breakdown, no individual entity info |
| `assert scene "name" was_rendered == true` | Checks if scene is in active group | Binary check; no lifecycle event tracking |
| `assert rendered_scene_count == N` | `getActiveSceneGroup().sceneNames.size()` | No scene creation/destruction history |
| `print <message>` | Writes to stdout with `[VDE:InputScript]` prefix | Output only; no read-back into script variables |
| `set VAR value` | Stores a double in script variable map | Variables never populated by engine; manual-set only |

### Architecture

- **ScriptEnvironment** ([ScriptEnvironment.h](include/vde/api/ScriptEnvironment.h)) — Narrow interface between executor and engine (Game implements it). Currently exposes: `getScene()`, `getActiveSceneGroup()`, `getSwapChainExtent()`, `resolveInputHandler()`, `captureScreenshot()`, `quit()`, `setExitCode()`.
- **InputScriptExecutor** ([InputScriptExecutor.cpp](src/api/InputScriptExecutor.cpp)) — Dispatch table of handlers; `handleAssertScene` resolves field values via `tryResolveAssertSceneFieldValue()` with a fixed if-else chain of field names.
- **Scene** ([Scene.h](include/vde/api/Scene.h)) — `getEntities()` returns all entities; `getEntitiesOfType<T>()` provides typed filtering. No statistics counters.
- **Entity type hierarchy**: `Entity` → `MeshEntity`, `SpriteEntity` (base visual types) → `PhysicsMeshEntity` (multiple-inherits MeshEntity + PhysicsEntity), `PhysicsSpriteEntity` (multiple-inherits SpriteEntity + PhysicsEntity), `TextEntity` (inherits SpriteEntity). Note: a `PhysicsSpriteEntity` IS-A `SpriteEntity` AND IS-A `PhysicsEntity` — type counts must account for double-counting.

## Proposed Changes

### Phase 1: Engine-Side Diagnostics Collection

#### 1A. Scene Diagnostics Struct

Add a lightweight per-scene diagnostics struct in `Scene.h` that tracks entity counts and lifecycle events:

```cpp
// In Scene.h
struct SceneDiagnostics {
    // Entity type counts (maintained incrementally, not recomputed)
    size_t totalEntityCount = 0;      // m_entities.size()
    size_t meshEntityCount = 0;       // MeshEntity + PhysicsMeshEntity
    size_t spriteEntityCount = 0;     // SpriteEntity + PhysicsSpriteEntity + TextEntity
    size_t textEntityCount = 0;       // TextEntity only
    size_t physicsEntityCount = 0;    // PhysicsMeshEntity + PhysicsSpriteEntity (PhysicsEntity mixin)
    
    // Entity lifecycle counters (cumulative since scene creation)
    size_t entitiesCreated = 0;       // incremented on every addEntity/create call
    size_t entitiesRemoved = 0;       // incremented on every removeEntity/clear call
    
    // Scene lifecycle state
    size_t enterCount = 0;            // number of times onEnter() was called
    size_t exitCount = 0;             // number of times onExit() was called
    size_t pauseCount = 0;            // number of times onPause() was called
    size_t resumeCount = 0;           // number of times onResume() was called
    bool isFocused = false;           // currently the keyboard-focused scene
};
```

**Collection strategy — O(1) per entity mutation**:

Entity type counters are maintained incrementally. On `addEntity()`, classify the new entity once and increment the relevant counters. On `removeEntity()`, classify the removed entity once and decrement. This avoids the O(n) full-vector-scan approach.

```cpp
// In Scene.cpp — called from addEntity()
void Scene::classifyAndIncrementEntity(Entity* e) {
    m_diagnostics.totalEntityCount = m_entities.size();
    m_diagnostics.entitiesCreated++;
    if (dynamic_cast<TextEntity*>(e))          m_diagnostics.textEntityCount++;
    if (dynamic_cast<PhysicsEntity*>(e))       m_diagnostics.physicsEntityCount++;
    if (dynamic_cast<MeshEntity*>(e))          m_diagnostics.meshEntityCount++;
    if (dynamic_cast<SpriteEntity*>(e))        m_diagnostics.spriteEntityCount++;
}

// In Scene.cpp — called from removeEntity()
void Scene::classifyAndDecrementEntity(Entity* e) {
    m_diagnostics.totalEntityCount = m_entities.size();
    m_diagnostics.entitiesRemoved++;
    if (dynamic_cast<TextEntity*>(e))          m_diagnostics.textEntityCount--;
    if (dynamic_cast<MeshEntity*>(e))          m_diagnostics.meshEntityCount--;
    if (dynamic_cast<SpriteEntity*>(e))        m_diagnostics.spriteEntityCount--;
    if (dynamic_cast<PhysicsEntity*>(e))       m_diagnostics.physicsEntityCount--;
}
```

**Type counting semantics**: Each entity increments every type it satisfies via `dynamic_cast`. A `PhysicsSpriteEntity` will increment both `spriteEntityCount` and `physicsEntityCount`. A `TextEntity` increments both `textEntityCount` and `spriteEntityCount`. This is consistent with IS-A semantics. Scripts that need an exclusive count can subtract (e.g., "pure sprites" = `sprite_entity_count - text_entity_count - physics_sprite_count`), but in practice smoke tests just check `> 0` or approximate counts.

**Lifecycle counters**: `onEnter()`, `onExit()`, `onPause()`, `onResume()` each increment their respective counter. This correctly handles push/pop scenarios where a scene may enter and exit multiple times.

**For `clearEntities()`**: Set all type counts to 0 and add the cleared count to `entitiesRemoved`.

**Enablement**: Always-on. The overhead is negligible (a few `dynamic_cast` checks on the single entity being added/removed, plus counter increments).

#### 1B. Scene Diagnostics Accessor

Expose diagnostics from Scene via a public getter:

```cpp
// In Scene.h
const SceneDiagnostics& getDiagnostics() const { return m_diagnostics; }
```

### Phase 2: ScriptEnvironment Extensions

Extend `ScriptEnvironment` with individual methods rather than a struct (simpler to mock, follows existing pattern):

```cpp
class ScriptEnvironment {
  public:
    // ... existing methods ...
    
    /// Get the name of the currently focused scene for keyboard input.
    virtual std::string getFocusedSceneName() const = 0;
    
    /// Get the name of the primary active scene.
    virtual std::string getActiveSceneName() const = 0;
    
    /// Get total number of scenes added to the game via addScene().
    virtual size_t getScenesCreated() const = 0;
    
    /// Get total number of scenes removed via removeScene().
    virtual size_t getScenesRemoved() const = 0;
};
```

Game implements these by reading from existing member variables (e.g., `m_focusedSceneName`, `m_scenes.size()`) plus simple counters added to Game for scenes-created/removed. The existing `getScene()` method already provides access to Scene objects and their `getDiagnostics()`.

### Phase 3: New Script Assert Fields

Extend `tryResolveAssertSceneFieldValue()` to support new per-scene fields. The function already receives a `Scene*`, so it calls `scene->getDiagnostics()` to access the new data:

| Field name | Source | Type |
|---|---|---|
| `entity_count` | `diagnostics.totalEntityCount` | int |
| `mesh_entity_count` | `diagnostics.meshEntityCount` | int |
| `sprite_entity_count` | `diagnostics.spriteEntityCount` | int |
| `text_entity_count` | `diagnostics.textEntityCount` | int |
| `physics_entity_count` | `diagnostics.physicsEntityCount` | int |
| `entities_created` | `diagnostics.entitiesCreated` | int |
| `entities_removed` | `diagnostics.entitiesRemoved` | int |
| `enter_count` | `diagnostics.enterCount` | int |
| `exit_count` | `diagnostics.exitCount` | int |
| `pause_count` | `diagnostics.pauseCount` | int |
| `resume_count` | `diagnostics.resumeCount` | int |
| `is_focused` | `diagnostics.isFocused` | bool (0/1) |

Keep the existing fields (`was_rendered`, `entities_drawn`, `draw_calls`, `viewport_width`, `viewport_height`, `not_blank`) unchanged for backward compatibility. `entities_drawn` remains as an alias for `entity_count`.

**Variable references in assertions**: Allow `$VAR_NAME` on the RHS of assert comparisons. When the parser encounters a `$` prefix on the assert value, it marks the command as having a variable reference. At execution time, the executor resolves the variable from `state.variables`. If the variable doesn't exist, the assertion fails with an error message. This allows:

```vdescript
set EXPECTED_COUNT 5
assert scene "gameplay" entity_count >= $EXPECTED_COUNT
```

Implementation: Add `std::string assertVarRef` to `ScriptCommand`. If non-empty, the executor resolves the value at runtime instead of using `assertValue`.

### Phase 4: New Global Assert Fields

Add new global-scope assert fields (alongside existing `rendered_scene_count`):

#### 4A. Numeric global fields (low risk)

```vdescript
assert scenes_created >= 3
assert scenes_removed == 0
```

These follow the existing `assert rendered_scene_count` pattern exactly — numeric value, same comparison operators. Implementation: extend the global field resolver in `handleAssertSceneCount()` (rename to `handleAssertGlobal()`), adding `scenes_created` and `scenes_removed` fields that call the new `ScriptEnvironment` methods.

#### 4B. String global fields (deferred — higher risk)

```vdescript
assert focused_scene == "gameplay"
assert active_scene == "gameplay"
```

**Deferred to a follow-up**: String comparisons require changes to the parser (quoted string RHS), a new field on `ScriptCommand` for string values, and a separate comparison code path in the executor. This is non-trivial and should be implemented after Phase 1-3 are working and tested.

**Interim workaround**: Use `is_focused` on specific scenes:
```vdescript
assert scene "gameplay" is_focused == true
```

### Phase 5: Updated Smoke Test Scripts

Update existing smoke test scripts to use the new diagnostics. Example upgrades:

**Breakout demo** (currently only tests input, no assertions):
```vdescript
# smoke_breakout.vdescript - Enhanced with entity diagnostics
wait startup
wait_frames 10

# Verify scene setup
assert scene "gameplay" enter_count > 0
assert scene "gameplay" is_focused == true
assert scene "gameplay" entity_count > 0
assert scene "gameplay" sprite_entity_count > 0
assert scene "gameplay" physics_entity_count > 0

# Play for a bit
keydown LEFT
wait 1000
keyup LEFT
keydown RIGHT
wait 1000
keyup RIGHT
wait 3s

# Entities should still exist after gameplay
assert scene "gameplay" entity_count > 0

print Breakout smoke test passed!
exit
```

**Multi-scene demo** (currently only tests input):
```vdescript
# smoke_multi_scene.vdescript - Enhanced with scene diagnostics
wait startup
wait_frames 10

# Verify initial scene
assert scene "scene1" is_focused == true
assert scene "scene1" enter_count > 0

# Switch scenes
press 1
wait 500
assert scene "scene1" is_focused == true

press 2
wait 500
assert scene "scene2" enter_count > 0

press 3
wait 500
press 4
wait 500

# Verify scene lifecycle
assert scenes_created >= 4

wait 2s
exit
```

**Four-scene 3D demo** (already has assertions, enhance with types):
```vdescript
# Existing assertions remain unchanged...
assert rendered_scene_count == 4
assert scene "crystal" was_rendered == true
assert scene "crystal" entities_drawn > 0

# Add entity type verification
assert scene "crystal" mesh_entity_count > 0
assert scene "metropolis" mesh_entity_count > 0
assert scene "nature" mesh_entity_count > 0

# Verify focus
assert scene "crystal" is_focused == true

print Enhanced scene assertions passed!
exit
```

## Implementation Order

| Step | What | Files changed | Risk |
|---|---|---|---|
| 1 | Add `SceneDiagnostics` struct + `getDiagnostics()` | `Scene.h` | Low — additive struct |
| 2 | Instrument entity add/remove with O(1) type classification | `Scene.cpp` | Low — existing code paths |
| 3 | Instrument lifecycle methods (`onEnter`/`onExit`/`onPause`/`onResume`) | `Scene.cpp` | Low — existing hooks |
| 4 | Wire `isFocused` from Game's focus tracking | `Game.cpp` | Low — existing `setFocusedScene()` |
| 5 | Add `ScriptEnvironment` methods + Game implementation | `ScriptEnvironment.h`, `Game.h`, `Game.cpp` | Medium — interface change, mock must update |
| 6 | Add new scene-level assert fields in executor | `InputScriptExecutor.cpp` | Low — extend existing if-chain |
| 7 | Add variable references (`$VAR_NAME`) in assert RHS | `InputScript.h`, `InputScript.cpp`, `InputScriptExecutor.cpp` | Medium — parser + runtime |
| 8 | Add numeric global assert fields (`scenes_created`, etc.) | `InputScript.cpp`, `InputScriptExecutor.cpp` | Low — follows existing pattern |
| 9 | Update unit tests | `tests/` | Required |
| 10 | Update smoke scripts | `smoketests/scripts/` | Low |
| 11 | Update documentation | `API-DOC.md`, skill files | Low |

## Design Decisions

1. **No runtime toggle for diagnostics** — The counters are trivially cheap (integer increments on existing code paths). Always-on simplifies the implementation and avoids conditional compilation paths.

2. **Diagnostics are read-only from scripts** — Scripts can observe but not mutate engine state. This preserves the existing safety model where scripts don't perform entity mutations.

3. **O(1) incremental type counting** — Each entity is classified once at add/remove time via `dynamic_cast` on the single entity. No full-vector rescans. A `PhysicsSpriteEntity` increments both `spriteEntityCount` and `physicsEntityCount` (IS-A semantics).

4. **Lifecycle counters instead of boolean flags** — Using `enterCount`/`exitCount`/`pauseCount`/`resumeCount` instead of `hasEntered`/`hasExited` booleans. Counters correctly handle push/pop scenarios where a scene enters and exits multiple times. "Has the scene ever been entered?" is simply `enter_count > 0`.

5. **Per-method ScriptEnvironment extension** — Individual virtual methods (`getFocusedSceneName()`, `getScenesCreated()`, etc.) instead of a `GameDiagnostics` struct. Easier to mock in tests, follows existing pattern of narrow interfaces.

6. **SceneDiagnostics lives on Scene** — Exposed via `Scene::getDiagnostics()`. The executor accesses it through `getScene()->getDiagnostics()`. No need for a separate `getSceneDiagnostics()` method on ScriptEnvironment.

7. **String comparisons deferred** — Phase 4B (string global fields like `assert focused_scene == "gameplay"`) adds parser complexity for quoted strings and a type system change. Deferred until Phases 1–4A are proven. Workaround: `assert scene "gameplay" is_focused == true`.

8. **No `query` command** — Removed from the plan. The primary use case (reading engine state into variables for cross-comparison) is niche for smoke tests. `set` + `$VAR_NAME` references cover the threshold/constant use case. If `query` is needed later, it can be added as a follow-up without changing anything from this plan.

9. **`entities_drawn` retained as backward-compatible alias** — Existing smoke scripts use `entities_drawn`. It remains but is semantically equivalent to `entity_count`.

## Testing Strategy

- **Unit tests for SceneDiagnostics**: Verify entity type classification (especially multi-inheritance cases: `PhysicsSpriteEntity` increments both sprite and physics counts), lifecycle counters on enter/exit/pause/resume, `clearEntities()` resets type counts
- **Unit tests for field resolution**: New assert fields resolve correctly from `SceneDiagnostics`; unknown fields still return false
- **Unit tests for global asserts**: `scenes_created`, `scenes_removed` resolve through executor
- **Unit tests for variable references**: `$VAR_NAME` resolves at execution time; undefined variable produces assertion error
- **Mock updates**: `MockScriptEnvironment` (or equivalent test double) must implement new virtual methods
- **Smoke tests**: Update 3-5 existing smoke scripts to exercise the new fields as shown in Phase 5
- **Integration**: The existing `assert` infrastructure and `smoke-test.ps1` script require no changes — assertion failures already cause non-zero exit codes which the runner detects

## Appendix: Current Smoke Test Analysis

### Coverage Gap: Only 1 of 30 Scripts Uses Assertions

Of the 30 smoke test scripts, **only `smoke_four_scene_3d.vdescript`** uses any `assert` commands. The remaining 29 scripts are "input-only" — they verify that the executable launches and doesn't crash, but confirm nothing about what was rendered or created.

### Current Scripts by Pattern

| Script | Pattern | Scene Name(s) | Entity Types | Entity Count |
|--------|---------|---------------|--------------|-------------|
| smoke_breakout | input-only | "main" | SpriteEntity | 42 (paddle + ball + 40 bricks) |
| smoke_physics_demo | input-only | "main" | PhysicsSpriteEntity | 9 (ground + player + 7 boxes) |
| smoke_physics_showcase | input-only | "main" | PhysicsSpriteEntity | ~6 per test, 7 tests |
| smoke_sprite | input-only | "main" | SpriteEntity + AnimatedSprite | 7 |
| smoke_multi_scene | input-only | "space", "forest", "city", "ocean", "hud" | MeshEntity | ~15-30 per scene |
| smoke_shooter | input-only | "main" | SpriteEntity | ~5 + dynamic bullets |
| smoke_parallax_demo | input-only | "main" | SpriteEntity | ~2 layers |
| smoke_materials | input-only | "main" | MeshEntity (MaterialCube) | 5 |
| smoke_textured_cube | input-only | "main" | MeshEntity (TexturedCube) | 1 |
| smoke_transition_demo | input-only | "game", "credits", "showcase" | MeshEntity | ~10-20 per scene |
| smoke_mission_control | minimal (wait+exit) | "main" | TextEntity | ~22 (clock×2 + telemetry×12 + alert×2 + log title + log×5) |
| smoke_pixel_arcade | minimal (wait+exit) | "main" | SpriteEntity | ~8 |
| smoke_four_scene_3d | **assertions** | "crystal", "metropolis", "nature", "cosmos" | MeshEntity | ~20-50 per scene |
| smoke_asteroids | input-only | "main" | SpriteEntity | ~20-40 dynamic |
| smoke_vertical_shooter | input-only | "main" | SpriteEntity | dynamic |
| smoke_text_adventure | input-only | "main" | TextEntity | ~8 |
| smoke_text_metrics | input-only | "main" | SpriteEntity | ~8 |

### Improved Smoke Test Examples

The following examples show how each script can be enhanced with the new diagnostic fields from this plan. Each "Before" shows the current script; each "After" shows the improved version.

---

#### 1. Breakout Demo — Entity Type Verification

**Before** ([smoke_breakout.vdescript](smoketests/scripts/smoke_breakout.vdescript)):
```vdescript
wait startup
wait 500
keydown LEFT
wait 1000
keyup LEFT
keydown RIGHT
wait 1000
keyup RIGHT
wait 3s
exit
```
Only verifies the process doesn't crash. Cannot confirm the paddle, ball, or any bricks were created.

**After**:
```vdescript
# smoke_breakout.vdescript — Entity and lifecycle verification
wait startup
wait_frames 10

# Scene entered and got focus
assert scene "main" enter_count > 0
assert scene "main" is_focused == true

# Verify entity setup: paddle (1) + ball (1) + bricks (8×5=40) = 42 sprites
assert scene "main" entity_count >= 42
assert scene "main" sprite_entity_count >= 42

# No physics entities (breakout uses custom collision, not PhysicsEntity)
assert scene "main" physics_entity_count == 0

# Play — move paddle
keydown LEFT
wait 1000
keyup LEFT
keydown RIGHT
wait 1000
keyup RIGHT
wait 3s

# After gameplay, the ball and paddle should still exist;
# some bricks may have been removed
assert scene "main" entity_count >= 2
assert scene "main" entities_removed >= 0

print Breakout entity diagnostics passed!
exit
```

**What this catches**: If a refactor accidentally breaks brick creation (e.g., the loop doesn't run), or entity removal has a bug (use-after-free, double-remove), the entity count assertion will fail immediately instead of silently passing.

---

#### 2. Physics Demo — Physics Entity Type Counting

**Before** ([smoke_physics_demo.vdescript](smoketests/scripts/smoke_physics_demo.vdescript)):
```vdescript
wait startup
wait 1000
press 1
wait 500
press 2
wait 500
wait 2000
exit
```

**After**:
```vdescript
# smoke_physics_demo.vdescript — Physics entity verification
wait startup
wait_frames 10

# Scene lifecycle
assert scene "main" enter_count > 0
assert scene "main" is_focused == true

# Initial setup: ground + player + 7 boxes = 9 PhysicsSpriteEntities
assert scene "main" entity_count >= 9
assert scene "main" physics_entity_count >= 9
assert scene "main" sprite_entity_count >= 9

# Press 1 to reset — entities get cleared and recreated
press 1
wait 500

# After reset, same entity count
assert scene "main" entity_count >= 9
assert scene "main" entities_created >= 18
assert scene "main" entities_removed >= 9

# Press 2 to spawn more boxes
press 2
wait 500
assert scene "main" entity_count > 9

wait 2s
print Physics entity diagnostics passed!
exit
```

**What this catches**: Physics entity creation failures (e.g., PhysicsBody initialization crash), entity leak on reset (entities not properly cleared), or spawn function silently failing.

---

#### 3. Multi-Scene Demo — Scene Lifecycle and Focus

**Before** ([smoke_multi_scene.vdescript](smoketests/scripts/smoke_multi_scene.vdescript)):
```vdescript
wait startup
wait 1000
press 1
wait 500
press 2
wait 500
press 3
wait 500
press 4
wait 500
wait 2s
exit
```

**After**:
```vdescript
# smoke_multi_scene.vdescript — Scene lifecycle and multi-scene diagnostics
wait startup
wait_frames 10

# Verify all 5 scenes were created
assert scenes_created >= 5

# Initial state: dual-scene rendering
assert rendered_scene_count >= 2
assert scene "space" enter_count > 0

# Each scene should have entities
assert scene "space" entity_count > 0
assert scene "space" mesh_entity_count > 0

# Switch scene groups
press 1
wait 500
assert scene "space" enter_count > 0

press 2
wait 500
assert scene "forest" enter_count > 0
assert scene "forest" entity_count > 0

press 3
wait 500
assert scene "city" enter_count > 0
assert scene "city" entity_count > 0

press 4
wait 500
assert scene "ocean" enter_count > 0
assert scene "ocean" entity_count > 0

# HUD scene should have been active at some point
assert scene "hud" enter_count > 0

wait 2s
print Multi-scene lifecycle diagnostics passed!
exit
```

**What this catches**: Scene registration failures, scenes not entering lifecycle correctly, entity counts not propagating across scene switches, and the HUD overlay not being included in scene groups.

---

#### 4. Mission Control Demo — TextEntity Count Verification

**Before** ([smoke_mission_control_demo.vdescript](smoketests/scripts/smoke_mission_control_demo.vdescript)):
```vdescript
wait startup
wait 3s
exit
```
The weakest smoke test — only confirms the process starts and exits.

**After**:
```vdescript
# smoke_mission_control_demo.vdescript — TextEntity dashboard verification
wait startup
wait_frames 10

# Scene basics
assert scene "main" enter_count > 0
assert scene "main" is_focused == true

# Mission Control creates ~22 TextEntities:
# clock label + clock value + 12 telemetry rows + alert label + alert banner
# + log title + 5 log lines = 22
assert scene "main" entity_count >= 20
assert scene "main" text_entity_count >= 20
assert scene "main" sprite_entity_count >= 20

# TextEntity is-a SpriteEntity, so sprite count should match
# No mesh or physics entities in this demo
assert scene "main" mesh_entity_count == 0
assert scene "main" physics_entity_count == 0

# Let the dashboard update for a few seconds (it has live-updating telemetry)
wait 3s

# Entity count should remain stable (no dynamic spawning/despawning)
assert scene "main" entities_removed == 0

print Mission Control TextEntity diagnostics passed!
exit
```

**What this catches**: TextEntity creation failures (e.g., font loading issues), entity type misclassification, unexpected entity churn during live updates.

---

#### 5. Transition Demo — Scene Lifecycle Through Transitions

**Before** ([smoke_transition_demo.vdescript](smoketests/scripts/smoke_transition_demo.vdescript)):
```vdescript
wait startup
wait 1000
press 1
wait 2s
press 4
wait 2s
press 2
wait 2s
press 3
wait 2s
exit
```

**After**:
```vdescript
# smoke_transition_demo.vdescript — Scene transition lifecycle verification
wait startup
wait_frames 10

# Three scenes were registered
assert scenes_created >= 3

# Initial scene: "game" should be active and entered
assert scene "game" enter_count > 0
assert scene "game" is_focused == true
assert scene "game" entity_count > 0
assert scene "game" mesh_entity_count > 0

# Fade transition to game (press 1 triggers transition)
press 1
wait 2s

# After transition, game scene should still be active
assert scene "game" enter_count >= 1

# Circle reveal (press 4)
press 4
wait 2s

# Wipe left to credits (press 2)
press 2
wait 2s

# Credits scene should have been entered
assert scene "credits" enter_count > 0
assert scene "credits" entity_count > 0

# Wipe right back to game (press 3)
press 3
wait 2s

# Game scene should have been re-entered
assert scene "game" enter_count >= 2

print Transition lifecycle diagnostics passed!
exit
```

**What this catches**: Scene transition not completing (destination scene never entered), entity setup not running on re-entry, scene lifecycle callbacks not firing during transitions.

---

#### 6. Shooter Demo — Dynamic Entity Creation During Gameplay

**Before** ([smoke_shooter.vdescript](smoketests/scripts/smoke_shooter.vdescript)):
```vdescript
wait startup
wait 500
press SPACE
wait 1s
keydown LEFT
wait 500
keyup LEFT
keydown RIGHT
wait 500
keyup RIGHT
press SPACE
wait 500
wait 1s
exit
```

**After**:
```vdescript
# smoke_shooter.vdescript — Dynamic entity tracking
wait startup
wait_frames 10

# Initial setup (title state or gameplay scene)
assert scene "main" enter_count > 0
assert scene "main" is_focused == true
assert scene "main" entity_count > 0
assert scene "main" sprite_entity_count > 0

# Start game from title
press SPACE
wait 1s

# Ship and initial enemies should exist
assert scene "main" entity_count >= 3

# Move ship and fire
keydown LEFT
wait 500
keyup LEFT
keydown RIGHT
wait 500
keyup RIGHT

# Fire — should create bullet entities
press SPACE
wait 500

# More entities should have been created (bullets)
assert scene "main" entities_created > 5

wait 1s
print Shooter entity diagnostics passed!
exit
```

**What this catches**: Bullet entity creation not working, entity pool exhaustion, entities not being created in the correct scene.

---

#### 7. Four-Scene 3D Demo — Enhanced Existing Assertions

**Before** ([smoke_four_scene_3d.vdescript](smoketests/scripts/smoke_four_scene_3d.vdescript)):
Already uses assertions, but only `was_rendered`, `draw_calls > 0`, `entities_drawn > 0`, `viewport_width/height`, `not_blank`.

**After** (additions to the existing script):
```vdescript
# smoke_four_scene_3d.vdescript — Enhanced with type-level diagnostics
wait startup
wait_frames 10

# Existing assertions (unchanged)
assert rendered_scene_count == 4
assert scene "crystal" was_rendered == true
assert scene "crystal" draw_calls > 0
assert scene "crystal" entities_drawn > 0
assert scene "metropolis" was_rendered == true
assert scene "nature" was_rendered == true
assert scene "cosmos" was_rendered == true

# NEW: Entity type verification — all scenes use MeshEntity
assert scene "crystal" mesh_entity_count > 0
assert scene "metropolis" mesh_entity_count > 0
assert scene "nature" mesh_entity_count > 0
assert scene "cosmos" mesh_entity_count > 0

# NEW: No sprite or physics entities in this 3D demo
assert scene "crystal" sprite_entity_count == 0
assert scene "crystal" physics_entity_count == 0

# NEW: Scene lifecycle — all 4 should have entered
assert scene "crystal" enter_count > 0
assert scene "metropolis" enter_count > 0
assert scene "nature" enter_count > 0
assert scene "cosmos" enter_count > 0

# NEW: Focus check (crystal is the primary/focused scene)
assert scene "crystal" is_focused == true

# NEW: Viewport checks
assert scene "crystal" viewport_width > 0
assert scene "crystal" viewport_height > 0

set THRESHOLD 0.02
print Enhanced four-scene diagnostics passed!
screenshot test_output.png
wait_frames 5
exit
```

---

#### 8. Physics Showcase — Per-Test Entity Verification

**Before** ([smoke_physics_showcase.vdescript](smoketests/scripts/smoke_physics_showcase.vdescript)):
Only navigates through 7 tests with key presses.

**After**:
```vdescript
# smoke_physics_showcase.vdescript — Physics entity counts per test scenario
wait startup
wait_frames 10

# Scene lifecycle
assert scene "main" enter_count > 0
assert scene "main" is_focused == true

# Test 1: Gravity Rain (default) — entities should exist
assert scene "main" entity_count > 0
assert scene "main" physics_entity_count > 0

wait 700

# Cycle to Test 2: Bouncy Chamber — entities cleared and recreated
press right
wait_frames 10
assert scene "main" entity_count > 0
assert scene "main" physics_entity_count > 0
assert scene "main" entities_removed > 0

wait 700

# Cycle to Test 3: Domino Chain
press right
wait_frames 10
assert scene "main" entity_count > 0
press space
wait 500

# Cycle to Test 4: Wrecking Ball
press right
wait_frames 10
assert scene "main" physics_entity_count > 0

wait 700

# Cycle to Test 5: Zero Gravity
press right
wait_frames 10
assert scene "main" entity_count > 0

wait 700

# Cycle to Test 6: Gravity Flip
press right
wait 1000

# Cycle to Test 7: Orbital
press right
wait 1000

# After cycling all tests, many entities should have been created and removed
assert scene "main" entities_created > 20
assert scene "main" entities_removed > 10

print Physics showcase entity diagnostics passed!
exit
```

---

### Summary: What the Diagnostics Add

| Verification | Before (current) | After (with diagnostics) |
|---|---|---|
| "Did it crash?" | All 30 scripts | All 30 scripts (unchanged) |
| "Were entities created?" | Only four_scene_3d (via `entities_drawn > 0`) | All scripts with `entity_count` / type counts |
| "Correct entity types?" | Never verified | `mesh_entity_count`, `sprite_entity_count`, `physics_entity_count`, `text_entity_count` |
| "Scene entered correctly?" | Never verified | `enter_count > 0` on every scene |
| "Scene has focus?" | Never verified | `is_focused == true` |
| "Dynamic entity lifecycle?" | Never verified | `entities_created >= N`, `entities_removed >= N` |
| "Entity count stable?" | Never verified | Assert same count before/after gameplay |
| "Multiple scenes created?" | Only four_scene_3d | `scenes_created >= N` |

## Future Work (Out of Scope for This Plan)

- **String assert comparisons** (Phase 4B) — `assert focused_scene == "gameplay"` with quoted string RHS
- **Query command** — `query VAR scene "name" field` to read engine state into script variables
- Real-time draw call counting (requires Vulkan-level instrumentation)
- Per-entity CPU/GPU timing
- Memory usage diagnostics
- Script-side entity manipulation
- Conditional branching (`if`/`else`) in scripts
