/**
 * @file SpriteEditorScene.cpp
 * @brief Implementation of SpriteEditorScene — command handlers, UI, and
 *        viewport rendering for the Sprite Editor tool.
 */

#include "SpriteEditorScene.h"

#include <vde/ImageLoader.h>
#include <vde/Texture.h>
#include <vde/api/Entity.h>
#include <vde/api/GameCamera.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace vde {
namespace tools {

// ── Construction / lifecycle ────────────────────────────────────

SpriteEditorScene::SpriteEditorScene(ToolMode mode) : BaseToolScene(mode) {}

void SpriteEditorScene::onEnter() {
    // Set up a 2D camera covering a reasonable default viewport.
    m_camera2D = new Camera2D(20.0f, 15.0f);
    setCamera(m_camera2D);

    setBackgroundColor(Color(0.12f, 0.12f, 0.12f));

    addConsoleMessage("=== VDE Sprite Editor ===");
    addConsoleMessage("Type 'help' for available commands.");

    std::cerr << "[sprite_editor] onEnter: m_execFile='" << m_execFile << "'\n";
    addConsoleMessage("[debug] execFile: '" + m_execFile + "'");

    if (!m_execFile.empty()) {
        processScriptFile(m_execFile);
    }
}

void SpriteEditorScene::update(float deltaTime) {
    // Let base handle ESC, fullscreen, debug-ui toggle.
    // NOTE: Do NOT call BaseToolScene::update for orbit camera —
    // we override camera handling entirely for 2D.
    Scene::update(deltaTime);

    auto* input = dynamic_cast<SpriteEditorInputHandler*>(getInputHandler());
    if (!input)
        return;

    // ESC / fullscreen / UI toggle
    if (input->isEscapePressed()) {
        if (getGame())
            getGame()->quit();
    }
    if (input->isFullscreenTogglePressed()) {
        if (getGame() && getGame()->getWindow()) {
            auto* window = getGame()->getWindow();
            window->setFullscreen(!window->isFullscreen());
        }
    }
    if (input->isDebugUITogglePressed()) {
        m_debugUIVisible = !m_debugUIVisible;
    }

    // Camera zoom / pan (only if ImGui doesn't want the mouse)
    if (m_toolMode == ToolMode::INTERACTIVE && m_camera2D) {
        bool imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;

        if (!imguiWantsMouse) {
            // Scroll = zoom
            float scroll = input->consumeScrollDelta();
            if (scroll != 0.0f) {
                float zoom = m_camera2D->getZoom();
                zoom *= (scroll > 0.0f) ? 1.1f : 0.9f;
                zoom = std::clamp(zoom, 0.1f, 50.0f);
                m_camera2D->setZoom(zoom);
            }

            // Middle-mouse drag = pan
            if (input->isMiddleDown()) {
                double dx, dy;
                input->getPanDelta(dx, dy);
                if (dx != 0.0 || dy != 0.0) {
                    float zoom = m_camera2D->getZoom();
                    float panScale = 0.02f / std::max(zoom, 0.01f);
                    m_camera2D->move(static_cast<float>(-dx) * panScale,
                                     static_cast<float>(dy) * panScale);
                }
            }
        }
    }

    // Animation preview playback
    if (m_animPlaying && m_previewSprite && !m_playingAnimation.empty()) {
        const auto* anim = m_doc.findAnimation(m_playingAnimation);
        if (anim && !anim->frames.empty()) {
            m_animElapsed += deltaTime * m_animSpeed;
            const auto& frameName = anim->getFrameAt(m_animElapsed);
            const auto* region = m_doc.findSprite(frameName);
            if (region && m_doc.hasImage()) {
                float u = static_cast<float>(region->x) / m_doc.getImageWidth();
                float v = static_cast<float>(region->y) / m_doc.getImageHeight();
                float uw = static_cast<float>(region->w) / m_doc.getImageWidth();
                float vh = static_cast<float>(region->h) / m_doc.getImageHeight();
                m_previewSprite->setUVRect(u, v, uw, vh);
            }

            // Stop non-looping animation when done
            if (!anim->looping && m_animElapsed >= anim->getTotalDuration()) {
                m_animPlaying = false;
            }
        }
    }
}

// ── Command dispatch ────────────────────────────────────────────

void SpriteEditorScene::executeCommand(const std::string& cmdLine) {
    std::istringstream iss(cmdLine);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty())
        return;

