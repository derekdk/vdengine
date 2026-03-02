#pragma once

/**
 * @file ImageDocument.h
 * @brief Pure pixel data model for the Resource Editor.
 *
 * ImageDocument is an in-memory RGBA pixel buffer with drawing
 * primitives, undo/redo, and file I/O.  It has no knowledge of
 * canvases, commands, GPU textures, or any UI layer.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "commands/CommandTypes.h"

namespace vde::tools {

/**
 * @brief RGBA pixel buffer with drawing primitives and undo/redo.
 *
 * Every mutation bumps the generation counter and sets the dirty flag.
 * Callers snapshot before a group of edits and can undo/redo the
 * entire snapshot afterwards.
 */
class ImageDocument {
  public:
    /**
     * @brief Create a blank image filled with transparent black.
     * @param w Width in pixels.
     * @param h Height in pixels.
     * @return Owning pointer to the new document.
     */
    static std::unique_ptr<ImageDocument> createNew(uint32_t w, uint32_t h);

    /**
     * @brief Load an image from disk via stb_image (forced RGBA).
     * @param path Filesystem path to the image file.
     * @return Owning pointer, or nullptr on failure.
     */
    static std::unique_ptr<ImageDocument> loadFromFile(const std::string& path);

    // -------------------------------------------------------------------------
    // Pixel access
    // -------------------------------------------------------------------------

    /**
     * @brief Set a single pixel (bounds-checked).
     * @param x Pixel column.
     * @param y Pixel row.
     * @param color RGBA color to write.
     */
    void setPixel(uint32_t x, uint32_t y, RGBAColor color);

    /**
     * @brief Read a single pixel (bounds-checked).
     * @param x Pixel column.
     * @param y Pixel row.
     * @return RGBA color at (x, y), or transparent black if out of bounds.
     */
    RGBAColor getPixel(uint32_t x, uint32_t y) const;

    /** @brief Raw pointer to the RGBA pixel data (4 bytes per pixel). */
    const uint8_t* getPixelData() const;

    /** @brief Image width in pixels. */
    uint32_t getWidth() const;

    /** @brief Image height in pixels. */
    uint32_t getHeight() const;

    // -------------------------------------------------------------------------
    // Drawing primitives (all coordinates in pixels)
    // -------------------------------------------------------------------------

    /** @brief Fill the entire image with a single color. */
    void fill(RGBAColor color);

    /**
     * @brief Draw a line using Bresenham's algorithm.
     * @param x1 Start X.
     * @param y1 Start Y.
     * @param x2 End X.
     * @param y2 End Y.
     * @param color Line color.
     * @param thickness Line thickness (1 = single pixel).
     */
    void drawLine(int x1, int y1, int x2, int y2, RGBAColor color, int thickness = 1);

    /**
     * @brief Draw an axis-aligned rectangle.
     * @param x Top-left X.
     * @param y Top-left Y.
     * @param w Width.
     * @param h Height.
     * @param color Stroke/fill color.
     * @param filled If true, fill the interior; otherwise draw outline only.
     */
    void drawRect(int x, int y, int w, int h, RGBAColor color, bool filled);

    /**
     * @brief Draw a circle using the midpoint algorithm.
     * @param cx Center X.
     * @param cy Center Y.
     * @param r  Radius.
     * @param color Stroke/fill color.
     * @param filled If true, fill the interior.
     */
    void drawCircle(int cx, int cy, int r, RGBAColor color, bool filled);

    /**
     * @brief Draw an ellipse using the midpoint algorithm.
     * @param cx Center X.
     * @param cy Center Y.
     * @param rx Horizontal radius.
     * @param ry Vertical radius.
     * @param color Stroke/fill color.
     * @param filled If true, fill the interior.
     */
    void drawEllipse(int cx, int cy, int rx, int ry, RGBAColor color, bool filled);

    /**
     * @brief Stack-based flood fill starting at (x, y).
     * @param x Seed X.
     * @param y Seed Y.
     * @param color Fill color.
     */
    void floodFill(int x, int y, RGBAColor color);

    /** @brief Mirror the image left-to-right. */
    void flipHorizontal();

    /** @brief Mirror the image top-to-bottom. */
    void flipVertical();

    /**
     * @brief Resize using nearest-neighbor sampling.
     * @param newW New width.
     * @param newH New height.
     */
    void resize(uint32_t newW, uint32_t newH);

    /**
     * @brief Replace the pixel buffer in-place with new data and dimensions.
     *
     * Preserves the undo stack, file path, and other state.  Useful for
     * operations that change the image dimensions (e.g. 90/270-degree rotation).
     *
     * @param pixels New RGBA pixel data (must be exactly newW * newH * 4 bytes).
     * @param newW   New image width.
     * @param newH   New image height.
     */
    void replacePixels(std::vector<uint8_t> pixels, uint32_t newW, uint32_t newH);

    /**
     * @brief Crop to a sub-region.
     * @param x Top-left X of the crop rectangle.
     * @param y Top-left Y of the crop rectangle.
     * @param w Width.
     * @param h Height.
     */
    void crop(int x, int y, uint32_t w, uint32_t h);

    /** @brief Fill the image with transparent black (0,0,0,0). */
    void clear();

    // -------------------------------------------------------------------------
    // Undo / Redo
    // -------------------------------------------------------------------------

    /** @brief Push a full-buffer snapshot onto the undo stack. */
    void snapshotForUndo();

    /** @brief Restore the previous snapshot. @return false if nothing to undo. */
    bool undo();

    /** @brief Re-apply a previously undone snapshot. @return false if nothing to redo. */
    bool redo();

    /** @brief Number of available undo levels. */
    size_t getUndoCount() const;

    /** @brief Number of available redo levels. */
    size_t getRedoCount() const;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    /**
     * @brief Save to the document's current file path.
     * @param path Output file path.
     * @return true on success.
     */
    bool saveToFile(const std::string& path);

    /**
     * @brief Export to an arbitrary path (format by extension).
     * @param path Output file path (.png, .bmp, .tga).
     * @return true on success.
     */
    bool exportToFile(const std::string& path);

    // -------------------------------------------------------------------------
    // Sync / state
    // -------------------------------------------------------------------------

    /** @brief Monotonically increasing counter bumped by every mutation. */
    uint64_t getGeneration() const;

    /** @brief True if the document has been modified since the last clearDirty(). */
    bool isDirty() const;

    /** @brief Reset the dirty flag (e.g. after saving). */
    void clearDirty();

    /** @brief File path associated with this document (may be empty). */
    const std::string& getFilePath() const;

    /** @brief Associate a file path with this document. */
    void setFilePath(const std::string& path);

  private:
    ImageDocument() = default;

    /** @brief Set pixel without bounds checking (internal drawing ops). */
    void setPixelUnchecked(uint32_t x, uint32_t y, RGBAColor color);

    std::vector<uint8_t> m_pixels;  ///< RGBA, 4 bytes per pixel.
    uint32_t m_width = 0;           ///< Image width in pixels.
    uint32_t m_height = 0;          ///< Image height in pixels.
    std::string m_filePath;         ///< Associated file path.
    bool m_dirty = false;           ///< True if modified since last save/clear.
    uint64_t m_generation = 0;      ///< Mutation counter.

    /** @brief Full document state captured for undo/redo. */
    struct Snapshot {
        std::vector<uint8_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    std::vector<Snapshot> m_undoStack;            ///< Undo history.
    std::vector<Snapshot> m_redoStack;            ///< Redo future.
    static constexpr size_t kMaxUndoLevels = 50;  ///< Maximum undo depth.
};

}  // namespace vde::tools
