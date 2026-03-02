/**
 * @file EditorPanels.cpp
 * @brief Implementation of ImGui UI panels for the Resource Editor.
 */

#include "EditorPanels.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

#include "CommandSystem.h"

namespace vde {
namespace tools {

// =============================================================================
// Command Console
// =============================================================================

void EditorPanels::drawCommandConsole(CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 500 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * dpiScale, 350 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Command Console")) {
        // Output area
        ImVec2 consoleSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2);
        if (ImGui::BeginChild("ConsoleOutput", consoleSize, ImGuiChildFlags_Borders)) {
            const auto& log = cmd.getLog();
            for (const auto& entry : log) {
                // Color-code by success/failure
                if (entry.success) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                }

                // Show canvas prefix if present
                std::string prefix;
                if (!entry.canvasName.empty()) {
                    prefix = "[" + entry.canvasName + "] ";
                }

                ImGui::TextWrapped("[%s] %s%s", entry.timestamp.c_str(), prefix.c_str(),
                                   entry.commandLine.c_str());

                if (!entry.result.empty()) {
                    ImGui::TextWrapped("  => %s", entry.result.c_str());
                }

                ImGui::PopStyleColor();
            }

            if (m_scrollConsoleToBottom) {
                ImGui::SetScrollHereY(1.0f);
                m_scrollConsoleToBottom = false;
            }
        }
        ImGui::EndChild();

        // Input area
        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();