    // Lowercase the command
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "clear") {
        cmdClear();
    } else if (cmd == "info") {
        cmdInfo();
    } else if (cmd == "load") {
        cmdLoad(iss);
    } else if (cmd == "save") {
        cmdSave(iss);
    } else if (cmd == "open") {
        cmdOpen(iss);
    } else if (cmd == "grid") {
        cmdGrid(iss);
    } else if (cmd == "add") {
        cmdAdd(iss);
    } else if (cmd == "remove") {
        cmdRemove(iss);
    } else if (cmd == "rename") {
        cmdRename(iss);
    } else if (cmd == "anchor") {
        cmdAnchor(iss);
    } else if (cmd == "list") {
        cmdList();
    } else if (cmd == "select") {
        cmdSelect(iss);
    } else if (cmd == "anim") {
        cmdAnim(iss);
    } else if (cmd == "zoom") {
        cmdZoom(iss);
    } else {
        addConsoleMessage("ERROR: Unknown command '" + cmd + "'. Type 'help' for commands.");
    }
}

// ── Command implementations ─────────────────────────────────────

void SpriteEditorScene::cmdHelp() {
    addConsoleMessage("COMMANDS:");
    addConsoleMessage("  File:");
    addConsoleMessage("    load <path>              Load a source image (PNG/JPG)");
    addConsoleMessage("    save <path.vdesheet>     Export spritesheet metadata");
    addConsoleMessage("    open <path.vdesheet>     Open an existing spritesheet");
    addConsoleMessage("  Sprites:");
    addConsoleMessage("    grid <w> <h> [sx sy] [ox oy]  Auto-slice into grid");
    addConsoleMessage("    add <name> <x> <y> <w> <h>    Add a sprite region");
    addConsoleMessage("    remove <name>                  Remove a sprite region");
    addConsoleMessage("    rename <old> <new>             Rename a sprite region");
    addConsoleMessage("    anchor <name> <x> <y>          Set anchor (0-1 range)");
    addConsoleMessage("    list                           List all sprites");
    addConsoleMessage("    select <name>                  Select a sprite");
    addConsoleMessage("  Animations:");
    addConsoleMessage("    anim create <name> [loop]      Create animation");
    addConsoleMessage("    anim delete <name>             Delete animation");
    addConsoleMessage("    anim addframe <anim> <sprite> [dur]  Add frame");
    addConsoleMessage("    anim removeframe <anim> <idx>       Remove frame");
    addConsoleMessage("    anim setduration <anim> <idx> <sec> Set duration");
    addConsoleMessage("    anim list                      List all animations");
    addConsoleMessage("    anim play <name>               Play animation");
    addConsoleMessage("    anim stop                      Stop playback");
    addConsoleMessage("  View:");
    addConsoleMessage("    zoom <level>                   Set zoom (1.0=fit)");
    addConsoleMessage("  Utility:");
    addConsoleMessage("    help                           Show this help");
    addConsoleMessage("    clear                          Clear console");
    addConsoleMessage("    info                           Document summary");
}

void SpriteEditorScene::cmdClear() {
    m_consoleLog.clear();
}

void SpriteEditorScene::cmdInfo() {
    if (!m_doc.hasImage()) {
        addConsoleMessage("No image loaded.  Use 'load <path>' first.");
        return;
    }
    addConsoleMessage("Image: " + m_doc.getSourceImagePath());
    addConsoleMessage("  Size: " + std::to_string(m_doc.getImageWidth()) + " x " +
                      std::to_string(m_doc.getImageHeight()));
    addConsoleMessage("  Sprites: " + std::to_string(m_doc.getSpriteCount()));
    addConsoleMessage("  Animations: " + std::to_string(m_doc.getAnimationCount()));
}

