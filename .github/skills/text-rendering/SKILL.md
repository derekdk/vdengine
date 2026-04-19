---
name: text-rendering
description: Guide for adding and sizing text in VDE scenes using TextEntity, BitmapFont, setWorldHeight, and sizeToFit. Use this when creating or updating text labels, titles, HUD elements, or any on-screen text.
---

# Text Rendering in VDE

TextEntity renders bitmap-font text as a textured sprite. The engine provides **automatic sizing** via `setWorldHeight()` — configure a desired height and the entity recalculates its scale after every texture rebuild. This skill documents the recommended workflow, sizing patterns, and common pitfalls.

## When to use this skill

- Adding any TextEntity to a scene
- Updating text content dynamically (e.g., score, character name, stat values)
- Choosing font, pixelScale, and world-height values for a given setup2D size
- Debugging text that appears squished, oversized, or invisible

---

## Recommended: Automatic sizing with setWorldHeight

The easiest way to size text is to configure it once and let the engine handle the rest:

```cpp
auto label = addEntity<TextEntity>();
label->setText("HELLO WORLD");
label->setFont(BitmapFont::small());
label->setStyle({.color = Color::white(), .pixelScale = 1});
label->setPosition(0.0f, 4.0f, 0.0f);
label->setAnchor(0.5f, 0.5f);
label->setWorldHeight(0.35f);     // Auto-size to 0.35 world units tall
```

When `setWorldHeight()` is configured, `update()` automatically calls `sizeToFit()` after every texture rebuild. No manual `update(0.0f)` + `sizeToFit()` dance is needed.

For text that might overflow horizontally, add a max-width constraint:

```cpp
label->setWorldHeight(0.35f);
label->setMaxWidth(8.0f);         // Clamp width, reduce height proportionally
```

### How it works

When `worldHeight > 0`, the entity reads the texture dimensions after each rebuild, computes the aspect ratio, and sets the scale so the text is exactly `worldHeight` tall. If `maxWidth` is set and the computed width would exceed it, the width is clamped and the height is reduced proportionally.

---

## Alternative: Manual sizeToFit

`SpriteEntity::sizeToFit()` is available as a one-shot method on any sprite-based entity. Use this when you need precise manual control or when working with non-text sprites:

```cpp
auto label = addEntity<TextEntity>();
label->setText("HELLO WORLD");
label->setFont(BitmapFont::small());
label->setStyle({.color = Color::white(), .pixelScale = 1});
label->setPosition(0.0f, 4.0f, 0.0f);
label->setAnchor(0.5f, 0.5f);
label->update(0.0f);              // Forces texture rebuild — REQUIRED before manual sizeToFit
label->sizeToFit(0.35f);          // Size to 0.35 world units tall
```

With max-width clamping:

```cpp
label->update(0.0f);
label->sizeToFit(0.35f, 8.0f);   // 0.35 tall, max 8.0 wide
```

### Why update(0.0f) is mandatory before manual sizeToFit

`sizeToFit` reads the texture dimensions to compute aspect ratio. The texture is only created when `update()` triggers `rebuildTexture()`. Without this call the texture is either null or stale, and `sizeToFit` bails out via the `getWidth() < 2` guard — leaving the text at the default 1×1 scale.

**Note:** When using `setWorldHeight()`, the `update(0.0f)` call is not needed — the engine handles everything automatically.

---

## Updating text dynamically

With `setWorldHeight()` configured, just call `setText()` — the next frame's `update()` automatically rebuilds the texture and re-sizes:

```cpp
m_label->setText("NEW VALUE");
// That's it — auto-sizing handles the rest
```

For manual sizing, you must rebuild the texture and re-size:

```cpp
m_label->setText("NEW VALUE");
m_label->update(0.0f);
sizeToFit(*m_label, 0.35f);
```

**Exception:** If the text length is constant or nearly constant (e.g., a clock display "HH:MM:SS"), you can skip the re-size after the initial setup. The normal game-loop `update(dt)` will rebuild the texture, and the scale remains correct because the aspect ratio is unchanged.

---

## Fonts and pixelScale

### Available bitmap fonts

| Font | Glyph size | Characters | Use for |
|------|-----------|------------|---------|
| `BitmapFont::small()` | 5×7 px | ASCII 0x20–0x7E | Labels, stats, HUD, body text |
| `BitmapFont::large()` | 8×13 px | ASCII 0x20–0x7E | Titles, headings, emphasis |

Characters outside ASCII 0x20–0x7E render as blank space.

### What pixelScale does

`pixelScale` controls texture resolution, not world-space size. Each font pixel is stamped as a `scale × scale` block in the generated texture:

| Font | pixelScale | Texture per char | Best for |
|------|-----------|-----------------|----------|
| small | 1 | 6×7 px | Small labels (worldHeight ≤ 0.35) |
| small | 2 | 12×14 px | Medium labels (worldHeight 0.35–0.50) |
| large | 2 | 18×26 px | Titles (worldHeight 0.45–0.65) |

**Rule of thumb:** Use `pixelScale = 1` for most text. Increase to 2 when the text will be scaled to large world heights (>0.4) and looks blurry.

---

## Recommended sizes for common world dimensions

