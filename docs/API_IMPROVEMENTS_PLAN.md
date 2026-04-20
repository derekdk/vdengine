# API Improvements Plan

**Created:** April 19, 2026  
**Last updated:** April 20, 2026  
**Based on:** Analysis of examples created between February–April 2026  
**Goal:** Reduce boilerplate and friction in the most common API workflows

---

## Executive Summary

Analysis of the 12 most recently created examples reveals that **~55–60%** of typical example code is repeated setup and boilerplate rather than unique game logic. Seven findings were identified; two have been implemented in the engine API but not yet adopted by examples. This plan organizes the remaining work into three phases: new API additions, example migrations, and documentation.

### Status Overview (as of April 20, 2026)

| # | Finding | Status | Notes |
|---|---------|--------|-------|
| 1 | TextEntity auto-sizing | **DONE** | Implemented as `setWorldHeight()` + `sizeToFit()` |
| 2 | KeyStateTracker utility | **DONE** | Header, impl, 13 unit tests; documented in API-DOC.md |
| 3 | Scene::addTextLabel convenience | **Not started** | Lower priority now that `setWorldHeight()` exists |
| 4 | PhysicsBodyDef factory methods | **DONE** | 4 factories in `PhysicsTypes.h`; all physics examples migrated |
| 5 | ResourceManager::addPersistent | **DONE** | Strong-ref cache mode; 7 unit tests; `spritesheet_multiscene_demo` migrated |
| 6 | Migrate old examples to `setup2D()` | **DONE** | 12 scenes migrated across 10 examples |
| 7 | Entity collision helpers | **Not started** | 2–4 examples affected |

### New Findings (April 20, 2026)

| # | Finding | Impact | Effort |
|---|---------|--------|--------|
| 8 | Redundant `setAnchor(0.5f, 0.5f)` calls | **DONE** | ~50 calls removed across 14 example files |
| 9 | `TextRenderer::createTexture` sites that can migrate to `TextEntity` | Medium (20 sites) | Small |

---

## Phase 1 — New API Additions

New engine code that must be written, tested, and documented before examples can adopt it.

### 1A. KeyStateTracker Utility (Finding 2) — P1

**Status:** DONE  
**Impact:** High — 8+ examples, 30–50 lines of boilerplate each  
**Effort:** Small

Every interactive example creates a custom `InputHandler` subclass with repetitive boolean flag fields, `onKeyPress`/`onKeyRelease` overrides, and consume methods. A 10-key handler is ~40 lines of pure boilerplate.

**Affected examples:** `shooter_demo`, `breakout_demo`, `asteroids_demo`, `multi_scene_demo`, `diagnostics_demo`, `parallax_demo`, `hex_prism_stacks_demo`, `transition_demo`

**Proposed API:**

```cpp
class KeyStateTracker {
public:
    void bindHeld(int keyCode, const std::string& name);
    void bindOneShot(int keyCode, const std::string& name);
    bool isHeld(const std::string& name) const;
    bool consume(const std::string& name);
    void handlePress(int key);
    void handleRelease(int key);
};
```

**Before (40+ lines):** Custom InputHandler subclass per example  
**After (6 lines):**
```cpp
KeyStateTracker keys;
keys.bindHeld(vde::KEY_LEFT, "left");
keys.bindHeld(vde::KEY_RIGHT, "right");
keys.bindOneShot(vde::KEY_SPACE, "fire");

// In update:
if (keys.isHeld("left")) { /* move */ }
if (keys.consume("fire")) { /* shoot */ }
```

### 1B. ResourceManager::addPersistent (Finding 5) — P2

**Status:** DONE  
**Impact:** Medium — affects multi-scene resource sharing  
**Effort:** Small

`ResourceManager` stores resources as `weak_ptr`. If the caller doesn't hold a strong reference, the resource silently expires. The `spritesheet_multiscene_demo` works around this with explicit member variables.

**Proposed API:**

