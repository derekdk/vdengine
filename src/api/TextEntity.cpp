/**
 * @file TextEntity.cpp
 * @brief Implementation of the TextEntity class.
 */

#include <vde/api/TextEntity.h>

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/Game.h>
#include <vde/api/Scene.h>

namespace vde {

// ============================================================================
// Construction
// ============================================================================

TextEntity::TextEntity() : SpriteEntity() {
    m_bitmapFont = &BitmapFont::small();
}

// ============================================================================
// Property setters
// ============================================================================

void TextEntity::setText(const std::string& text) {
    if (m_text != text) {
        m_text = text;
        markDirty();
    }
}

void TextEntity::setFont(const BitmapFont& font) {
    if (m_bitmapFont != &font || m_ttFont != nullptr) {
        m_bitmapFont = &font;
        m_ttFont = nullptr;
        markDirty();
    }
}

void TextEntity::setTrueTypeFont(const TrueTypeFont* font) {
    if (m_ttFont != font) {
        m_ttFont = font;
        m_bitmapFont = nullptr;
        markDirty();
    }
}

void TextEntity::setStyle(const TextStyle& style) {
    if (!(m_style == style)) {
        m_style = style;
        markDirty();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void TextEntity::onAttach(Scene* scene) {
    SpriteEntity::onAttach(scene);

    // Create a 1x1 transparent placeholder so rendering never crashes
    auto placeholder = std::make_shared<Texture>();
    std::vector<uint8_t> transparent(4, 0);
    placeholder->loadFromData(transparent.data(), 1, 1);

    Game* game = scene->getGame();
    if (game) {
        placeholder->uploadToGPU(game->getVulkanContext());
    }
    setTexture(placeholder);
}

void TextEntity::update(float deltaTime) {
    SpriteEntity::update(deltaTime);

    if (m_dirty) {
        rebuildTexture();
        m_dirty = false;
    }
}

// ============================================================================
// Internal
// ============================================================================

void TextEntity::markDirty() { m_dirty = true; }

void TextEntity::rebuildTexture() {
    VulkanContext* ctx = nullptr;
    if (m_scene) {
        Game* game = m_scene->getGame();
        if (game) {
            ctx = game->getVulkanContext();
        }
    }

    std::shared_ptr<Texture> tex;

    if (m_ttFont && m_ttFont->isLoaded()) {
        tex = TextRenderer::createTexture(ctx, m_text, *m_ttFont, m_style);
    } else if (m_bitmapFont) {
        tex = TextRenderer::createTexture(ctx, m_text, *m_bitmapFont, m_style);
    } else {
        // Fallback to small bitmap font
        tex = TextRenderer::createTexture(ctx, m_text, BitmapFont::small(), m_style);
    }

    if (tex) {
        setTexture(tex);
    }
}

}  // namespace vde
