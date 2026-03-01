/**
 * @file ImageDocument.h
 * @brief Pure pixel data model with drawing primitives and undo/redo.
 *
 * ImageDocument represents a single image canvas with RGBA pixel data.
 * It provides drawing primitives (brush, line, rect, circle, flood fill),
 * undo/redo via full-buffer snapshots, and file I/O via stb_image.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vde {
namespace tools {

/**
 * @brief RGBA color value (4 bytes).
 */
struct RGBAColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    bool operator==(const RGBAColor& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const RGBAColor& other) const { return !(*this == other); }
};

/**
 * @brief Maximum number of undo levels.
 */
static constexpr size_t kMaxUndoLevels = 50;

/**
 * @brief Pure pixel data model with drawing primitives and undo/redo.
 *
 * Provides:
 * - RGBA pixel buffer with bounds-checked access
 * - Drawing primitives: brush, line, rect, circle, flood fill
 * - Image transforms: flip, resize, crop
 * - Full-buffer snapshot undo/redo (capped at kMaxUndoLevels)
 * - File I/O via stb_image (load) and stb_image_write (save/export)
 * - Generation counter for efficient GPU texture re-upload detection
 */
class ImageDocument {
  public:
    ImageDocument() = default;
    ~ImageDocument() = default;

    // Non-copyable, movable
    ImageDocument(const ImageDocument&) = delete;
    ImageDocument& operator=(const ImageDocument&) = delete;
    ImageDocument(ImageDocument&&) = default;
    ImageDocument& operator=(ImageDocument&&) = default;

    // --- Construction ---

    /**
     * @brief Create a new blank document with transparent black pixels.
     * @param width Canvas width in pixels
     * @param height Canvas height in pixels
     */
    void createNew(uint32_t width, uint32_t height);

    /**
     * @brief Load an image from file via stb_image.
     * @param path Path to the image file (PNG, JPEG, BMP, TGA, etc.)
     * @return true if successful
     */
    bool loadFromFile(const std::string& path);

    // --- Pixel access ---

    /**
     * @brief Set a single pixel (bounds-checked, increments generation).
     */
    void setPixel(int x, int y, RGBAColor color);

    /**
     * @brief Get a single pixel (returns transparent black if out of bounds).
     */
    RGBAColor getPixel(int x, int y) const;

    /**
     * @brief Get raw pixel data pointer (RGBA, 4 bytes per pixel).
     */
    const uint8_t* getPixelData() const { return m_pixels.data(); }

    /**
     * @brief Get mutable raw pixel data pointer.
     */
    uint8_t* getPixelDataMutable() { return m_pixels.data(); }

    // --- Drawing primitives ---

    /**
     * @brief Fill entire canvas with a solid color.
     */
    void fill(RGBAColor color);

    /**
     * @brief Draw a filled circle brush stamp.
     * @param cx Center X
     * @param cy Center Y
     * @param radius Brush radius (0 = single pixel)
     * @param color Brush color
     */
    void drawBrush(int cx, int cy, int radius, RGBAColor color);

    /**
     * @brief Draw a line using Bresenham's algorithm.
     * @param x1 Start X
     * @param y1 Start Y
     * @param x2 End X
     * @param y2 End Y
     * @param color Line color
     * @param thickness Line thickness (1 = single pixel line)
     */
    void drawLine(int x1, int y1, int x2, int y2, RGBAColor color, int thickness = 1);

    /**
     * @brief Draw a rectangle (filled or outline).
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param color Rectangle color
     * @param filled True for filled, false for outline only
     */
    void drawRect(int x, int y, int w, int h, RGBAColor color, bool filled = true);

    /**
     * @brief Draw a circle using the midpoint algorithm.
     * @param cx Center X
     * @param cy Center Y
     * @param r Radius
     * @param color Circle color
     * @param filled True for filled circle, false for outline
     */
    void drawCircle(int cx, int cy, int r, RGBAColor color, bool filled = false);

    /**
     * @brief Queue-based flood fill starting from (x, y).
     * @param x Start X
     * @param y Start Y
     * @param color Fill color
     */
    void floodFill(int x, int y, RGBAColor color);

    // --- Image transforms ---

    /**
     * @brief Flip the image horizontally (mirror left-right).
     */
    void flipHorizontal();

    /**
     * @brief Flip the image vertically (mirror top-bottom).
     */
    void flipVertical();

    /**
     * @brief Resize using nearest-neighbor resampling (pixel art).
     * @param newWidth New width
     * @param newHeight New height
     */
    void resize(uint32_t newWidth, uint32_t newHeight);

    /**
     * @brief Crop to a sub-rectangle.
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     */
    void crop(int x, int y, int w, int h);

    // --- Undo/Redo ---

    /**
     * @brief Push current pixel state to undo stack. Call before mutations.
     */
    void snapshotForUndo();

    /**
     * @brief Undo the last change. Returns false if nothing to undo.
     */
    bool undo();

    /**
     * @brief Redo the last undone change. Returns false if nothing to redo.
     */
    bool redo();

    /**
     * @brief Get the number of available undo steps.
     */
    size_t getUndoCount() const { return m_undoStack.size(); }

    /**
     * @brief Get the number of available redo steps.
     */
    size_t getRedoCount() const { return m_redoStack.size(); }

    // --- Persistence ---

    /**
     * @brief Save to PNG file.
     * @param path Output file path
     * @return true if successful
     */
    bool saveToFile(const std::string& path) const;

    /**
     * @brief Export to file, detecting format from extension.
     * @param path Output file path (.png, .bmp, .tga)
     * @return true if successful
     */
    bool exportToFile(const std::string& path) const;

    // --- Accessors ---

    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint64_t getGeneration() const { return m_generation; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }
    void markDirty() { m_dirty = true; }
    const std::string& getFilePath() const { return m_filePath; }
    void setFilePath(const std::string& path) { m_filePath = path; }
    bool isValid() const { return m_width > 0 && m_height > 0 && !m_pixels.empty(); }

  private:
    /**
     * @brief Set pixel without bounds check (internal use only).
     */
    void setPixelUnchecked(int x, int y, RGBAColor color);

    /**
     * @brief Get pixel without bounds check (internal use only).
     */
    RGBAColor getPixelUnchecked(int x, int y) const;

    /**
     * @brief Check if coordinates are within bounds.
     */
    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < m_width &&
               static_cast<uint32_t>(y) < m_height;
    }

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<uint8_t> m_pixels;  ///< RGBA pixel data (4 bytes per pixel)
    uint64_t m_generation = 0;
    bool m_dirty = false;
    std::string m_filePath;

    // Undo/Redo stacks (full-buffer snapshots)
    std::vector<std::vector<uint8_t>> m_undoStack;
    std::vector<std::vector<uint8_t>> m_redoStack;
};

}  // namespace tools
}  // namespace vde