For a `setup2D(16.0f, 10.0f, ...)` world (the most common 2D setup):

| Purpose | Font | pixelScale | worldHeight | Row spacing |
|---------|------|-----------|-------------|-------------|
| Small labels | small | 1 | 0.25–0.30 | 0.40 |
| Body / stats | small | 1 | 0.30–0.35 | 0.50 |
| Subtitles | small | 2 | 0.40–0.45 | 0.55 |
| Titles | large | 2 | 0.50–0.60 | — |

For larger worlds like `setup2D(20.0f, 14.0f, ...)`, scale heights proportionally.

---

## Anchor and positioning

Anchor controls which point on the text sprite sits at the entity's position:

| Anchor | Alignment | Common use |
|--------|-----------|-----------|
| `(0.5, 0.5)` | Centered | Titles, centered labels |
| `(0.0, 0.5)` | Left edge, vertically centered | Left-aligned stats, lists |
| `(1.0, 0.5)` | Right edge, vertically centered | Right-aligned values |
| `(0.5, 0.0)` | Centered above position | Labels above sprites |

**Edge margins:** Keep text at least 0.3 world units inside the view edges to avoid clipping.

---

## Color

**Always use TextStyle.color**, not `setColor()`:

```cpp
// Correct — color baked into texture
label->setStyle({.color = Color::fromHex(0xFF4444), .pixelScale = 1});

// Wrong — tint-multiplies with baked color, unintended results
label->setColor(Color::fromHex(0xFF4444));
```

`setColor()` on TextEntity acts as a post-shader tint multiplied with the baked texture color. Since the default entity color is white, using `setColor()` appears to work initially, but breaks if the style color is non-white. Use `setStyle({.color = ...})` exclusively.

---

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| No sizing configured | Text renders as a tiny/garbled 1×1 square | Add `setWorldHeight()` or call `update(0.0f)` + `sizeToFit()` |
| Manual `sizeToFit` before `update(0.0f)` | Text invisible (guard bails out on null texture) | Always `update(0.0f)` first, or use `setWorldHeight()` instead |
| High `pixelScale` without sizing | Oversized blurry text | `pixelScale` only affects texture res — size with `setWorldHeight()` |
| Using `setColor()` for text color | Color multiplied with baked color, wrong result | Use `setStyle({.color = ...})` |
| Dynamic `setText()` without re-sizing | Text squished/stretched as content length changes | Use `setWorldHeight()` for automatic re-sizing, or call `update(0.0f)` + `sizeToFit()` |
| Text positioned at world edge | Clipped or partially off-screen | Inset by ≥ 0.3 world units from edges |
| Empty string "" | 1×1 transparent texture, invisible | Use `" "` (space) as placeholder if needed |

---

## Complete example — stat panel with dynamic text

```cpp
// In onEnter():
constexpr float kStatX = 1.5f;
constexpr float kStartY = 2.0f;
constexpr float kSpacing = 1.2f;

const char* statNames[] = {"HP", "ATK", "DEF", "SPD"};
for (int i = 0; i < 4; ++i) {
    auto nameLabel = addEntity<TextEntity>();
    nameLabel->setText(statNames[i]);
    nameLabel->setFont(BitmapFont::small());
    nameLabel->setStyle({.color = Color(0.7f, 0.7f, 0.8f, 1.0f), .pixelScale = 1});
    nameLabel->setPosition(kStatX, kStartY - static_cast<float>(i) * kSpacing, 0.0f);
    nameLabel->setAnchor(0.0f, 0.5f);
    nameLabel->setWorldHeight(0.35f);
}

// Value labels (updated dynamically later)
for (int i = 0; i < 4; ++i) {
    auto val = addEntity<TextEntity>();
    val->setFont(BitmapFont::small());
    val->setStyle({.color = Color::white(), .pixelScale = 1});
    val->setPosition(kStatX + 2.8f, kStartY - static_cast<float>(i) * kSpacing, 0.0f);
    val->setAnchor(0.0f, 0.5f);
    val->setWorldHeight(0.35f);   // Auto-sizes on every text change
    m_statValues.push_back(val);
}

// Later, when updating values — just setText(), auto-sizing handles the rest:
void showStats(int hp, int atk, int def, int spd) {
    int values[] = {hp, atk, def, spd};
    for (int i = 0; i < 4; ++i) {
        m_statValues[i]->setText(std::to_string(values[i]));
    }
}
```

## API Reference

### TextEntity auto-sizing

| Method | Description |
|--------|-------------|
| `setWorldHeight(float h)` | Set desired world-space height; 0 disables auto-sizing |
| `getWorldHeight()` | Get configured world height |
| `setMaxWidth(float w)` | Set max width constraint; 0 = unconstrained |
| `getMaxWidth()` | Get configured max width |

### SpriteEntity::sizeToFit (inherited by TextEntity)

| Method | Description |
|--------|-------------|
| `sizeToFit(float worldHeight, float maxWidth = 0)` | One-shot scale based on current texture aspect ratio |

## References

- `using-api` — Scene setup and entity creation
- `2d-games` — Camera and world coordinate patterns
- `writing-examples` — Example structure and conventions
