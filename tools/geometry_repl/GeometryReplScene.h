/**
 * @file GeometryReplScene.h
 * @brief Geometry REPL Scene - interactive and scriptable geometry creation
 *
 * Uses CommandRegistry for dynamic command management and ReplConsole
 * for tab-completion and history in the interactive UI.
 */

#pragma once

#include <map>
#include <string>

#include "../ToolBase.h"
#include "CommandRegistry.h"
#include "GeometryObject.h"
#include "ReplConsole.h"

namespace vde {
namespace tools {

/**
 * @brief Geometry REPL scene for creating and managing geometry objects.
 *
 * Commands are registered dynamically via CommandRegistry and can be
 * added, removed, enabled, or disabled at runtime.
 */
class GeometryReplScene : public BaseToolScene {
  public:
    explicit GeometryReplScene(ToolMode mode = ToolMode::INTERACTIVE);
    ~GeometryReplScene() override = default;

    void onEnter() override;
    void executeCommand(const std::string& cmdLine) override;
    void drawDebugUI() override;
    void addConsoleMessage(const std::string& message) override;

    std::string getToolName() const override { return "Geometry REPL"; }

    std::string getToolDescription() const override {
        return "Interactive geometry creation and OBJ import/export tool";
    }

    void update(float deltaTime) override;

    /**
     * @brief Get the command registry (e.g. for external command registration).
     */
    CommandRegistry& getCommandRegistry() { return m_commands; }

    /**
     * @brief Get the console (e.g. for external message logging).
     */
    ReplConsole& getConsole() { return m_console; }

    /**
     * @brief Get the names of all geometry objects (for tab-completion).
     */
    std::vector<std::string> getObjectNames() const;

  private:
    CommandRegistry m_commands;
    ReplConsole m_console;
    std::map<std::string, GeometryObject> m_geometryObjects;
    float m_dpiScale = 1.0f;

    // Deferred file dialog handling
    struct PendingLoadDialog {
        std::string name;
        std::string filename;
    };
    std::optional<PendingLoadDialog> m_pendingLoadDialog;

    // --- Command registration ---
    void registerCoreCommands();

    // --- Command handlers ---
    void cmdHelp(const std::string& args);
    void cmdCreate(const std::string& args);
    void cmdAddPoint(const std::string& args);
    void cmdSetColor(const std::string& args);
    void cmdSetVisible(const std::string& args);
    void cmdHide(const std::string& args);
    void cmdExport(const std::string& args);
    void cmdLoad(const std::string& args);
    void cmdList(const std::string& args);
    void cmdClear(const std::string& args);
    void cmdMove(const std::string& args);
    void cmdShowWireframe(const std::string& args);
    void cmdWireframeColor(const std::string& args);

    // --- Completion helpers ---

    /**
     * @brief Build a completer that completes existing object names.
     */
    CompletionCallback objectNameCompleter() const;

    /**
     * @brief Build a completer for the "create" command (type names).
     */
    CompletionCallback createCompleter() const;

    // --- Scene helpers ---
    void setGeometryVisible(const std::string& name, bool visible);
    void updateGeometryMesh(const std::string& name);
    size_t countVisibleGeometry() const;
    void createReferenceAxes();
};

}  // namespace tools
}  // namespace vde
