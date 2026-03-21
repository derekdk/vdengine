/**
 * @file SpriteDocument.h
 * @brief In-memory spritesheet data model for the Sprite Editor.
 *
 * SpriteDocument holds the source image path, sprite regions, and animation
 * sequences.  It can be serialized to and deserialized from `.vdesheet` TOML
 * files via tomlplusplus.
 */

#pragma once

#include <string>
#include <vector>

namespace vde {
namespace tools {

/// A named rectangular region within the source image (pixel coordinates).
struct SpriteRegion {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float anchorX = 0.5f;
    float anchorY = 0.5f;

    bool operator==(const SpriteRegion& other) const {
        return name == other.name && x == other.x && y == other.y && w == other.w && h == other.h &&
               anchorX == other.anchorX && anchorY == other.anchorY;
    }
};

/// A single frame in an animation sequence.
struct AnimationFrame {
    std::string spriteName;  ///< References a SpriteRegion by name.
    float duration = 0.1f;   ///< Duration in seconds.

    bool operator==(const AnimationFrame& other) const {
        return spriteName == other.spriteName && duration == other.duration;
    }
};

/// A named animation sequence built from sprite frames.
struct AnimationSequence {
    std::string name;
    bool looping = true;
    std::vector<AnimationFrame> frames;

    /// Total duration of the animation.
    float getTotalDuration() const;

    /// Get the sprite name for a given elapsed time.
    /// Returns an empty string if there are no frames.
    const std::string& getFrameAt(float elapsed) const;

    bool operator==(const AnimationSequence& other) const {
        return name == other.name && looping == other.looping && frames == other.frames;
    }
};

/**
 * @brief In-memory spritesheet document.
 *
 * Stores sprite regions and animation sequences for a single source image.
 * Supports serialization to `.vdesheet` TOML format.
 */
class SpriteDocument {
  public:
    SpriteDocument() = default;

    // ── Source image ──────────────────────────────────────────────

    void setSourceImage(const std::string& path, int width, int height);
    const std::string& getSourceImagePath() const { return m_imagePath; }
    int getImageWidth() const { return m_imageWidth; }
    int getImageHeight() const { return m_imageHeight; }
    bool hasImage() const { return !m_imagePath.empty() && m_imageWidth > 0 && m_imageHeight > 0; }

    // ── Sprite regions ───────────────────────────────────────────

    /// Auto-slice the image into a uniform grid.
    /// Generates names like "sprite_0", "sprite_1", ...
    /// Returns the number of regions created.
    int gridSlice(int cellWidth, int cellHeight, int spacingX = 0, int spacingY = 0,
                  int offsetX = 0, int offsetY = 0);

    /// Add a named sprite region.  Returns false if the name already exists.
    bool addSprite(const SpriteRegion& region);

    /// Remove a sprite region by name.  Returns false if not found.
    bool removeSprite(const std::string& name);

    /// Rename a sprite region.  Returns false if oldName not found or newName exists.
    bool renameSprite(const std::string& oldName, const std::string& newName);

    /// Set the anchor of a sprite.  Returns false if not found.
    bool setAnchor(const std::string& name, float ax, float ay);

    /// Find a sprite region by name.  Returns nullptr if not found.
    const SpriteRegion* findSprite(const std::string& name) const;

    /// Get all sprite regions.
    const std::vector<SpriteRegion>& getSprites() const { return m_sprites; }

    /// Get sprite count.
    int getSpriteCount() const { return static_cast<int>(m_sprites.size()); }

    /// Clear all sprite regions (does NOT clear animations).
    void clearSprites();

    // ── Animations ───────────────────────────────────────────────

    /// Create a new (empty) animation.  Returns false if name already exists.
    bool createAnimation(const std::string& name, bool looping = true);

    /// Delete an animation by name.  Returns false if not found.
    bool deleteAnimation(const std::string& name);

    /// Add a frame to an animation.  Returns false if animation or sprite not found.
    bool addFrame(const std::string& animName, const std::string& spriteName,
                  float duration = 0.1f);

    /// Remove a frame by index.  Returns false if animation not found or index OOB.
    bool removeFrame(const std::string& animName, int index);

    /// Set frame duration.  Returns false if animation/index invalid.
    bool setFrameDuration(const std::string& animName, int index, float duration);

    /// Find animation by name.  Returns nullptr if not found.
    const AnimationSequence* findAnimation(const std::string& name) const;

    /// Get all animations.
    const std::vector<AnimationSequence>& getAnimations() const { return m_animations; }

    /// Get animation count.
    int getAnimationCount() const { return static_cast<int>(m_animations.size()); }

    /// Clear all animations.
    void clearAnimations();

    // ── Serialization ────────────────────────────────────────────

    /// Save the document as a `.vdesheet` TOML file.
    /// Returns true on success.
    bool saveToFile(const std::string& path) const;

    /// Load a document from a `.vdesheet` TOML file.
    /// Returns true on success (clears existing data first).
    bool loadFromFile(const std::string& path);

    /// Serialize to a TOML string (for testing).
    std::string serializeToString() const;

    /// Deserialize from a TOML string (for testing).
    bool deserializeFromString(const std::string& toml);

    // ── Metadata ─────────────────────────────────────────────────

    void setAuthor(const std::string& author) { m_author = author; }
    const std::string& getAuthor() const { return m_author; }

    void setNotes(const std::string& notes) { m_notes = notes; }
    const std::string& getNotes() const { return m_notes; }

  private:
    std::string m_imagePath;
    int m_imageWidth = 0;
    int m_imageHeight = 0;

    std::vector<SpriteRegion> m_sprites;
    std::vector<AnimationSequence> m_animations;

    std::string m_author;
    std::string m_notes;

    /// Internal helper: find mutable sprite by name.
    SpriteRegion* findSpriteMut(const std::string& name);

    /// Internal helper: find mutable animation by name.
    AnimationSequence* findAnimationMut(const std::string& name);
};

}  // namespace tools
}  // namespace vde