```cpp
class ResourceManager {
public:
    // Existing: weak_ptr cache (auto-cleanup when last external ref dies)
    template<typename T>
    void add(const std::string& key, std::shared_ptr<T> resource);

    // New: strong cache (lives until explicitly removed or manager destroyed)
    template<typename T>
    void addPersistent(const std::string& key, std::shared_ptr<T> resource);
};
```

Retrieval via `get<T>()` works identically for both storage modes.

### 1C. Entity Collision Helpers (Finding 7) — P3

**Status:** Not started  
**Impact:** Low-Medium — 2–4 action/shooter examples  
**Effort:** Medium

Examples that use simple AABB collision (without full physics) manually extract positions, construct `WorldBounds2D`, and check intersections.

**Proposed API:**

```cpp
// On SpriteEntity / Entity
void setCollisionExtents(float halfWidth, float halfHeight);
bool overlaps(const Entity& other) const;
WorldBounds2D getWorldBounds() const;
```

**Before (6 lines per check):**
```cpp
auto bBounds = WorldBounds2D::fromCenterSize({bPos.x, bPos.y}, {0.2f, 0.4f});
auto eBounds = WorldBounds2D::fromCenterSize({ePos.x, ePos.y}, {0.7f, 0.7f});
if (bBounds.intersects(eBounds)) { /* hit */ }
```

**After (1 line per check, after one-time setup):**
```cpp
if (bullet->overlaps(*enemy)) { /* hit */ }
```

### 1D. Scene::addTextLabel Convenience (Finding 3) — P3

**Status:** Not started  
**Impact:** Reduced (was Medium-High, now Low) — `setWorldHeight()` already covers most use cases  
**Effort:** Small

A one-liner convenience for the most common text creation pattern. Lower priority now that `TextEntity::setWorldHeight()` exists.

```cpp
TextEntity* addTextLabel(const std::string& text,
                         const BitmapFont& font,
                         float worldHeight,
                         float x, float y,
                         const TextStyle& style = {});
```

---

## Phase 2 — Example Migrations

Adopt existing API features that examples haven't migrated to yet. No engine changes required — pure example cleanup. Each migration is independent and can be done in any order.

### 2A. PhysicsBodyDef Factory Adoption (Finding 4)

**Status:** DONE — all 4 physics examples migrated (11 verbose blocks replaced)  
**Impact:** Medium — ~70 verbose definitions across 6 examples  
**Effort:** Small per example

Four factory methods (`dynamicBox`, `dynamicCircle`, `staticBox`, `kinematicBox`) exist in `PhysicsTypes.h` but no examples use them.

**Affected examples:** `physics_demo`, `physics_audio_demo`, `physics_showcase_demo`, `parallel_physics_demo`, `breakout_demo`, `spritesheet_multiscene_demo`

**Before (8 lines):**
```cpp
PhysicsBodyDef def;
def.type = PhysicsBodyType::Dynamic;
def.shape = PhysicsShape::Box;
def.position = {2.0f, 5.0f};
def.extents = {0.5f, 0.5f};
def.mass = 1.0f;
def.restitution = 0.6f;
entity->createPhysicsBody(def);
```

**After (2–3 lines):**
```cpp
auto def = PhysicsBodyDef::dynamicBox({2.0f, 5.0f}, {0.5f, 0.5f});
def.restitution = 0.6f;
entity->createPhysicsBody(def);
```

### 2B. setup2D() Migration (Finding 6)

**Status:** DONE — 12 scenes migrated across 10 examples  
**Impact:** Low — pure cleanup  
**Effort:** Trivial per example

**Remaining examples (manual `Camera2D` + `setBackgroundColor`):** `breakout_demo`, `asteroids_demo`, `multi_scene_demo` (3 scenes), `mission_control_demo`, `text_adventure_demo`, `world_bounds_demo`, `text_rendering_demo`, `shooter_demo`, `quad_viewport_demo` (4 scenes)

