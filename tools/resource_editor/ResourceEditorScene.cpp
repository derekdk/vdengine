#include "ResourceEditorScene.h"

#include <vde/Texture.h>
#include <vde/VulkanContext.h>

#include <iostream>

#include "FileOperations.h"
#include <imgui_impl_vulkan.h>

namespace vde::tools {

// =============================================================================
// Construction
// =============================================================================

ResourceEditorScene::ResourceEditorScene(ToolMode mode) : BaseToolScene(mode) {}

// =============================================================================
// Scene lifecycle
// =============================================================================

void ResourceEditorScene::onEnter() {
    // Wire up the editor context
    m_editorContext.canvases = &m_canvasRegistry;
    m_editorContext.commands = &m_commandSystem;
    m_editorContext.palette = &m_toolPalette;
    m_editorContext.game = getGame();

    // Initialize the command system
    m_commandSystem.initialize(m_editorContext);

    addConsoleMessage("Welcome to the VDE Resource Editor.");
    addConsoleMessage("Type 'help' in the console for available commands.");
}

void ResourceEditorScene::update(float deltaTime) {
    // Base handles ESC, F1, F11, mouse camera
    BaseToolScene::update(deltaTime);

    // Sync dirty canvases to the GPU each frame
    syncGpuTextures();
}

// =============================================================================
// Command execution
// =============================================================================

void ResourceEditorScene::executeCommand(const std::string& cmdLine) {
    m_commandSystem.execute(cmdLine);
}

// =============================================================================
// Tool identity
// =============================================================================

std::string ResourceEditorScene::getToolName() const {
    return "VDE Resource Editor";
}

std::string ResourceEditorScene::getToolDescription() const {
    return "2D pixel art editor with command console";
}

// =============================================================================
// ImGui panels
// =============================================================================

void ResourceEditorScene::drawDebugUI() {
    float dpiScale = 1.0f;
    if (getGame()) {
        dpiScale = getGame()->getDPIScale();
        if (dpiScale <= 0.0f) {
            dpiScale = 1.0f;
        }
    }

    m_editorPanels.drawMenuBar(m_commandSystem, m_canvasRegistry);
    m_editorPanels.drawCanvasTabs(m_canvasRegistry, m_commandSystem, dpiScale);
    m_editorPanels.drawToolPalette(m_toolPalette, m_commandSystem, dpiScale);
    m_editorPanels.drawColorPicker(m_toolPalette, m_commandSystem, dpiScale);
    m_editorPanels.drawAllCanvasViewports(m_canvasRegistry, m_toolPalette, m_commandSystem,
                                          dpiScale);

    Canvas* activeCanvas = m_canvasRegistry.getById(m_commandSystem.getActiveCanvasId());
    m_editorPanels.drawPropertiesPanel(activeCanvas, dpiScale);
    m_editorPanels.drawCommandConsole(m_commandSystem, dpiScale);
}

// =============================================================================
// GPU texture synchronisation
// =============================================================================

void ResourceEditorScene::syncGpuTextures() {
    if (!getGame() || !getGame()->getVulkanContext()) {
        return;
    }

    auto* vulkanCtx = getGame()->getVulkanContext();

    for (uint32_t id : m_canvasRegistry.getIds()) {
        Canvas* canvas = m_canvasRegistry.getById(id);
        if (!canvas || !canvas->document) {
            continue;
        }

        if (canvas->document->getGeneration() <= canvas->lastUploadedGeneration) {
            continue;
        }

        // Remove stale ImGui texture descriptor if present
        if (canvas->imguiTextureId != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(canvas->imguiTextureId);
            canvas->imguiTextureId = VK_NULL_HANDLE;
        }

        // Create a fresh Texture from the pixel data
        auto tex = std::make_shared<vde::Texture>();
        if (!tex->loadFromData(canvas->document->getPixelData(), canvas->document->getWidth(),
                               canvas->document->getHeight())) {
            std::cerr << "ResourceEditorScene: loadFromData failed for canvas '" << canvas->name
                      << "'\n";
            continue;
        }

        if (!tex->uploadToGPU(vulkanCtx)) {
            std::cerr << "ResourceEditorScene: uploadToGPU failed for canvas '" << canvas->name
                      << "'\n";
            continue;
        }

        canvas->gpuTexture = tex;
        canvas->imguiTextureId = ImGui_ImplVulkan_AddTexture(
            tex->getSampler(), tex->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        canvas->lastUploadedGeneration = canvas->document->getGeneration();
    }
}

// =============================================================================
// ImGui cleanup
// =============================================================================

void ResourceEditorScene::onBeforeImGuiShutdown() {
    cleanupImGuiTextures();
}

void ResourceEditorScene::cleanupImGuiTextures() {
    for (uint32_t id : m_canvasRegistry.getIds()) {
        Canvas* canvas = m_canvasRegistry.getById(id);
        if (canvas && canvas->imguiTextureId != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(canvas->imguiTextureId);
            canvas->imguiTextureId = VK_NULL_HANDLE;
        }
    }
}

}  // namespace vde::tools