void SpriteEditorScene::cmdLoad(std::istringstream& iss) {
    std::string path;
    iss >> path;
    if (path.empty()) {
        addConsoleMessage("ERROR: Usage: load <path>");
        return;
    }

    auto imageData = ImageLoader::load(path, 4);
    if (!imageData.isValid()) {
        addConsoleMessage("ERROR: Failed to load image: " + path);
        return;
    }

    int imgWidth = imageData.width;
    int imgHeight = imageData.height;

    // Create texture
    auto texture = std::make_shared<Texture>();
    texture->loadFromData(imageData.pixels, static_cast<uint32_t>(imgWidth),
                          static_cast<uint32_t>(imgHeight));
    auto* ctx = getGame() ? getGame()->getVulkanContext() : nullptr;
    if (ctx) {
        texture->uploadToGPU(ctx);
    }
    ImageLoader::free(imageData);

    m_sourceTexture = texture;
    m_doc.setSourceImage(path, imgWidth, imgHeight);

    // Create source image sprite
    if (!m_sourceSprite) {
        auto spriteRef = addEntity<SpriteEntity>();
        m_sourceSprite = spriteRef.get();
    }
    m_sourceSprite->setTexture(m_sourceTexture);

    // Scale sprite to pixel dimensions in world units
    float scaleX = static_cast<float>(m_doc.getImageWidth()) / 64.0f;
    float scaleY = static_cast<float>(m_doc.getImageHeight()) / 64.0f;
    m_sourceSprite->setScale(Scale(scaleX, scaleY, 1.0f));

    fitImageToView();

    addConsoleMessage("Loaded " + std::to_string(m_doc.getImageWidth()) + "x" +
                      std::to_string(m_doc.getImageHeight()) + " image: " + path);
}

void SpriteEditorScene::cmdSave(std::istringstream& iss) {
    std::string path;
    iss >> path;
    if (path.empty()) {
        addConsoleMessage("ERROR: Usage: save <path.vdesheet>");
        return;
    }

    if (m_doc.saveToFile(path)) {
        addConsoleMessage("Saved spritesheet to: " + path);
    } else {
        addConsoleMessage("ERROR: Failed to save to: " + path);
    }
}

void SpriteEditorScene::cmdOpen(std::istringstream& iss) {
    std::string path;
    iss >> path;
    if (path.empty()) {
        addConsoleMessage("ERROR: Usage: open <path.vdesheet>");
        return;
    }

    if (!m_doc.loadFromFile(path)) {
        addConsoleMessage("ERROR: Failed to open: " + path);
        return;
    }

    addConsoleMessage("Opened spritesheet: " + path);
    addConsoleMessage("  Image: " + m_doc.getSourceImagePath());
    addConsoleMessage("  Sprites: " + std::to_string(m_doc.getSpriteCount()));
    addConsoleMessage("  Animations: " + std::to_string(m_doc.getAnimationCount()));

    // Try to load the referenced source image
    std::istringstream imgIss(m_doc.getSourceImagePath());
    cmdLoad(imgIss);
}

void SpriteEditorScene::cmdGrid(std::istringstream& iss) {
    int cellW = 0, cellH = 0;
    int spacingX = 0, spacingY = 0;
    int offsetX = 0, offsetY = 0;
    iss >> cellW >> cellH >> spacingX >> spacingY >> offsetX >> offsetY;

    if (cellW <= 0 || cellH <= 0) {
        addConsoleMessage(
            "ERROR: Usage: grid <cellW> <cellH> [spacingX spacingY] [offsetX offsetY]");
        return;
    }
    if (!m_doc.hasImage()) {
        addConsoleMessage("ERROR: Load an image first with 'load <path>'.");
        return;
    }

    int count = m_doc.gridSlice(cellW, cellH, spacingX, spacingY, offsetX, offsetY);

    int cols = 0;
    for (int x = offsetX; x + cellW <= m_doc.getImageWidth(); x += cellW + spacingX)
        ++cols;
    int rows = count > 0 ? count / cols : 0;

    addConsoleMessage("Created " + std::to_string(count) + " sprite regions (" +
                      std::to_string(cols) + " cols x " + std::to_string(rows) + " rows)");
}

void SpriteEditorScene::cmdAdd(std::istringstream& iss) {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;
    iss >> name >> x >> y >> w >> h;

    if (name.empty() || w <= 0 || h <= 0) {
        addConsoleMessage("ERROR: Usage: add <name> <x> <y> <w> <h>");
        return;
    }

    SpriteRegion region;
    region.name = name;
    region.x = x;
    region.y = y;
    region.w = w;
    region.h = h;

    if (m_doc.addSprite(region)) {
        addConsoleMessage("Added sprite '" + name + "' [" + std::to_string(x) + "," +
                          std::to_string(y) + " " + std::to_string(w) + "x" + std::to_string(h) +
                          "]");
    } else {
        addConsoleMessage("ERROR: Sprite '" + name + "' already exists.");
    }
}

