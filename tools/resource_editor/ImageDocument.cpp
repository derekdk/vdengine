/**
 * @file ImageDocument.cpp
 * @brief Implementation of the ImageDocument pixel buffer model.
 */

#include "ImageDocument.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stack>

#include "stb_image.h"
#include "stb_image_write.h"

namespace vde {
namespace tools {

// =============================================================================
// Factory methods
// =============================================================================

std::unique_ptr<ImageDocument> ImageDocument::createNew(uint32_t w, uint32_t h) {
    auto doc = std::unique_ptr<ImageDocument>(new ImageDocument());
    doc->m_width = w;
    doc->m_height = h;
    doc->m_pixels.resize(static_cast<size_t>(w) * h * 4, 0);  // Transparent black
    return doc;
}

std::unique_ptr<ImageDocument> ImageDocument::loadFromFile(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // Force RGBA
    if (!data) {
        return nullptr;
    }

    auto doc = std::unique_ptr<ImageDocument>(new ImageDocument());
    doc->m_width = static_cast<uint32_t>(w);
    doc->m_height = static_cast<uint32_t>(h);
    size_t byteCount = static_cast<size_t>(w) * h * 4;
    doc->m_pixels.assign(data, data + byteCount);
    doc->m_filePath = path;
    stbi_image_free(data);
    return doc;
}

// =============================================================================
// Pixel access
// =============================================================================

void ImageDocument::setPixel(uint32_t x, uint32_t y, RGBAColor color) {
    if (x >= m_width || y >= m_height) {
        return;
    }
    setPixelUnchecked(x, y, color);
    ++m_generation;
    m_dirty = true;
}

RGBAColor ImageDocument::getPixel(uint32_t x, uint32_t y) const {
    if (x >= m_width || y >= m_height) {
        return RGBAColor{0, 0, 0, 0};
    }
    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    return RGBAColor{m_pixels[idx], m_pixels[idx + 1], m_pixels[idx + 2], m_pixels[idx + 3]};
}

const uint8_t* ImageDocument::getPixelData() const {
    return m_pixels.data();
}

uint32_t ImageDocument::getWidth() const {
    return m_width;
}

uint32_t ImageDocument::getHeight() const {
    return m_height;
}

void ImageDocument::setPixelUnchecked(uint32_t x, uint32_t y, RGBAColor color) {
    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    m_pixels[idx + 0] = color.r;
    m_pixels[idx + 1] = color.g;
    m_pixels[idx + 2] = color.b;
    m_pixels[idx + 3] = color.a;
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

void ImageDocument::drawLine(int x1, int y1, int x2, int y2, RGBAColor color, int thickness) {
    // Bresenham's line algorithm
    auto plotPixel = [&](int px, int py) {
        if (thickness <= 1) {
            if (px >= 0 && py >= 0 && static_cast<uint32_t>(px) < m_width &&
                static_cast<uint32_t>(py) < m_height) {
                setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
            }
        } else {
            // Draw filled circle at each point for thick lines
            int r = thickness / 2;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy <= r * r) {
                        int fx = px + dx;
                        int fy = py + dy;
                        if (fx >= 0 && fy >= 0 && static_cast<uint32_t>(fx) < m_width &&
                            static_cast<uint32_t>(fy) < m_height) {
                            setPixelUnchecked(static_cast<uint32_t>(fx), static_cast<uint32_t>(fy),
                                              color);
                        }
                    }
                }
            }
        }
    };

