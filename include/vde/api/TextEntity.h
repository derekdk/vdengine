#pragma once

/**
 * @file TextEntity.h
 * @brief A scene entity that owns a text texture and rebuilds it lazily on change.
 *
 * TextEntity inherits SpriteEntity and adds text-specific properties
 * (string content, font, style). When any property changes the entity
 * marks itself dirty and rebuilds the backing texture once per frame
 * at the start of the next update() call.
 */

#include <vde/api/BitmapFont.h>
#include <vde/api/Entity.h>
#include <vde/api/TextRenderer.h>
#include <vde/api/TrueTypeFont.h>

#include <memory>
#include <string>

namespace vde {

/**
 * @brief A SpriteEntity that renders text and rebuilds its texture lazily.
 *
 * Usage:
 * @code
 * auto label = scene->addEntity<vde::TextEntity>();
 * label->setText("SCORE: 0");
 * label->setFont(vde::BitmapFont::small());
 * label->setStyle({ .color = vde::Color::yellow(), .pixelScale = 2 });
 * label->setPosition(0.0f, 5.0f, 0.0f);
 * @endcode
 *
 * The texture is rebuilt automatically on the next frame whenever text,
 * font, or style changes. Consecutive property changes within the same
 * frame produce only a single rebuild.
 */
class TextEntity : public SpriteEntity {
  public:
    using Ref = std::shared_ptr<TextEntity>;

    TextEntity();
    ~TextEntity() override = default;

    // ── Text content ────────────────────────────────────────────────

    /**
     * @brief Set the text string to render.
     * @param text ASCII printable string
     */
    void setText(const std::string& text);

    /**
     * @brief Get the current text string.
     */
    const std::string& getText() const { return m_text; }

    // ── Font selection ──────────────────────────────────────────────

    /**
     * @brief Use a bitmap font for rendering.
     * @param font Reference to BitmapFont singleton (small or large)
     */
    void setFont(const BitmapFont& font);

    /**
     * @brief Use a TrueType font for rendering.
     * @param font Pointer to a loaded TrueTypeFont (must outlive the entity)
     */
    void setTrueTypeFont(const TrueTypeFont* font);

    // ── Style ───────────────────────────────────────────────────────

    /**
     * @brief Set the text rendering style (color, pixelScale, letterSpacing).
     */
    void setStyle(const TextStyle& style);

    /**
     * @brief Get the current text style.
     */
    const TextStyle& getStyle() const { return m_style; }

    // ── Auto-sizing ─────────────────────────────────────────────────

    /**
     * @brief Set the desired world-space height for automatic sizing.
     *
     * When a positive value is set, the entity automatically calls
     * sizeToFit() after every texture rebuild (including the first).
     * This eliminates the need for manual update(0)+sizeToFit() calls.
     *
     * Set to 0 to disable auto-sizing (the default).
     *
     * @param height Desired height in world units (0 = disabled)
     */
    void setWorldHeight(float height);

    /**
     * @brief Get the configured world height for auto-sizing.
     * @return The world height, or 0 if auto-sizing is disabled
     */
    float getWorldHeight() const { return m_worldHeight; }

    /**
     * @brief Set a maximum width constraint for auto-sizing.
     *
     * When auto-sizing is active and the computed width would exceed
     * this limit, the text is scaled down proportionally.
     * Set to 0 for no width limit (the default).
     *
     * @param width Maximum width in world units (0 = no limit)
     */
    void setMaxWidth(float width);

    /**
     * @brief Get the configured maximum width for auto-sizing.
     * @return The max width, or 0 if unconstrained
     */
    float getMaxWidth() const { return m_maxWidth; }

    // ── Lifecycle ───────────────────────────────────────────────────

    void onAttach(Scene* scene) override;
    void update(float deltaTime) override;
    void render() override;

  private:
    void markDirty();
    void rebuildTexture();

    std::string m_text;
    const BitmapFont* m_bitmapFont = nullptr;
    const TrueTypeFont* m_ttFont = nullptr;
    TextStyle m_style;
    bool m_dirty = true;         ///< True when the texture needs rebuilding
    float m_worldHeight = 0.0f;  ///< Auto-sizing target height (0 = disabled)
    float m_maxWidth = 0.0f;     ///< Auto-sizing max width constraint (0 = none)
};

}  // namespace vde