void SpriteEditorScene::cmdRemove(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: remove <name>");
        return;
    }

    if (m_doc.removeSprite(name)) {
        if (m_selectedSprite == name)
            m_selectedSprite.clear();
        addConsoleMessage("Removed sprite '" + name + "'.");
    } else {
        addConsoleMessage("ERROR: Sprite '" + name + "' not found.");
    }
}

void SpriteEditorScene::cmdRename(std::istringstream& iss) {
    std::string oldName, newName;
    iss >> oldName >> newName;
    if (oldName.empty() || newName.empty()) {
        addConsoleMessage("ERROR: Usage: rename <old> <new>");
        return;
    }

    if (m_doc.renameSprite(oldName, newName)) {
        if (m_selectedSprite == oldName)
            m_selectedSprite = newName;
        addConsoleMessage("Renamed '" + oldName + "' -> '" + newName + "'.");
    } else {
        addConsoleMessage("ERROR: Rename failed (source not found or target already exists).");
    }
}

void SpriteEditorScene::cmdAnchor(std::istringstream& iss) {
    std::string name;
    float ax = 0.5f, ay = 0.5f;
    iss >> name >> ax >> ay;
    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: anchor <name> <x> <y>");
        return;
    }

    if (m_doc.setAnchor(name, ax, ay)) {
        addConsoleMessage("Set anchor of '" + name + "' to (" + std::to_string(ax) + ", " +
                          std::to_string(ay) + ").");
    } else {
        addConsoleMessage("ERROR: Sprite '" + name + "' not found.");
    }
}

void SpriteEditorScene::cmdList() {
    const auto& sprites = m_doc.getSprites();
    if (sprites.empty()) {
        addConsoleMessage("No sprites defined.");
        return;
    }
    addConsoleMessage("Sprites (" + std::to_string(sprites.size()) + "):");
    for (const auto& s : sprites) {
        addConsoleMessage("  " + s.name + "  [" + std::to_string(s.x) + "," + std::to_string(s.y) +
                          " " + std::to_string(s.w) + "x" + std::to_string(s.h) + "]");
    }
}

void SpriteEditorScene::cmdSelect(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        m_selectedSprite.clear();
        addConsoleMessage("Selection cleared.");
        return;
    }

    if (m_doc.findSprite(name)) {
        m_selectedSprite = name;
        addConsoleMessage("Selected: " + name);
    } else {
        addConsoleMessage("ERROR: Sprite '" + name + "' not found.");
    }
}