**Before (5 lines):**
```cpp
auto* camera = new vde::Camera2D(18.0f, 11.0f);
camera->setPosition(0.0f, 0.0f);
camera->setZoom(1.0f);
setCamera(camera);
setBackgroundColor(vde::Color::fromHex(0x1a1a2e));
```

**After (1 line):**
```cpp
setup2D(18.0f, 11.0f, vde::Color::fromHex(0x1a1a2e));
```

### 2C. TextRenderer::createTexture → TextEntity Migration (Finding 9)

**Status:** Not started  
**Impact:** Medium — ~20 call sites across 3 examples  
**Effort:** Small per example

Three examples still create text sprites manually via `TextRenderer::createTexture()` when they could use `TextEntity` + `setWorldHeight()`.

**Affected examples:** `pixel_arcade_demo` (8 sites), `font_specimen_demo` (8+ sites), `emoji_demo` (2 sites)

**Note:** `emoji_demo` may require keeping some `TextRenderer` calls for its dual TTF+emoji font path. `font_specimen_demo` may intentionally use raw texture creation to demonstrate the API.

### 2D. Remove Redundant setAnchor(0.5, 0.5) Calls (Finding 8)

**Status:** DONE — ~50 redundant calls removed across 14 example files  
**Impact:** Low — pure cleanup  
**Effort:** Trivial

`Entity.h` defaults `m_anchorX = 0.5f, m_anchorY = 0.5f`. Over 20 examples call `setAnchor(0.5f, 0.5f)` redundantly. These calls can be removed.

**Note:** Only remove calls that set the default value. Calls that set non-default anchors (e.g., `setAnchor(0.0f, 0.0f)`) must be preserved.

### 2E. Remove Legacy sizeToFit Free Function (Finding 1 cleanup)

**Status:** DONE — free function removed, 7 call sites migrated to `setWorldHeight()`  
**Impact:** Low — cleanup  
**Effort:** Trivial

The `static void sizeToFit(TextEntity&, float)` free function survives in `spritesheet_multiscene_demo/main.cpp`. Replace with `setWorldHeight()`.

---

## Phase 3 — Documentation

Update `API-DOC.md` to cover gaps identified during the analysis.

| Topic | Current State | Action |
|-------|---------------|--------|
| SpriteSheet API | Not documented | Add full section with usage examples |
| TextEntity + `setWorldHeight()` | Minimal | Expand with sizing patterns and examples |
| ResourceManager weak_ptr semantics | **Documented** | `add()` vs `addPersistent()` documented in API-DOC.md |
| Input handler consume pattern | **Documented** | `KeyStateTracker` usage documented in API-DOC.md |
| PhysicsBodyDef factory methods | **Documented** | Factory examples added to Physics System section |
| `setup2D()` | **Documented** | Added to Scene section in API-DOC.md |

---

## Completed Items

### ~~Finding 1: TextEntity Sizing~~ — DONE

Implemented as `TextEntity::setWorldHeight(float)` + `Entity::sizeToFit(float, float)`.

- `setWorldHeight()` makes sizing declarative — `update()` automatically calls `sizeToFit()` after every texture rebuild
- `setMaxWidth()` available for width-constrained text
- 20+ call sites in newer examples already use the new API
- Legacy free function removed from `spritesheet_multiscene_demo` (Phase 2E)

### ~~Finding 2: KeyStateTracker~~ — DONE

Implemented as `KeyStateTracker` utility class in `include/vde/api/KeyStateTracker.h`.

- `bindHeld()` / `bindOneShot()` for registering key bindings
- `isHeld()` / `consume()` for querying state in `update()`
- `handlePress()` / `handleRelease()` wired from `InputHandler` callbacks
- 13 unit tests covering held, one-shot, mixed bindings, multi-key actions, edge cases
- Included via `GameAPI.h`

### ~~Finding 4: PhysicsBodyDef Factory Methods~~ — DONE

