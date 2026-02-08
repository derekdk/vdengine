#include <vde/api/CameraBounds.h>

#include <algorithm>

namespace vde {

CameraBounds2D::CameraBounds2D() {
    updateMapping();
}

void CameraBounds2D::setScreenSize(Pixels width, Pixels height) {
    m_screenSize = ScreenSize(width, height);
    updateMapping();
    applyConstraints();
}

void CameraBounds2D::setScreenSize(const ScreenSize& size) {
    m_screenSize = size;
    updateMapping();
    applyConstraints();
}

void CameraBounds2D::setWorldWidth(Meters width) {
    m_baseWorldWidth = width;
    updateMapping();
    applyConstraints();
}

void CameraBounds2D::setWorldHeight(Meters height) {
    // Derive width from height and aspect ratio
    float aspect = m_screenSize.aspectRatio();
    m_baseWorldWidth = Meters(height.value * aspect);
    updateMapping();
    applyConstraints();
}

void CameraBounds2D::setZoom(float zoom) {
    m_zoom = std::max(0.001f, zoom);  // Prevent zero/negative zoom
    updateMapping();
    applyConstraints();
}

void CameraBounds2D::centerOn(Meters worldX, Meters worldY) {
    m_centerX = worldX;
    m_centerY = worldY;
    applyConstraints();
}

void CameraBounds2D::centerOn(const glm::vec2& worldPos) {
    centerOn(Meters(worldPos.x), Meters(worldPos.y));
}

void CameraBounds2D::move(Meters deltaX, Meters deltaY) {
    m_centerX += deltaX;
    m_centerY += deltaY;
    applyConstraints();
}

void CameraBounds2D::setConstraintBounds(const WorldBounds2D& bounds) {
    m_constraints = bounds;
    m_hasConstraints = true;
    applyConstraints();
}

void CameraBounds2D::clearConstraintBounds() {
    m_hasConstraints = false;
}

WorldBounds2D CameraBounds2D::getVisibleBounds() const {
    Meters halfW = getVisibleWidth() * 0.5f;
    Meters halfH = getVisibleHeight() * 0.5f;
    return WorldBounds2D(m_centerX - halfW, m_centerY - halfH, m_centerX + halfW,
                         m_centerY + halfH);
}

Meters CameraBounds2D::getVisibleWidth() const {
    return m_baseWorldWidth / m_zoom;
}

Meters CameraBounds2D::getVisibleHeight() const {
    float aspect = m_screenSize.aspectRatio();
    return Meters(m_baseWorldWidth.value / (m_zoom * aspect));
}

glm::vec2 CameraBounds2D::screenToWorld(Pixels screenX, Pixels screenY) const {
    // Screen origin is top-left, Y increases downward
    // World Y typically increases upward, so we flip Y

    // Get visible bounds
    WorldBounds2D visible = getVisibleBounds();

    // Normalize screen position to 0-1
    float normalizedX = screenX.value / m_screenSize.width.value;
    float normalizedY = screenY.value / m_screenSize.height.value;

    // Map to world coordinates
    // X: left to right maps to minX to maxX
    // Y: top to bottom maps to maxY to minY (flipped)
    float worldX = visible.minX.value + normalizedX * visible.width().value;
    float worldY = visible.maxY.value - normalizedY * visible.height().value;

    return glm::vec2(worldX, worldY);
}

glm::vec2 CameraBounds2D::screenToWorld(const glm::vec2& screenPos) const {
    return screenToWorld(Pixels(screenPos.x), Pixels(screenPos.y));
}

glm::vec2 CameraBounds2D::worldToScreen(Meters worldX, Meters worldY) const {
    WorldBounds2D visible = getVisibleBounds();

    // Normalize world position relative to visible bounds
    float normalizedX = (worldX.value - visible.minX.value) / visible.width().value;
    float normalizedY = (visible.maxY.value - worldY.value) / visible.height().value;  // Flip Y

    // Map to screen coordinates
    float screenX = normalizedX * m_screenSize.width.value;
    float screenY = normalizedY * m_screenSize.height.value;

    return glm::vec2(screenX, screenY);
}

glm::vec2 CameraBounds2D::worldToScreen(const glm::vec2& worldPos) const {
    return worldToScreen(Meters(worldPos.x), Meters(worldPos.y));
}

bool CameraBounds2D::isVisible(Meters worldX, Meters worldY) const {
    WorldBounds2D visible = getVisibleBounds();
    return visible.contains(worldX, worldY);
}

bool CameraBounds2D::isVisible(const glm::vec2& worldPos) const {
    return isVisible(Meters(worldPos.x), Meters(worldPos.y));
}

bool CameraBounds2D::isVisible(const WorldBounds2D& bounds) const {
    WorldBounds2D visible = getVisibleBounds();

    // Check for intersection
    return bounds.maxX >= visible.minX && bounds.minX <= visible.maxX &&
           bounds.maxY >= visible.minY && bounds.minY <= visible.maxY;
}

void CameraBounds2D::updateMapping() {
    Meters visibleWidth = getVisibleWidth();
    m_mapping = PixelToWorldMapping::fitWidth(visibleWidth, m_screenSize.width);
}

void CameraBounds2D::applyConstraints() {
    if (!m_hasConstraints) {
        return;
    }

    Meters halfW = getVisibleWidth() * 0.5f;
    Meters halfH = getVisibleHeight() * 0.5f;

    // If visible area is smaller than constraints, clamp center
    if (getVisibleWidth() <= m_constraints.width()) {
        // Clamp X so visible area stays within constraints
        if (m_centerX - halfW < m_constraints.minX) {
            m_centerX = m_constraints.minX + halfW;
        }
        if (m_centerX + halfW > m_constraints.maxX) {
            m_centerX = m_constraints.maxX - halfW;
        }
    } else {
        // Visible area larger than constraints, center on constraints
        m_centerX = (m_constraints.minX + m_constraints.maxX) * 0.5f;
    }

    if (getVisibleHeight() <= m_constraints.height()) {
        // Clamp Y so visible area stays within constraints
        if (m_centerY - halfH < m_constraints.minY) {
            m_centerY = m_constraints.minY + halfH;
        }
        if (m_centerY + halfH > m_constraints.maxY) {
            m_centerY = m_constraints.maxY - halfH;
        }
    } else {
        // Visible area larger than constraints, center on constraints
        m_centerY = (m_constraints.minY + m_constraints.maxY) * 0.5f;
    }
}

}  // namespace vde