void SpriteEditorScene::cmdAnim(std::istringstream& iss) {
    std::string subcmd;
    iss >> subcmd;
    if (subcmd.empty()) {
        addConsoleMessage(
            "ERROR: Usage: anim <create|delete|addframe|removeframe|setduration|list|play|stop>");
        return;
    }

    std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (subcmd == "create") {
        std::string name;
        std::string loopStr;
        iss >> name >> loopStr;
        if (name.empty()) {
            addConsoleMessage("ERROR: Usage: anim create <name> [loop|noloop]");
            return;
        }
        bool looping = (loopStr != "noloop");
        if (m_doc.createAnimation(name, looping)) {
            addConsoleMessage("Created animation '" + name + "'" +
                              (looping ? " (looping)" : " (one-shot)"));
        } else {
            addConsoleMessage("ERROR: Animation '" + name + "' already exists.");
        }
    } else if (subcmd == "delete") {
        std::string name;
        iss >> name;
        if (name.empty()) {
            addConsoleMessage("ERROR: Usage: anim delete <name>");
            return;
        }
        if (m_doc.deleteAnimation(name)) {
            if (m_playingAnimation == name) {
                m_animPlaying = false;
                m_playingAnimation.clear();
            }
            addConsoleMessage("Deleted animation '" + name + "'.");
        } else {
            addConsoleMessage("ERROR: Animation '" + name + "' not found.");
        }
    } else if (subcmd == "addframe") {
        std::string animName, spriteName;
        float duration = 0.1f;
        iss >> animName >> spriteName >> duration;
        if (animName.empty() || spriteName.empty()) {
            addConsoleMessage("ERROR: Usage: anim addframe <anim> <sprite> [duration]");
            return;
        }
        if (m_doc.addFrame(animName, spriteName, duration)) {
            addConsoleMessage("Added frame '" + spriteName + "' (dur=" + std::to_string(duration) +
                              "s) to '" + animName + "'.");
        } else {
            addConsoleMessage("ERROR: Animation or sprite not found.");
        }
    } else if (subcmd == "removeframe") {
        std::string animName;
        int index = -1;
        iss >> animName >> index;
        if (animName.empty() || index < 0) {
            addConsoleMessage("ERROR: Usage: anim removeframe <anim> <index>");
            return;
        }
        if (m_doc.removeFrame(animName, index)) {
            addConsoleMessage("Removed frame " + std::to_string(index) + " from '" + animName +
                              "'.");
        } else {
            addConsoleMessage("ERROR: Animation not found or index out of range.");
        }
    } else if (subcmd == "setduration") {
        std::string animName;
        int index = -1;
        float duration = 0.1f;
        iss >> animName >> index >> duration;
        if (animName.empty() || index < 0) {
            addConsoleMessage("ERROR: Usage: anim setduration <anim> <index> <seconds>");
            return;
        }
        if (m_doc.setFrameDuration(animName, index, duration)) {
            addConsoleMessage("Set frame " + std::to_string(index) + " duration to " +
                              std::to_string(duration) + "s in '" + animName + "'.");
        } else {
            addConsoleMessage("ERROR: Animation not found or index out of range.");
        }
    } else if (subcmd == "list") {
        const auto& anims = m_doc.getAnimations();
        if (anims.empty()) {
            addConsoleMessage("No animations defined.");
            return;
        }
        addConsoleMessage("Animations (" + std::to_string(anims.size()) + "):");
        for (const auto& a : anims) {
            addConsoleMessage("  " + a.name + " [" + std::to_string(a.frames.size()) + " frames, " +
                              std::to_string(a.getTotalDuration()) + "s" +
                              (a.looping ? ", loop" : "") + "]");
        }
    } else if (subcmd == "play") {
        std::string name;
        iss >> name;
        if (name.empty()) {
            addConsoleMessage("ERROR: Usage: anim play <name>");
            return;
        }
        if (!m_doc.findAnimation(name)) {
            addConsoleMessage("ERROR: Animation '" + name + "' not found.");
            return;
        }

        m_playingAnimation = name;
        m_animElapsed = 0.0f;
        m_animPlaying = true;

        // Set up preview sprite if needed
        if (!m_previewSprite && m_sourceTexture) {
            auto previewRef = addEntity<SpriteEntity>();
            m_previewSprite = previewRef.get();
            m_previewSprite->setTexture(m_sourceTexture);
            // Place preview to the right of the source image
            float offsetX = static_cast<float>(m_doc.getImageWidth()) / 64.0f + 3.0f;
            m_previewSprite->setPosition(Position(offsetX, 0.0f, 0.0f));
            m_previewSprite->setScale(Scale(2.0f, 2.0f, 1.0f));
        }

        addConsoleMessage("Playing animation: " + name);
    } else if (subcmd == "stop") {
        m_animPlaying = false;
        m_playingAnimation.clear();
        addConsoleMessage("Animation stopped.");
    } else {
        addConsoleMessage("ERROR: Unknown anim subcommand '" + subcmd + "'.");
    }
}

void SpriteEditorScene::cmdZoom(std::istringstream& iss) {
    float level = 1.0f;
    iss >> level;
    if (level <= 0.0f) {
        addConsoleMessage("ERROR: Zoom level must be positive.");
        return;
    }
    if (m_camera2D) {
        m_camera2D->setZoom(level);
        addConsoleMessage("Zoom set to " + std::to_string(level));
    }
}

// ── Camera helpers ──────────────────────────────────────────────

void SpriteEditorScene::fitImageToView() {
    if (!m_camera2D || !m_doc.hasImage())
        return;

    m_camera2D->setPosition(0.0f, 0.0f);
    m_camera2D->setZoom(1.0f);
}

// ── ImGui UI ────────────────────────────────────────────────────

void SpriteEditorScene::drawDebugUI() {
    drawMenuBar();
    drawConsole();
    drawSpriteList();
    drawSpriteProperties();
    drawAnimationEditor();
}