Four static factories implemented in `PhysicsTypes.h`:
- `PhysicsBodyDef::dynamicBox(pos, extents, mass)`
- `PhysicsBodyDef::dynamicCircle(pos, radius, mass)`
- `PhysicsBodyDef::staticBox(pos, extents)`
- `PhysicsBodyDef::kinematicBox(pos, extents)` (bonus — not in original plan)

All 4 physics examples migrated (11 verbose blocks replaced).

### ~~Finding 5: ResourceManager::addPersistent~~ — DONE

Added `addPersistent<T>(key, resource)` to `ResourceManager`.

- Stores a strong `shared_ptr` alongside the weak cache entry
- Resource survives after all external refs dropped; `get<T>()` returns valid ptr
- Cleared by `remove()`, `clear()`, or manager destruction
- 7 unit tests; `spritesheet_multiscene_demo` migrated (removed manual strong-ref workaround)

### ~~Finding 6: setup2D() Migration~~ — DONE

12 scenes across 10 examples migrated from manual `Camera2D` + `setBackgroundColor` to `setup2D()`.

### ~~Finding 8: Redundant setAnchor Removal~~ — DONE

~50 redundant `setAnchor(0.5f, 0.5f)` calls removed across 14 example files. Only intentional anchor resets preserved (e.g., `sprite_demo` key-1 interactive reset).

---

## Testing Plan

Each phase has specific verification gates. All changes must pass the full verify pipeline before merge.

### Phase 1 Testing (New API)

| Item | Unit Tests | Integration | Smoke |
|------|-----------|-------------|-------|
| **1A. KeyStateTracker** | `bindHeld`/`bindOneShot` registration; `isHeld` returns true only while key held; `consume` returns true once then false; unknown name returns false; multiple keys bound to same name | Migrate 1 example (e.g., `breakout_demo`) and run its smoke test | Full smoke suite after all example migrations |
| **1B. addPersistent** | Resource survives after all external refs dropped; `get<T>()` returns valid ptr; weak `add()` still expires as before; persistent resource removed on `clear()` | Update `spritesheet_multiscene_demo` to use `addPersistent`; run its smoke test | Full smoke suite |
| **1C. Entity collision** | `setCollisionExtents` + `overlaps` returns correct results for overlapping/non-overlapping entities; `getWorldBounds` matches position+extents | Migrate `shooter_demo` collision; run its smoke test | Full smoke suite |
| **1D. addTextLabel** | Returns valid `TextEntity*` with correct position, font, text, and size | Migrate one example label set; visual inspection | Full smoke suite |

**Verification command for each item:**
```powershell
# Tight loop: build + run affected unit tests
.\scripts\build-and-test.ps1

# After migration: targeted smoke test
.\scripts\smoke-test.ps1 -Filter "*breakout*"

# Final gate: full verification
.\scripts\verify.ps1
```

### Phase 2 Testing (Migrations)

Each migration is a behavior-preserving refactor. The primary verification is:

1. **Build succeeds** — `.\scripts\build.ps1`
2. **All unit tests pass** — `.\scripts\test.ps1`
3. **Smoke test for the modified example passes** — `.\scripts\smoke-test.ps1 -Filter "*example_name*"`
4. **Render verification (if golden images exist)** — `.\scripts\render-verify.ps1 -Filter "*example_name*"`

Migrations can be batched (e.g., all `setup2D` migrations in one commit) and verified together:
```powershell
.\scripts\verify.ps1
```

**Risk:** Migrations 2C (TextRenderer→TextEntity) may subtly change text rendering (different code path). These require visual comparison or render verification.

### Phase 3 Testing (Documentation)

- Review `API-DOC.md` changes for accuracy against actual header signatures
- Verify all code snippets compile (copy into a test example)
- No automated tests required

### Regression Safety

After all phases complete, run the full verification pipeline to catch any interactions:
```powershell
.\scripts\verify.ps1 -SmokeExtended
```

This runs build + unit tests + all priority-1 and priority-2 smoke tests + render verification.
