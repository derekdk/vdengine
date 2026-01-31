/**
 * @file LightBox.cpp
 * @brief Implementation of lighting classes
 */

#include <vde/api/LightBox.h>
#include <stdexcept>

namespace vde {

// ============================================================================
// LightBox Implementation
// ============================================================================

size_t LightBox::addLight(const Light& light) {
    m_lights.push_back(light);
    return m_lights.size() - 1;
}

void LightBox::removeLight(size_t index) {
    if (index >= m_lights.size()) {
        throw std::out_of_range("Light index out of range");
    }
    m_lights.erase(m_lights.begin() + static_cast<ptrdiff_t>(index));
}

// ============================================================================
// ThreePointLightBox Implementation
// ============================================================================

ThreePointLightBox::ThreePointLightBox(const Color& keyColor, float keyIntensity) {
    // Set ambient to low intensity
    m_ambientColor = Color(0.1f, 0.1f, 0.1f);
    m_ambientIntensity = 0.3f;
    
    // Key light - main light from upper front-right
    // This is the primary light source
    Light key = Light::directional(
        Direction(-0.5f, -1.0f, -0.5f).normalized(),
        keyColor,
        keyIntensity
    );
    addLight(key);
    
    // Fill light - softer light from front-left
    // Fills in shadows created by the key light
    Light fill = Light::directional(
        Direction(0.5f, -0.5f, -0.5f).normalized(),
        keyColor,
        keyIntensity * 0.4f  // Fill is typically 40-50% of key
    );
    addLight(fill);
    
    // Back light (rim light) - from behind
    // Creates separation from background
    Light back = Light::directional(
        Direction(0.0f, -0.3f, 1.0f).normalized(),
        keyColor,
        keyIntensity * 0.6f  // Back is typically 60-80% of key
    );
    addLight(back);
}

} // namespace vde
