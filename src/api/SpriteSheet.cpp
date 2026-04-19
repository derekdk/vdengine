#include <vde/api/SpriteSheet.h>

#include <stdexcept>

namespace vde {

SpriteSheet::Ref SpriteSheet::createGrid(std::shared_ptr<Texture> texture, int columns, int rows,
                                         int spacingPx) {
    if (!texture) {
        throw std::invalid_argument("SpriteSheet::createGrid: texture must not be null");
    }
    if (columns <= 0 || rows <= 0) {
        throw std::invalid_argument("SpriteSheet::createGrid: columns and rows must be > 0");
    }
    if (spacingPx < 0) {
        throw std::invalid_argument("SpriteSheet::createGrid: spacingPx must be >= 0");
    }

    auto sheet = Ref(new SpriteSheet());
    sheet->m_texture = std::move(texture);

    int texW = static_cast<int>(sheet->m_texture->getWidth());
    int texH = static_cast<int>(sheet->m_texture->getHeight());

    if (texW <= 0 || texH <= 0) {
        throw std::invalid_argument("SpriteSheet::createGrid: texture dimensions must be > 0");
    }

    // Compute cell size in pixels (account for spacing between cells)
    int totalSpacingX = spacingPx * (columns - 1);
    int totalSpacingY = spacingPx * (rows - 1);
    int usableW = texW - totalSpacingX;
    int usableH = texH - totalSpacingY;
    int cellW = usableW / columns;
    int cellH = usableH / rows;

    if (cellW <= 0 || cellH <= 0) {
        throw std::invalid_argument(
            "SpriteSheet::createGrid: spacing too large — computed cell size is non-positive");
    }

    if (usableW % columns != 0 || usableH % rows != 0) {
        throw std::invalid_argument(
            "SpriteSheet::createGrid: texture dimensions (minus spacing) are not evenly "
            "divisible by the grid size — remainder pixels would be silently dropped");
    }

    float invW = (texW > 0) ? 1.0f / static_cast<float>(texW) : 0.0f;
    float invH = (texH > 0) ? 1.0f / static_cast<float>(texH) : 0.0f;

    sheet->m_rects.reserve(static_cast<size_t>(columns * rows));

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int px = col * (cellW + spacingPx);
            int py = row * (cellH + spacingPx);

            UVRect rect;
            rect.u = static_cast<float>(px) * invW;
            rect.v = static_cast<float>(py) * invH;
            rect.width = static_cast<float>(cellW) * invW;
            rect.height = static_cast<float>(cellH) * invH;

            sheet->m_rects.push_back(rect);
        }
    }

    sheet->m_loaded = true;
    return sheet;
}

SpriteSheet::Ref SpriteSheet::create(std::shared_ptr<Texture> texture) {
    if (!texture) {
        throw std::invalid_argument("SpriteSheet::create: texture must not be null");
    }

    auto sheet = Ref(new SpriteSheet());
    sheet->m_texture = std::move(texture);
    sheet->m_loaded = true;
    return sheet;
}

void SpriteSheet::addSprite(const std::string& name, int x, int y, int w, int h) {
    if (name.empty()) {
        throw std::invalid_argument("SpriteSheet::addSprite: name must not be empty");
    }
    if (m_nameIndex.count(name)) {
        throw std::invalid_argument("SpriteSheet::addSprite: duplicate name '" + name + "'");
    }
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("SpriteSheet::addSprite: width and height must be > 0");
    }
    if (x < 0 || y < 0) {
        throw std::invalid_argument("SpriteSheet::addSprite: x and y must be >= 0");
    }

    int texW = static_cast<int>(m_texture->getWidth());
    int texH = static_cast<int>(m_texture->getHeight());

    if (x + w > texW || y + h > texH) {
        throw std::out_of_range("SpriteSheet::addSprite: region (" + std::to_string(x) + "," +
                                std::to_string(y) + "," + std::to_string(w) + "," +
                                std::to_string(h) + ") exceeds texture bounds (" +
                                std::to_string(texW) + "x" + std::to_string(texH) + ")");
    }

    float invW = (texW > 0) ? 1.0f / static_cast<float>(texW) : 0.0f;
    float invH = (texH > 0) ? 1.0f / static_cast<float>(texH) : 0.0f;

    UVRect rect;
    rect.u = static_cast<float>(x) * invW;
    rect.v = static_cast<float>(y) * invH;
    rect.width = static_cast<float>(w) * invW;
    rect.height = static_cast<float>(h) * invH;

    int index = static_cast<int>(m_rects.size());
    m_rects.push_back(rect);
    m_nameIndex[name] = index;
}

SpriteSheet::UVRect SpriteSheet::getUVRect(int index) const {
    if (index < 0 || index >= static_cast<int>(m_rects.size())) {
        throw std::out_of_range("SpriteSheet::getUVRect: index " + std::to_string(index) +
                                " out of range [0, " + std::to_string(m_rects.size()) + ")");
    }
    return m_rects[static_cast<size_t>(index)];
}

SpriteSheet::UVRect SpriteSheet::getUVRect(const std::string& name) const {
    auto it = m_nameIndex.find(name);
    if (it == m_nameIndex.end()) {
        throw std::out_of_range("SpriteSheet::getUVRect: name '" + name + "' not found");
    }
    return m_rects[static_cast<size_t>(it->second)];
}

}  // namespace vde