void SpriteEditorScene::drawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                m_doc = SpriteDocument();
                m_selectedSprite.clear();
                m_animPlaying = false;
                m_playingAnimation.clear();
                m_sourceSprite = nullptr;
                m_previewSprite = nullptr;
                m_sourceTexture.reset();
                addConsoleMessage("New document created.");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Help")) {
                cmdHelp();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Fit Image")) {
                fitImageToView();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void SpriteEditorScene::drawConsole() {
    float scale = getGame()->getDPIScale();

    ImGui::SetNextWindowPos(ImVec2(10 * scale, 500 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700 * scale, 250 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Console")) {
        // Output area
        ImVec2 consoleSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2);
        if (ImGui::BeginChild("Output", consoleSize, true)) {
            for (const auto& msg : getConsoleLog()) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            if (shouldScrollToBottom()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        // Input area
        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();

        if (ImGui::InputText("##input", m_inputBuffer, sizeof(m_inputBuffer),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string cmd(m_inputBuffer);
            if (!cmd.empty()) {
                addConsoleMessage("> " + cmd);
                executeCommand(cmd);
                m_inputBuffer[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

void SpriteEditorScene::drawSpriteList() {
    float scale = getGame()->getDPIScale();

    ImGui::SetNextWindowPos(ImVec2(720 * scale, 30 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350 * scale, 250 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Sprite List")) {
        const auto& sprites = m_doc.getSprites();
        for (const auto& s : sprites) {
            bool selected = (s.name == m_selectedSprite);
            std::string label =
                s.name + "  [" + std::to_string(s.w) + "x" + std::to_string(s.h) + "]";
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_selectedSprite = s.name;
            }
        }
    }
    ImGui::End();
}

void SpriteEditorScene::drawSpriteProperties() {
    float scale = getGame()->getDPIScale();

    ImGui::SetNextWindowPos(ImVec2(720 * scale, 290 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350 * scale, 200 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Sprite Properties")) {
        if (m_selectedSprite.empty()) {
            ImGui::Text("No sprite selected.");
        } else {
            const auto* sprite = m_doc.findSprite(m_selectedSprite);
            if (sprite) {
                ImGui::Text("Name: %s", sprite->name.c_str());
                ImGui::Text("Position: (%d, %d)", sprite->x, sprite->y);
                ImGui::Text("Size: %d x %d", sprite->w, sprite->h);
                ImGui::Text("Anchor: (%.2f, %.2f)", sprite->anchorX, sprite->anchorY);
            }
        }
    }
    ImGui::End();
}

void SpriteEditorScene::drawAnimationEditor() {
    float scale = getGame()->getDPIScale();

    ImGui::SetNextWindowPos(ImVec2(720 * scale, 500 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350 * scale, 250 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Animation Editor")) {
        const auto& anims = m_doc.getAnimations();
        if (anims.empty()) {
            ImGui::Text("No animations. Use 'anim create <name>'.");
        } else {
            for (const auto& a : anims) {
                if (ImGui::TreeNode(a.name.c_str())) {
                    ImGui::Text("%s  [%zu frames, %.2fs]", a.looping ? "Loop" : "Once",
                                a.frames.size(), a.getTotalDuration());

                    for (size_t i = 0; i < a.frames.size(); ++i) {
                        ImGui::BulletText("[%zu] %s  (%.3fs)", i, a.frames[i].spriteName.c_str(),
                                          a.frames[i].duration);
                    }

                    // Play / stop buttons
                    if (m_animPlaying && m_playingAnimation == a.name) {
                        if (ImGui::Button("Stop")) {
                            m_animPlaying = false;
                            m_playingAnimation.clear();
                        }
                    } else {
                        std::string playLabel = "Play##" + a.name;
                        if (ImGui::Button(playLabel.c_str())) {
                            std::istringstream playIss(a.name);
                            // Reuse the anim play command path
                            m_playingAnimation = a.name;
                            m_animElapsed = 0.0f;
                            m_animPlaying = true;
                        }
                    }

                    ImGui::TreePop();
                }
            }
        }

        ImGui::Separator();
        ImGui::SliderFloat("Speed", &m_animSpeed, 0.1f, 5.0f, "%.1fx");
    }
    ImGui::End();
}

}  // namespace tools
}  // namespace vde
