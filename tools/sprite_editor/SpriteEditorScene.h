/**
 * @file SpriteEditorScene.h
 * @brief Scene implementation for the VDE Sprite Editor tool.
 *
 * Provides the command REPL, ImGui panels, and viewport rendering for
 * creating, editing, and exporting spritesheet assets.
 */

#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "../ToolBase.h"
#include "SpriteDocument.h"

namespace vde {
namespace tools {

/**
 * @brief Input handler for the Sprite Editor.
 *
 * Adds 2D-specific pan (middle-mouse) and zoom (scroll) on Camera2D,
 * overriding the base orbit-camera behavior.
 */
class SpriteEditorInputHandler : public BaseToolInputHandler {
  public:
    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_MIDDLE) {
            m_middleDown = true;
            m_lastMouseX = x;
            m_lastMouseY = y;
        }
        BaseToolInputHandler::onMouseButtonPress(button, x, y);
    }

    void onMouseButtonRelease(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_MIDDLE) {
            m_middleDown = false;
        }
        BaseToolInputHandler::onMouseButtonRelease(button, x, y);
    }

    void onMouseMove(double x, double y) override {
        if (m_middleDown) {
            m_panDeltaX = x - m_lastMouseX;
            m_panDeltaY = y - m_lastMouseY;
        }
        BaseToolInputHandler::onMouseMove(x, y);
    }

    bool isMiddleDown() const { return m_middleDown; }

    void getPanDelta(double& dx, double& dy) {
        dx = m_panDeltaX;
        dy = m_panDeltaY;
        m_panDeltaX = 0.0;
        m_panDeltaY = 0.0;
    }

  private:
    bool m_middleDown = false;
    double m_panDeltaX = 0.0;
    double m_panDeltaY = 0.0;
};

/**
 * @brief Main scene for the Sprite Editor tool.
 */
class SpriteEditorScene : public BaseToolScene {
  public:
    explicit SpriteEditorScene(ToolMode mode = ToolMode::INTERACTIVE);

    void onEnter() override;
    void update(float deltaTime) override;
    void drawDebugUI() override;

    // ── BaseToolScene interface ──────────────────────────────────
    void executeCommand(const std::string& cmdLine) override;
    std::string getToolName() const override { return "Sprite Editor"; }
    std::string getToolDescription() const override {
        return "Create, preview, and export spritesheet assets";
    }

    /// Access the underlying document (for testing).
    const SpriteDocument& getDocument() const { return m_doc; }

    /// Queue a command file to be executed at the end of onEnter().
    void setExecFile(const std::string& path) { m_execFile = path; }

  private:
    // ── Commands ─────────────────────────────────────────────────
    void cmdHelp();
    void cmdClear();
    void cmdInfo();
    void cmdLoad(std::istringstream& iss);
    void cmdSave(std::istringstream& iss);
    void cmdOpen(std::istringstream& iss);
    void cmdGrid(std::istringstream& iss);
    void cmdAdd(std::istringstream& iss);
    void cmdRemove(std::istringstream& iss);
    void cmdRename(std::istringstream& iss);
    void cmdAnchor(std::istringstream& iss);
    void cmdList();
    void cmdSelect(std::istringstream& iss);
    void cmdAnim(std::istringstream& iss);
    void cmdZoom(std::istringstream& iss);

    // ── UI drawing ───────────────────────────────────────────────
    void drawMenuBar();
    void drawConsole();
    void drawSpriteList();
    void drawSpriteProperties();
    void drawAnimationEditor();
    void drawAnimationPreview();

    // ── Camera helpers ───────────────────────────────────────────
    void handleCameraInput();
    void fitImageToView();

    // ── Data ─────────────────────────────────────────────────────
    SpriteDocument m_doc;

    // Textures
    std::shared_ptr<vde::Texture> m_sourceTexture;
    vde::SpriteEntity* m_sourceSprite = nullptr;

    // Animation preview
    vde::SpriteEntity* m_previewSprite = nullptr;
    std::string m_playingAnimation;
    float m_animElapsed = 0.0f;
    float m_animSpeed = 1.0f;
    bool m_animPlaying = false;

    // Selection
    std::string m_selectedSprite;

    // Console input buffer
    char m_inputBuffer[256] = {};

    // Camera (owned by Scene via setCamera)
    vde::Camera2D* m_camera2D = nullptr;

    // Optional command file to execute at end of onEnter()
    std::string m_execFile;
};

/**
 * @brief Game class for the Sprite Editor tool.
 */
class SpriteEditorGame : public BaseToolGame<SpriteEditorInputHandler, SpriteEditorScene> {
  public:
    SpriteEditorGame(ToolMode mode, const std::string& scriptFile = "",
                     const std::string& execFile = "")
        : BaseToolGame(mode), m_scriptFile(scriptFile), m_execFile(execFile) {}

    void onStart() override {
        BaseToolGame::onStart();

        if (m_toolMode == ToolMode::SCRIPT && !m_scriptFile.empty()) {
            auto* scene = getToolScene();
            if (scene && !scene->processScriptFile(m_scriptFile)) {
                m_exitCode = 1;
            }
            quit();
        } else if (m_toolMode == ToolMode::INTERACTIVE && !m_execFile.empty()) {
            auto* scene = getToolScene();
            if (scene) {
                std::cerr << "[sprite_editor] onStart: setting execFile='" << m_execFile << "'\n";
                scene->setExecFile(m_execFile);
            } else {
                std::cerr << "[sprite_editor] onStart: scene is null, cannot set execFile\n";
            }
        } else if (m_toolMode == ToolMode::INTERACTIVE) {
            std::cerr << "[sprite_editor] onStart: no execFile (m_execFile is empty)\n";
        }
    }

  private:
    std::string m_scriptFile;
    std::string m_execFile;
};

}  // namespace tools
}  // namespace vde
