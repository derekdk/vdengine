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

    auto sheet = Ref(new SpriteSheet());
    sheet->m_texture = std::move(texture);

    int texW = static_cast<int>(sheet->m_texture->getWidth());
    int texH = static_cast<int>(sheet->m_texture->getHeight());

    // Compute cell size in pixels (account for spacing between cells)
    int totalSpacingX = spacingPx * (columns - 1);
    int totalSpacingY = spacingPx * (rows - 1);
    int cellW = (texW - totalSpacingX) / columns;
    int cellH = (texH - totalSpacingY) / rows;

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

    return sheet;
}

SpriteSheet::Ref SpriteSheet::create(std::shared_ptr<Texture> texture) {
    if (!texture) {
        throw std::invalid_argument("SpriteSheet::create: texture must not be null");
    }

    auto sheet = Ref(new SpriteSheet());
    sheet->m_texture = std::move(texture);
    return sheet;
}

void SpriteSheet::addSprite(const std::string& name, int x, int y, int w, int h) {
    if (name.empty()) {
        throw std::invalid_argument("SpriteSheet::addSprite: name must not be empty");
    }
    if (m_nameIndex.count(name)) {
        throw std::invalid_argument("SpriteSheet::addSprite: duplicate name '" + name + "'");
    }

    int texW = static_cast<int>(m_texture->getWidth());
    int texH = static_cast<int>(m_texture->getHeight());

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