    int dx = std::abs(x2 - x1);
    int dy = -std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        plotPixel(x1, y1);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
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
                if (col >= 0 && row >= 0 && static_cast<uint32_t>(col) < m_width &&
                    static_cast<uint32_t>(row) < m_height) {
                    setPixelUnchecked(static_cast<uint32_t>(col), static_cast<uint32_t>(row),
                                      color);
                }
            }
        }
    } else {
        // Four edges
        drawLine(x, y, x + w - 1, y, color);                  // Top
        drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);  // Bottom
        drawLine(x, y, x, y + h - 1, color);                  // Left
        drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);  // Right
        return;                                               // drawLine already bumps generation
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawCircle(int cx, int cy, int r, RGBAColor color, bool filled) {
    if (r <= 0) {
        if (cx >= 0 && cy >= 0 && static_cast<uint32_t>(cx) < m_width &&
            static_cast<uint32_t>(cy) < m_height) {
            setPixelUnchecked(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), color);
        }
        ++m_generation;
        m_dirty = true;
        return;
    }

    // Helper to set a clamped pixel
    auto safePlot = [&](int px, int py) {
        if (px >= 0 && py >= 0 && static_cast<uint32_t>(px) < m_width &&
            static_cast<uint32_t>(py) < m_height) {
            setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    };

    // Helper to draw a horizontal span (clamped)
    auto hLine = [&](int lx, int rx, int py) {
        if (py < 0 || static_cast<uint32_t>(py) >= m_height) {
            return;
        }
        lx = std::max(lx, 0);
        rx = std::min(rx, static_cast<int>(m_width) - 1);
        for (int px = lx; px <= rx; ++px) {
            setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    };

    // Midpoint circle algorithm
    int x = 0;
    int y = r;
    int d = 1 - r;

    while (x <= y) {
        if (filled) {
            hLine(cx - x, cx + x, cy + y);
            hLine(cx - x, cx + x, cy - y);
            hLine(cx - y, cx + y, cy + x);
            hLine(cx - y, cx + y, cy - x);
        } else {
            safePlot(cx + x, cy + y);
            safePlot(cx - x, cy + y);
            safePlot(cx + x, cy - y);
            safePlot(cx - x, cy - y);
            safePlot(cx + y, cy + x);
            safePlot(cx - y, cy + x);
            safePlot(cx + y, cy - x);
            safePlot(cx - y, cy - x);
        }

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            --y;
        }
        ++x;
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawEllipse(int cx, int cy, int rx, int ry, RGBAColor color, bool filled) {
    if (rx <= 0 && ry <= 0) {
        if (cx >= 0 && cy >= 0 && static_cast<uint32_t>(cx) < m_width &&
            static_cast<uint32_t>(cy) < m_height) {
            setPixelUnchecked(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), color);
        }
        ++m_generation;
        m_dirty = true;
        return;
    }

    auto safePlot = [&](int px, int py) {
        if (px >= 0 && py >= 0 && static_cast<uint32_t>(px) < m_width &&
            static_cast<uint32_t>(py) < m_height) {
            setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    };

    auto hLine = [&](int lx, int rxEnd, int py) {
        if (py < 0 || static_cast<uint32_t>(py) >= m_height) {
            return;
        }
        lx = std::max(lx, 0);
        rxEnd = std::min(rxEnd, static_cast<int>(m_width) - 1);
        for (int px = lx; px <= rxEnd; ++px) {
            setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    };

    // Midpoint ellipse algorithm
    long long rxSq = static_cast<long long>(rx) * rx;
    long long rySq = static_cast<long long>(ry) * ry;

    int x = 0;
    int y = ry;
    long long dx = 2 * rySq * x;
    long long dy = 2 * rxSq * y;

    // Region 1: slope magnitude < 1
    long long p1 = rySq - rxSq * ry + rxSq / 4;
    while (dx < dy) {
        if (filled) {
            hLine(cx - x, cx + x, cy + y);
            hLine(cx - x, cx + x, cy - y);
        } else {
            safePlot(cx + x, cy + y);
            safePlot(cx - x, cy + y);
            safePlot(cx + x, cy - y);
            safePlot(cx - x, cy - y);
        }

        ++x;
        dx += 2 * rySq;
        if (p1 < 0) {
            p1 += dx + rySq;
        } else {
            --y;
            dy -= 2 * rxSq;
            p1 += dx - dy + rySq;
        }
    }

    // Region 2: slope magnitude >= 1
    long long p2 = rySq * (static_cast<long long>(x) * x + x) +
                   rxSq * (static_cast<long long>(y - 1) * (y - 1)) - rxSq * rySq;
    // Adjust: use standard formulation
    p2 = rySq * (2 * x + 1) * (2 * x + 1) / 4 + rxSq * (static_cast<long long>(y) - 1) * (y - 1) -
         rxSq * rySq;
    // Simpler: recompute
    {
        long long tx = x;
        long long ty = y;
        p2 = rySq * (tx + 1) * (tx + 1) + rxSq * (ty - 1) * (ty - 1) - rxSq * rySq;
    }

    while (y >= 0) {
        if (filled) {
            hLine(cx - x, cx + x, cy + y);
            hLine(cx - x, cx + x, cy - y);
        } else {
            safePlot(cx + x, cy + y);
            safePlot(cx - x, cy + y);
            safePlot(cx + x, cy - y);
            safePlot(cx - x, cy - y);
        }

        --y;
        dy -= 2 * rxSq;
        if (p2 > 0) {
            p2 += rxSq - dy;
        } else {
            ++x;
            dx += 2 * rySq;
            p2 += dx - dy + rxSq;
        }
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawArc(int cx, int cy, int r, float startAngle, float endAngle,
                            RGBAColor color, int thickness) {
    if (r <= 0) {
        return;
    }

    auto plotPixel = [&](int px, int py) {
        if (thickness <= 1) {
            if (px >= 0 && py >= 0 && static_cast<uint32_t>(px) < m_width &&
                static_cast<uint32_t>(py) < m_height) {
                setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
            }
        } else {
            int hr = thickness / 2;
            for (int dy = -hr; dy <= hr; ++dy) {
                for (int dx = -hr; dx <= hr; ++dx) {
                    if (dx * dx + dy * dy <= hr * hr) {
                        int fx = px + dx;
                        int fy = py + dy;
                        if (fx >= 0 && fy >= 0 && static_cast<uint32_t>(fx) < m_width &&
                            static_cast<uint32_t>(fy) < m_height) {
                            setPixelUnchecked(static_cast<uint32_t>(fx), static_cast<uint32_t>(fy),
                                              color);
                        }
                    }
                }
            }
        }
    };

    // Convert degrees to radians
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    float startRad = startAngle * kDegToRad;
    float endRad = endAngle * kDegToRad;

    // Normalize so we always sweep forward
    while (endRad < startRad) {
        endRad += 2.0f * 3.14159265358979323846f;
    }

    // Step in ~0.5-degree increments
    float step = 0.5f * kDegToRad;
    for (float a = startRad; a <= endRad; a += step) {
        int px = cx + static_cast<int>(std::round(r * std::cos(a)));
        int py = cy + static_cast<int>(std::round(r * std::sin(a)));
        plotPixel(px, py);
    }

    // Always plot the endpoint
    {
        int px = cx + static_cast<int>(std::round(r * std::cos(endRad)));
        int py = cy + static_cast<int>(std::round(r * std::sin(endRad)));
        plotPixel(px, py);
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::drawBezier(const std::vector<std::pair<int, int>>& points, RGBAColor color,
                               int thickness) {
    if (points.size() != 4) {
        return;
    }

    auto plotPixel = [&](int px, int py) {
        if (thickness <= 1) {
            if (px >= 0 && py >= 0 && static_cast<uint32_t>(px) < m_width &&
                static_cast<uint32_t>(py) < m_height) {
                setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
            }
        } else {
            int hr = thickness / 2;
            for (int dy = -hr; dy <= hr; ++dy) {
                for (int dx = -hr; dx <= hr; ++dx) {
                    if (dx * dx + dy * dy <= hr * hr) {
                        int fx = px + dx;
                        int fy = py + dy;
                        if (fx >= 0 && fy >= 0 && static_cast<uint32_t>(fx) < m_width &&
                            static_cast<uint32_t>(fy) < m_height) {
                            setPixelUnchecked(static_cast<uint32_t>(fx), static_cast<uint32_t>(fy),
                                              color);
                        }
                    }
                }
            }
        }
    };

    float p0x = static_cast<float>(points[0].first);
    float p0y = static_cast<float>(points[0].second);
    float p1x = static_cast<float>(points[1].first);
    float p1y = static_cast<float>(points[1].second);
    float p2x = static_cast<float>(points[2].first);
    float p2y = static_cast<float>(points[2].second);
    float p3x = static_cast<float>(points[3].first);
    float p3y = static_cast<float>(points[3].second);

    // Adaptive step count based on rough arc length estimate
    float chordLen = std::sqrt((p3x - p0x) * (p3x - p0x) + (p3y - p0y) * (p3y - p0y));
    float controlLen = std::sqrt((p1x - p0x) * (p1x - p0x) + (p1y - p0y) * (p1y - p0y)) +
                       std::sqrt((p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y)) +
                       std::sqrt((p3x - p2x) * (p3x - p2x) + (p3y - p2y) * (p3y - p2y));
    int steps = std::max(100, static_cast<int>(controlLen + chordLen) * 2);

    float dt = 1.0f / static_cast<float>(steps);
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) * dt;
        float u = 1.0f - t;
        float u2 = u * u;
        float u3 = u2 * u;
        float t2 = t * t;
        float t3 = t2 * t;

        float bx = u3 * p0x + 3.0f * u2 * t * p1x + 3.0f * u * t2 * p2x + t3 * p3x;
        float by = u3 * p0y + 3.0f * u2 * t * p1y + 3.0f * u * t2 * p2y + t3 * p3y;

        plotPixel(static_cast<int>(std::round(bx)), static_cast<int>(std::round(by)));
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::floodFill(int x, int y, RGBAColor color) {
    if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= m_width ||
        static_cast<uint32_t>(y) >= m_height) {
        return;
    }

    RGBAColor target = getPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y));

    // Don't fill if target color is the same as fill color
    if (target.r == color.r && target.g == color.g && target.b == color.b && target.a == color.a) {
        return;
    }

    std::stack<std::pair<int, int>> stack;
    stack.push({x, y});

    while (!stack.empty()) {
        auto [px, py] = stack.top();
        stack.pop();

        if (px < 0 || py < 0 || static_cast<uint32_t>(px) >= m_width ||
            static_cast<uint32_t>(py) >= m_height) {
            continue;
        }

        RGBAColor current = getPixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py));
        if (current.r != target.r || current.g != target.g || current.b != target.b ||
            current.a != target.a) {
            continue;
        }

        setPixelUnchecked(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);

        stack.push({px + 1, py});
        stack.push({px - 1, py});
        stack.push({px, py + 1});
        stack.push({px, py - 1});
    }

    ++m_generation;
    m_dirty = true;
}

void ImageDocument::flipHorizontal() {
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width / 2; ++x) {
            uint32_t mirrorX = m_width - 1 - x;
            size_t idxA = (static_cast<size_t>(y) * m_width + x) * 4;
            size_t idxB = (static_cast<size_t>(y) * m_width + mirrorX) * 4;
            for (int c = 0; c < 4; ++c) {
                std::swap(m_pixels[idxA + c], m_pixels[idxB + c]);
            }
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::flipVertical() {
    for (uint32_t y = 0; y < m_height / 2; ++y) {
        uint32_t mirrorY = m_height - 1 - y;
        size_t rowA = static_cast<size_t>(y) * m_width * 4;
        size_t rowB = static_cast<size_t>(mirrorY) * m_width * 4;
        for (size_t i = 0; i < static_cast<size_t>(m_width) * 4; ++i) {
            std::swap(m_pixels[rowA + i], m_pixels[rowB + i]);
        }
    }
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::resize(uint32_t newW, uint32_t newH) {
    if (newW == 0 || newH == 0) {
        return;
    }

    std::vector<uint8_t> newPixels(static_cast<size_t>(newW) * newH * 4, 0);

    // Nearest-neighbor sampling
    for (uint32_t ny = 0; ny < newH; ++ny) {
        for (uint32_t nx = 0; nx < newW; ++nx) {
            uint32_t srcX = static_cast<uint32_t>(static_cast<float>(nx) * m_width / newW);
            uint32_t srcY = static_cast<uint32_t>(static_cast<float>(ny) * m_height / newH);
            srcX = std::min(srcX, m_width - 1);
            srcY = std::min(srcY, m_height - 1);

            size_t srcIdx = (static_cast<size_t>(srcY) * m_width + srcX) * 4;
            size_t dstIdx = (static_cast<size_t>(ny) * newW + nx) * 4;
            newPixels[dstIdx + 0] = m_pixels[srcIdx + 0];
            newPixels[dstIdx + 1] = m_pixels[srcIdx + 1];
            newPixels[dstIdx + 2] = m_pixels[srcIdx + 2];
            newPixels[dstIdx + 3] = m_pixels[srcIdx + 3];
        }
    }

    m_pixels = std::move(newPixels);
    m_width = newW;
    m_height = newH;
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::replacePixels(std::vector<uint8_t> pixels, uint32_t newW, uint32_t newH) {
    m_pixels = std::move(pixels);
    m_width = newW;
    m_height = newH;
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::crop(int x, int y, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) {
        return;
    }

    std::vector<uint8_t> newPixels(static_cast<size_t>(w) * h * 4, 0);

    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; ++col) {
            int srcX = x + static_cast<int>(col);
            int srcY = y + static_cast<int>(row);
            if (srcX >= 0 && srcY >= 0 && static_cast<uint32_t>(srcX) < m_width &&
                static_cast<uint32_t>(srcY) < m_height) {
                size_t srcIdx = (static_cast<size_t>(srcY) * m_width + srcX) * 4;
                size_t dstIdx = (static_cast<size_t>(row) * w + col) * 4;
                newPixels[dstIdx + 0] = m_pixels[srcIdx + 0];
                newPixels[dstIdx + 1] = m_pixels[srcIdx + 1];
                newPixels[dstIdx + 2] = m_pixels[srcIdx + 2];
                newPixels[dstIdx + 3] = m_pixels[srcIdx + 3];
            }
        }
    }

    m_pixels = std::move(newPixels);
    m_width = w;
    m_height = h;
    ++m_generation;
    m_dirty = true;
}

void ImageDocument::clear() {
    fill(RGBAColor{0, 0, 0, 0});
}

// =============================================================================
// Undo / Redo
// =============================================================================

void ImageDocument::snapshotForUndo() {
    m_undoStack.push_back({m_pixels, m_width, m_height});
    if (m_undoStack.size() > kMaxUndoLevels) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
}

bool ImageDocument::undo() {
    if (m_undoStack.empty()) {
        return false;
    }
    m_redoStack.push_back({m_pixels, m_width, m_height});
    auto& snap = m_undoStack.back();
    m_pixels = std::move(snap.pixels);
    m_width = snap.width;
    m_height = snap.height;
    m_undoStack.pop_back();
    ++m_generation;
    m_dirty = true;
    return true;
}

bool ImageDocument::redo() {
    if (m_redoStack.empty()) {
        return false;
    }
    m_undoStack.push_back({m_pixels, m_width, m_height});
    auto& snap = m_redoStack.back();
    m_pixels = std::move(snap.pixels);
    m_width = snap.width;
    m_height = snap.height;
    m_redoStack.pop_back();
    ++m_generation;
    m_dirty = true;
    return true;
}

size_t ImageDocument::getUndoCount() const {
    return m_undoStack.size();
}

size_t ImageDocument::getRedoCount() const {
    return m_redoStack.size();
}

// =============================================================================
// Persistence
// =============================================================================

/**
 * @brief Detect image format from file extension.
 * @param path File path.
 * @return Lowercase extension without the dot, e.g. "png", "bmp", "tga".
 */
static std::string detectFormat(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "png";
    }
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool ImageDocument::saveToFile(const std::string& path) {
    std::string fmt = detectFormat(path);
    int w = static_cast<int>(m_width);
    int h = static_cast<int>(m_height);
    int stride = w * 4;
    int result = 0;

    if (fmt == "bmp") {
        result = stbi_write_bmp(path.c_str(), w, h, 4, m_pixels.data());
    } else if (fmt == "tga") {
        result = stbi_write_tga(path.c_str(), w, h, 4, m_pixels.data());
    } else {
        // Default to PNG
        result = stbi_write_png(path.c_str(), w, h, 4, m_pixels.data(), stride);
    }

    if (result) {
        m_filePath = path;
        m_dirty = false;
    }
    return result != 0;
}

bool ImageDocument::exportToFile(const std::string& path) {
    return saveToFile(path);
}

// =============================================================================
// Sync / state
// =============================================================================

uint64_t ImageDocument::getGeneration() const {
    return m_generation;
}

bool ImageDocument::isDirty() const {
    return m_dirty;
}

void ImageDocument::clearDirty() {
    m_dirty = false;
}

const std::string& ImageDocument::getFilePath() const {
    return m_filePath;
}

void ImageDocument::setFilePath(const std::string& path) {
    m_filePath = path;
}

}  // namespace tools
}  // namespace vde
