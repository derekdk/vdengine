/**
 * @file ImageDocument.cpp
 * @brief Implementation of the ImageDocument pixel data model.
 */

#include "ImageDocument.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <queue>

#include <stb_image.h>
#include <stb_image_write.h>

namespace vde {
namespace tools {

// =============================================================================
// Construction
// =============================================================================

void ImageDocument::createNew(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_pixels.assign(static_cast<size_t>(width) * height * 4, 0);
    m_generation = 0;
    m_dirty = false;
    m_filePath.clear();
    m_undoStack.clear();
    m_redoStack.clear();
}

bool ImageDocument::loadFromFile(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        return false;
    }

    m_width = static_cast<uint32_t>(w);
    m_height = static_cast<uint32_t>(h);
    size_t dataSize = static_cast<size_t>(w) * h * 4;
    m_pixels.assign(data, data + dataSize);
    stbi_image_free(data);

    m_generation = 0;
    m_dirty = false;
    m_filePath = path;
    m_undoStack.clear();
    m_redoStack.clear();

    return true;
}

// =============================================================================
// Pixel access
// =============================================================================

void ImageDocument::setPixel(int x, int y, RGBAColor color) {
    if (!inBounds(x, y))
        return;
    setPixelUnchecked(x, y, color);
    ++m_generation;
    m_dirty = true;
}

RGBAColor ImageDocument::getPixel(int x, int y) const {
    if (!inBounds(x, y))
        return {0, 0, 0, 0};
    return getPixelUnchecked(x, y);
}

void ImageDocument::setPixelUnchecked(int x, int y, RGBAColor color) {
    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    m_pixels[idx + 0] = color.r;
    m_pixels[idx + 1] = color.g;
    m_pixels[idx + 2] = color.b;
    m_pixels[idx + 3] = color.a;
}

RGBAColor ImageDocument::getPixelUnchecked(int x, int y) const {
    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    return {m_pixels[idx + 0], m_pixels[idx + 1], m_pixels[idx + 2], m_pixels[idx + 3]};
}

// =============================================================================
// Drawing primitives
// =============================================================================

void ImageDocument::fill(RGBAColor color) {
    for (size_t i = 0; i < m_pixels.size(); i += 4) {
        m_pixels[i + 0] = color.r;
        m_pixels[i + 1] = color.g;
        m_pixels[i + 2] = color.b;
        m_pixels[i + 3] = color.a;
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawBrush(int cx, int cy, int radius, RGBAColor color) {
    if (radius <= 0) {
        if (inBounds(cx, cy)) {
            setPixelUnchecked(cx, cy, color);
        }
        ++m_generation;
        m_dirty = true;
        return;
    }

    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                int px = cx + dx;
                int py = cy + dy;
                if (inBounds(px, py)) {
                    setPixelUnchecked(px, py, color);
                }
            }
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawLine(int x1, int y1, int x2, int y2, RGBAColor color, int thickness) {
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int brushRadius = (thickness - 1) / 2;

    while (true) {
        if (brushRadius > 0) {
            // Stamp brush at each point for thick lines
            int r2 = brushRadius * brushRadius;
            for (int bdy = -brushRadius; bdy <= brushRadius; ++bdy) {
                for (int bdx = -brushRadius; bdx <= brushRadius; ++bdx) {
                    if (bdx * bdx + bdy * bdy <= r2) {
                        int px = x1 + bdx;
                        int py = y1 + bdy;
                        if (inBounds(px, py)) {
                            setPixelUnchecked(px, py, color);
                        }
                    }
                }
            }
        } else {
            if (inBounds(x1, y1)) {
                setPixelUnchecked(x1, y1, color);
            }
        }

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawRect(int x, int y, int w, int h, RGBAColor color, bool filled) {
    if (filled) {
        for (int row = y; row < y + h; ++row) {
            for (int col = x; col < x + w; ++col) {
                if (inBounds(col, row)) {
                    setPixelUnchecked(col, row, color);
                }
            }
        }
    } else {
        // Top and bottom edges
        for (int col = x; col < x + w; ++col) {
            if (inBounds(col, y))
                setPixelUnchecked(col, y, color);
            if (inBounds(col, y + h - 1))
                setPixelUnchecked(col, y + h - 1, color);
        }
        // Left and right edges
        for (int row = y; row < y + h; ++row) {
            if (inBounds(x, row))
                setPixelUnchecked(x, row, color);
            if (inBounds(x + w - 1, row))
                setPixelUnchecked(x + w - 1, row, color);
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawCircle(int cx, int cy, int r, RGBAColor color, bool filled) {
    if (r <= 0) {
        if (inBounds(cx, cy)) {
            setPixelUnchecked(cx, cy, color);
        }
        ++m_generation;
        m_dirty = true;
        return;
    }

    if (filled) {
        // Filled circle: iterate all pixels in bounding box
        int r2 = r * r;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r2) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (inBounds(px, py)) {
                        setPixelUnchecked(px, py, color);
                    }
                }
            }
        }
    } else {
        // Midpoint circle algorithm (outline only)
        int x = r;
        int y = 0;
        int err = 1 - r;

        while (x >= y) {
            // Plot 8 octant points
            if (inBounds(cx + x, cy + y))
                setPixelUnchecked(cx + x, cy + y, color);
            if (inBounds(cx - x, cy + y))
                setPixelUnchecked(cx - x, cy + y, color);
            if (inBounds(cx + x, cy - y))
                setPixelUnchecked(cx + x, cy - y, color);
            if (inBounds(cx - x, cy - y))
                setPixelUnchecked(cx - x, cy - y, color);
            if (inBounds(cx + y, cy + x))
                setPixelUnchecked(cx + y, cy + x, color);
            if (inBounds(cx - y, cy + x))
                setPixelUnchecked(cx - y, cy + x, color);
            if (inBounds(cx + y, cy - x))
                setPixelUnchecked(cx + y, cy - x, color);
            if (inBounds(cx - y, cy - x))
                setPixelUnchecked(cx - y, cy - x, color);

            ++y;
            if (err < 0) {
                err += 2 * y + 1;
            } else {
                --x;
                err += 2 * (y - x) + 1;
            }
        }
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::floodFill(int x, int y, RGBAColor color) {
    if (!inBounds(x, y))
        return;

    RGBAColor targetColor = getPixelUnchecked(x, y);
    if (targetColor == color)
        return;  // Already the fill color

    std::queue<std::pair<int, int>> queue;
    queue.push({x, y});

    while (!queue.empty()) {
        auto [px, py] = queue.front();
        queue.pop();

        if (!inBounds(px, py))
            continue;

        RGBAColor current = getPixelUnchecked(px, py);
        if (current != targetColor)
            continue;

        setPixelUnchecked(px, py, color);

        queue.push({px + 1, py});
        queue.push({px - 1, py});
        queue.push({px, py + 1});
        queue.push({px, py - 1});
    }

    ++m_generation;
    m_dirty = true;
}

// =============================================================================
// Image transforms
// =============================================================================

void ImageDocument::flipHorizontal() {
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width / 2; ++x) {
            uint32_t mirrorX = m_width - 1 - x;
            size_t idx1 = (static_cast<size_t>(y) * m_width + x) * 4;
            size_t idx2 = (static_cast<size_t>(y) * m_width + mirrorX) * 4;
            for (int c = 0; c < 4; ++c) {
                std::swap(m_pixels[idx1 + c], m_pixels[idx2 + c]);
            }
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::flipVertical() {
    for (uint32_t y = 0; y < m_height / 2; ++y) {
        uint32_t mirrorY = m_height - 1 - y;
        size_t rowSize = static_cast<size_t>(m_width) * 4;
        size_t idx1 = static_cast<size_t>(y) * rowSize;
        size_t idx2 = static_cast<size_t>(mirrorY) * rowSize;
        for (size_t i = 0; i < rowSize; ++i) {
            std::swap(m_pixels[idx1 + i], m_pixels[idx2 + i]);
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::resize(uint32_t newWidth, uint32_t newHeight) {
    if (newWidth == 0 || newHeight == 0)
        return;
    if (newWidth == m_width && newHeight == m_height)
        return;

    std::vector<uint8_t> newPixels(static_cast<size_t>(newWidth) * newHeight * 4, 0);

    // Nearest-neighbor resampling (good for pixel art)
    for (uint32_t y = 0; y < newHeight; ++y) {
        for (uint32_t x = 0; x < newWidth; ++x) {
            uint32_t srcX = x * m_width / newWidth;
            uint32_t srcY = y * m_height / newHeight;
            srcX = std::min(srcX, m_width - 1);
            srcY = std::min(srcY, m_height - 1);

            size_t srcIdx = (static_cast<size_t>(srcY) * m_width + srcX) * 4;
            size_t dstIdx = (static_cast<size_t>(y) * newWidth + x) * 4;

            newPixels[dstIdx + 0] = m_pixels[srcIdx + 0];
            newPixels[dstIdx + 1] = m_pixels[srcIdx + 1];
            newPixels[dstIdx + 2] = m_pixels[srcIdx + 2];
            newPixels[dstIdx + 3] = m_pixels[srcIdx + 3];
        }
    }

    m_width = newWidth;
    m_height = newHeight;
    m_pixels = std::move(newPixels);
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::crop(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0)
        return;

    // Clamp to canvas bounds
    int x2 = std::min(x + w, static_cast<int>(m_width));
    int y2 = std::min(y + h, static_cast<int>(m_height));
    x = std::max(x, 0);
    y = std::max(y, 0);
    int croppedW = x2 - x;
    int croppedH = y2 - y;
    if (croppedW <= 0 || croppedH <= 0)
        return;

    std::vector<uint8_t> newPixels(static_cast<size_t>(croppedW) * croppedH * 4, 0);

    for (int row = 0; row < croppedH; ++row) {
        size_t srcIdx = (static_cast<size_t>(y + row) * m_width + x) * 4;
        size_t dstIdx = static_cast<size_t>(row) * croppedW * 4;
        std::memcpy(&newPixels[dstIdx], &m_pixels[srcIdx], static_cast<size_t>(croppedW) * 4);
    }

    m_width = static_cast<uint32_t>(croppedW);
    m_height = static_cast<uint32_t>(croppedH);
    m_pixels = std::move(newPixels);
    ++m_generation;
    m_dirty = true;
}

// =============================================================================
// Undo/Redo
// =============================================================================

void ImageDocument::snapshotForUndo() {
    m_undoStack.push_back(m_pixels);

    // Cap the undo stack
    if (m_undoStack.size() > kMaxUndoLevels) {
        m_undoStack.erase(m_undoStack.begin());
    }

    // Any new action clears the redo stack
    m_redoStack.clear();
}

bool ImageDocument::undo() {
    if (m_undoStack.empty())
        return false;

    m_redoStack.push_back(std::move(m_pixels));
    m_pixels = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    ++m_generation;
    m_dirty = true;
    return true;
}

bool ImageDocument::redo() {
    if (m_redoStack.empty())
        return false;

    m_undoStack.push_back(std::move(m_pixels));
    m_pixels = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    ++m_generation;
    m_dirty = true;
    return true;
}

// =============================================================================
// Persistence
// =============================================================================

bool ImageDocument::saveToFile(const std::string& path) const {
    if (m_pixels.empty() || m_width == 0 || m_height == 0)
        return false;

    int stride = static_cast<int>(m_width) * 4;
    int result = stbi_write_png(path.c_str(), static_cast<int>(m_width), static_cast<int>(m_height),
                                4, m_pixels.data(), stride);
    return result != 0;
}

bool ImageDocument::exportToFile(const std::string& path) const {
    if (m_pixels.empty() || m_width == 0 || m_height == 0)
        return false;

    std::string ext = std::filesystem::path(path).extension().string();
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    int w = static_cast<int>(m_width);
    int h = static_cast<int>(m_height);

    if (ext == ".png") {
        return stbi_write_png(path.c_str(), w, h, 4, m_pixels.data(), w * 4) != 0;
    } else if (ext == ".bmp") {
        return stbi_write_bmp(path.c_str(), w, h, 4, m_pixels.data()) != 0;
    } else if (ext == ".tga") {
        return stbi_write_tga(path.c_str(), w, h, 4, m_pixels.data()) != 0;
    }

    // Default to PNG if unknown extension
    return stbi_write_png(path.c_str(), w, h, 4, m_pixels.data(), w * 4) != 0;
}

}  // namespace tools
}  // namespace vde
