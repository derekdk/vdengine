#pragma once

/**
 * @file SpriteSheet.h
 * @brief Sprite sheet / atlas management for VDE.
 *
 * Provides a SpriteSheet resource that maps named or indexed sub-regions
 * of a texture atlas to UV rectangles, eliminating manual UV math for frame
 * animation and atlas-based sprite rendering.
 *
 * SpriteSheet is a Resource, so it can be managed by Scene::addResource()
 * and shared across scenes via ResourceManager.
 */

#include <vde/Texture.h>
#include <vde/api/Resource.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

/**
 * @brief Manages a texture atlas with indexed and named sprite regions.
 *
 * Two creation modes:
 * - **Grid:** Uniform columns × rows layout with optional pixel spacing.
 * - **Manual:** Call addSprite() to define each named region individually.
 *
 * Created programmatically (not loaded from a file), then registered with
 * a Scene via addResource() or cached in ResourceManager for cross-scene
 * sharing.
 *
 * Usage:
 * @code
 * // Grid-based spritesheet (4 columns, 2 rows)
 * auto sheet = SpriteSheet::createGrid(texture, 4, 2);
 * scene->addResource<SpriteSheet>(sheet);
 * auto uv = sheet->getUVRect(0); // first frame
 *
 * // Manual named regions
 * auto atlas = SpriteSheet::create(texture);
 * atlas->addSprite("idle", 0, 0, 32, 32);
 * atlas->addSprite("run",  32, 0, 32, 32);
 * auto uv2 = atlas->getUVRect("idle");
 * @endcode
 */
class SpriteSheet : public Resource {
  public:
    using Ref = std::shared_ptr<SpriteSheet>;

    /**
     * @brief UV rectangle in normalized texture coordinates.
     */
    struct UVRect {
        float u = 0.0f;
        float v = 0.0f;
        float width = 1.0f;
        float height = 1.0f;
    };

    /**
     * @brief Create a grid-based spritesheet.
     * @param texture The source texture atlas.
     * @param columns Number of columns in the grid.
     * @param rows Number of rows in the grid.
     * @param spacingPx Optional pixel spacing between cells (must be >= 0).
     * @return Shared pointer to the new SpriteSheet.
     * @throws std::invalid_argument if texture is null, columns/rows <= 0,
     *         spacingPx < 0, or spacing produces non-positive cell dimensions.
     */
    static Ref createGrid(std::shared_ptr<Texture> texture, int columns, int rows,
                          int spacingPx = 0);

    /**
     * @brief Create an empty spritesheet for manual region definitions.
     * @param texture The source texture atlas.
     * @return Shared pointer to the new SpriteSheet.
     * @throws std::invalid_argument if texture is null.
     */
    static Ref create(std::shared_ptr<Texture> texture);

    /**
     * @brief Add a named sprite region (pixel coordinates).
     * @param name Unique name for this region.
     * @param x Left edge in pixels (must be >= 0).
     * @param y Top edge in pixels (must be >= 0).
     * @param w Width in pixels (must be > 0).
     * @param h Height in pixels (must be > 0).
     * @throws std::invalid_argument if name is empty, already exists, or
     *         w/h <= 0.
     * @throws std::out_of_range if region extends beyond texture bounds.
     */
    void addSprite(const std::string& name, int x, int y, int w, int h);

    /**
     * @brief Get a UV rect by numeric index.
     * @param index Zero-based sprite index.
     * @return The UV rectangle for the requested sprite.
     * @throws std::out_of_range if index is out of bounds.
     */
    UVRect getUVRect(int index) const;

    /**
     * @brief Get a UV rect by name.
     * @param name The sprite region name.
     * @return The UV rectangle for the requested sprite.
     * @throws std::out_of_range if name is not found.
     */
    UVRect getUVRect(const std::string& name) const;

    /**
     * @brief Get the underlying texture.
     */
    std::shared_ptr<Texture> getTexture() const { return m_texture; }

    /**
     * @brief Get the total number of sprite regions.
     */
    int getSpriteCount() const { return static_cast<int>(m_rects.size()); }

    // -- Resource interface --------------------------------------------------

    const char* getTypeName() const override { return "SpriteSheet"; }

  private:
    SpriteSheet() = default;

    std::shared_ptr<Texture> m_texture;
    std::vector<UVRect> m_rects;
    std::unordered_map<std::string, int> m_nameIndex;
};

}  // namespace vde
