/**
 * @file TextEntity.cpp
 * @brief Implementation of the TextEntity class.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/TextEntity.h>

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

void TextEntity::setWorldHeight(float height) {
    m_worldHeight = height;
    // Apply immediately if a valid texture already exists
    if (m_worldHeight > 0.0f) {
        sizeToFit(m_worldHeight, m_maxWidth);
    }
}

void TextEntity::setMaxWidth(float width) {
    m_maxWidth = width;
    // Re-apply if auto-sizing is active and a valid texture exists
    if (m_worldHeight > 0.0f) {
        sizeToFit(m_worldHeight, m_maxWidth);
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void TextEntity::onAttach(Scene* scene) {
    SpriteEntity::onAttach(scene);

    // Only install a 1x1 transparent placeholder when no texture has been
    // set yet (e.g. text/style configured before attach).  Either way, mark
    // dirty so the first scene update rebuilds the real text texture.
    if (!getTexture()) {
        auto placeholder = std::make_shared<Texture>();
        std::vector<uint8_t> transparent(4, 0);
        placeholder->loadFromData(transparent.data(), 1, 1);

        Game* game = scene->getGame();
        if (game) {
            placeholder->uploadToGPU(game->getVulkanContext());
        }
        setTexture(placeholder);
    }
    markDirty();
}

void TextEntity::update(float deltaTime) {
    SpriteEntity::update(deltaTime);

    if (m_dirty) {
        rebuildTexture();
        m_dirty = false;

        // Auto-size after texture rebuild when worldHeight is configured
        if (m_worldHeight > 0.0f) {
            sizeToFit(m_worldHeight, m_maxWidth);
        }
    }
}

void TextEntity::render() {
    // Note: rebuildTexture() must NOT be called here — it creates Vulkan
    // resources which is illegal during an active render pass.  The dirty
    // flag is handled by update() which runs before the render pass.
    SpriteEntity::render();
}

// ============================================================================
// Internal
// ============================================================================

void TextEntity::markDirty() {
    m_dirty = true;
}

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