        bool reclaim = false;
        if (ImGui::InputText("##consoleinput", m_consoleInputBuffer, sizeof(m_consoleInputBuffer),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string input(m_consoleInputBuffer);
            if (!input.empty()) {
                cmd.execute(input);
                m_consoleInputBuffer[0] = '\0';
                m_scrollConsoleToBottom = true;
            }
            reclaim = true;
        }

        if (reclaim) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

// =============================================================================
// Tool Palette
// =============================================================================

void EditorPanels::drawToolPalette(ToolPalette& palette, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200 * dpiScale, 300 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tools")) {
        auto& state = palette.getStateMutable();

        // Tool radio buttons
        struct ToolEntry {
            EditorTool tool;
            const char* label;
        };
        static const ToolEntry tools[] = {
            {EditorTool::Brush, "Brush"},      {EditorTool::Eraser, "Eraser"},
            {EditorTool::ColorPicker, "Pick"}, {EditorTool::Fill, "Fill"},
            {EditorTool::Line, "Line"},        {EditorTool::Rect, "Rect"},
            {EditorTool::Circle, "Circle"},
        };

        int currentTool = static_cast<int>(state.activeTool);
        for (const auto& t : tools) {
            if (ImGui::RadioButton(t.label, currentTool == static_cast<int>(t.tool))) {
                cmd.execute("settool " + ToolPalette::toolToString(t.tool));
            }
        }

        ImGui::Separator();

        // Brush size
        int size = state.brushSize;
        if (ImGui::SliderInt("Size", &size, 0, 16)) {
            cmd.execute("setsize " + std::to_string(size));
        }

        // Fill toggle for shapes
        bool fill = state.fillShape;
        if (ImGui::Checkbox("Fill Shapes", &fill)) {
            state.fillShape = fill;
        }
    }
    ImGui::End();
}

// =============================================================================
// Color Picker
// =============================================================================

void EditorPanels::drawColorPicker(ToolPalette& palette, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(10 * dpiScale, 320 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200 * dpiScale, 170 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Color")) {
        auto& state = palette.getStateMutable();

        // Convert to float for ImGui
        float col[4] = {state.color.r / 255.0f, state.color.g / 255.0f, state.color.b / 255.0f,
                        state.color.a / 255.0f};

        if (ImGui::ColorEdit4("##color", col,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
                                  ImGuiColorEditFlags_AlphaPreview)) {
            RGBAColor newColor = {
                static_cast<uint8_t>(col[0] * 255), static_cast<uint8_t>(col[1] * 255),
                static_cast<uint8_t>(col[2] * 255), static_cast<uint8_t>(col[3] * 255)};
            cmd.execute("setcolor " + ToolPalette::colorToHex(newColor));
        }

        // Hex display
        ImGui::Text("Hex: %s", ToolPalette::colorToHex(state.color).c_str());
    }
    ImGui::End();
}

// =============================================================================
// Canvas Tabs
// =============================================================================

void EditorPanels::drawCanvasTabs(CanvasRegistry& canvases, CommandSystem& cmd, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(220 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900 * dpiScale, 40 * dpiScale), ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("##CanvasTabs", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        if (ImGui::BeginTabBar("CanvasTabs")) {
            auto ids = canvases.getIds();
            for (uint32_t id : ids) {
                Canvas* canvas = canvases.getById(id);
                if (!canvas)
                    continue;

                std::string label = "[" + std::to_string(canvas->id) + "] " + canvas->name;
                if (canvas->document && canvas->document->isDirty()) {
                    label += " *";
                }

                bool isActive = (id == cmd.getActiveCanvasId());
                if (isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                }

                if (ImGui::BeginTabItem(label.c_str())) {
                    if (!isActive) {
                        cmd.execute("select canvas " + canvas->name);
                    }
                    ImGui::EndTabItem();
                }

                if (isActive) {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// =============================================================================
// Canvas Viewport
// =============================================================================

void EditorPanels::drawCanvasViewport(Canvas& canvas, ToolPalette& palette, CommandSystem& cmd,
                                      bool isActive, float dpiScale) {
    std::string winName = canvas.name + " (" + std::to_string(canvas.document->getWidth()) + "x" +
                          std::to_string(canvas.document->getHeight()) + ")###viewport_" +
                          std::to_string(canvas.id);

    ImGui::SetNextWindowSize(ImVec2(400 * dpiScale, 400 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(winName.c_str())) {
        if (canvas.imguiTextureId != VK_NULL_HANDLE) {
            float zoom = canvas.zoomLevel;
            float texW = static_cast<float>(canvas.document->getWidth()) * zoom;
            float texH = static_cast<float>(canvas.document->getHeight()) * zoom;

            // Get cursor position for mouse coordinate calculation
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();

            ImGui::Image((ImTextureID)canvas.imguiTextureId, ImVec2(texW, texH));

            // Mouse interaction
            if (ImGui::IsItemHovered()) {
                // Zoom with mouse wheel
                float scroll = ImGui::GetIO().MouseWheel;
                if (scroll != 0.0f) {
                    canvas.zoomLevel =
                        std::max(1.0f, std::min(64.0f, canvas.zoomLevel + scroll * 2.0f));
                }

                // Convert mouse position to pixel coordinates
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                int pixelX = static_cast<int>((mousePos.x - cursorPos.x) / zoom);
                int pixelY = static_cast<int>((mousePos.y - cursorPos.y) / zoom);

                // Clamp to canvas bounds
                pixelX = std::max(
                    0, std::min(pixelX, static_cast<int>(canvas.document->getWidth()) - 1));
                pixelY = std::max(
                    0, std::min(pixelY, static_cast<int>(canvas.document->getHeight()) - 1));

                // Status text
                ImGui::Text("Pixel: (%d, %d) Zoom: %.0fx", pixelX, pixelY, canvas.zoomLevel);

                // Handle mouse events
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (!isActive) {
                        cmd.execute("select canvas " + canvas.name);
                    }
                    std::string action = palette.onCanvasMouseDown(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }

                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    std::string action = palette.onCanvasMouseDrag(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    std::string action = palette.onCanvasMouseUp(canvas.id, pixelX, pixelY);
                    if (!action.empty()) {
                        cmd.execute("@" + std::to_string(canvas.id) + " " + action);
                    }
                }
            }
        } else {
            ImGui::Text("Loading texture...");
        }
    }
    ImGui::End();
}

// =============================================================================
// All Canvas Viewports
// =============================================================================

void EditorPanels::drawAllCanvasViewports(CanvasRegistry& canvases, ToolPalette& palette,
                                          CommandSystem& cmd, float dpiScale) {
    auto ids = canvases.getIds();
    for (uint32_t id : ids) {
        Canvas* canvas = canvases.getById(id);
        if (!canvas || !canvas->document)
            continue;

        bool isActive = (id == cmd.getActiveCanvasId());
        drawCanvasViewport(*canvas, palette, cmd, isActive, dpiScale);
    }
}

// =============================================================================
// Properties Panel
// =============================================================================

void EditorPanels::drawPropertiesPanel(Canvas* activeCanvas, float dpiScale) {
    ImGui::SetNextWindowPos(ImVec2(1130 * dpiScale, 10 * dpiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250 * dpiScale, 300 * dpiScale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Properties")) {
        if (activeCanvas && activeCanvas->document) {
            auto* doc = activeCanvas->document.get();

            ImGui::Text("Canvas ID: %u", activeCanvas->id);
            ImGui::Text("Name: %s", activeCanvas->name.c_str());
            ImGui::Text("Size: %u x %u", doc->getWidth(), doc->getHeight());
            ImGui::Text("File: %s",
                        doc->getFilePath().empty() ? "(unsaved)" : doc->getFilePath().c_str());
            ImGui::Text("Dirty: %s", doc->isDirty() ? "Yes" : "No");
            ImGui::Text("Generation: %llu", static_cast<unsigned long long>(doc->getGeneration()));
            ImGui::Text("Undo: %zu / Redo: %zu", doc->getUndoCount(), doc->getRedoCount());
            ImGui::Text("Zoom: %.0fx", activeCanvas->zoomLevel);
        } else {
            ImGui::TextDisabled("No active canvas");
        }
    }
    ImGui::End();
}

// =============================================================================
// Menu Bar
// =============================================================================

void EditorPanels::drawMenuBar(CommandSystem& cmd, CanvasRegistry& canvases) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New...", "Ctrl+N")) {
                m_showNewCanvasPopup = true;
                m_newCanvasWidth = 32;
                m_newCanvasHeight = 32;
                std::snprintf(m_newCanvasName, sizeof(m_newCanvasName), "%s",
                              canvases.generateUniqueName().c_str());
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                cmd.execute("load");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                cmd.execute("save");
            }
            if (ImGui::MenuItem("Save As...")) {
                cmd.execute("saveas");
            }
            if (ImGui::MenuItem("Export...")) {
                cmd.execute("export png");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "ESC")) {
                cmd.execute("exit");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                cmd.execute("undo");
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                cmd.execute("redo");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Zoom In", "+")) {
                cmd.execute("zoom in");
            }
            if (ImGui::MenuItem("Zoom Out", "-")) {
                cmd.execute("zoom out");
            }
            if (ImGui::MenuItem("Reset Zoom")) {
                cmd.execute("zoom 1");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Commands...")) {
                cmd.execute("help");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // New canvas popup
    if (m_showNewCanvasPopup) {
        ImGui::OpenPopup("New Canvas");
        m_showNewCanvasPopup = false;
    }

    if (ImGui::BeginPopupModal("New Canvas", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_newCanvasName, sizeof(m_newCanvasName));
        ImGui::InputInt("Width", &m_newCanvasWidth);
        ImGui::InputInt("Height", &m_newCanvasHeight);

        m_newCanvasWidth = std::max(1, std::min(4096, m_newCanvasWidth));
        m_newCanvasHeight = std::max(1, std::min(4096, m_newCanvasHeight));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string name(m_newCanvasName);
            if (name.empty()) {
                name = canvases.generateUniqueName();
            }
            cmd.execute("create canvas " + name + " " + std::to_string(m_newCanvasWidth) + " " +
                        std::to_string(m_newCanvasHeight));
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}  // namespace tools
}  // namespace vde
