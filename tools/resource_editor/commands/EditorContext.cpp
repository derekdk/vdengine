/**
 * @file EditorContext.cpp
 * @brief Implementation of EditorContext methods that depend on full subsystem headers.
 */

#include "EditorContext.h"

#include "../CanvasRegistry.h"
#include "../CommandSystem.h"

namespace vde::tools {

Canvas* EditorContext::getActiveCanvas() {
    if (!canvases || !commands) {
        return nullptr;
    }
    uint32_t activeId = commands->getActiveCanvasId();
    if (activeId == 0) {
        return nullptr;
    }
    return canvases->getById(activeId);
}

}  // namespace vde::tools
