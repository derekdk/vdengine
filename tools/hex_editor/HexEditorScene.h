#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "../ToolBase.h"
#include "../geometry_repl/FileDialog.h"

// ---------------------------------------------------------------------------
// Data model for a single opened file
// ---------------------------------------------------------------------------
struct HexFile {
    std::string path;
    std::string label;          // Short name shown in tabs
    std::vector<uint8_t> data;  // Raw bytes
    bool loaded = false;
};

// ---------------------------------------------------------------------------
// HexEditorScene
// ---------------------------------------------------------------------------
class HexEditorScene : public vde::tools::BaseToolScene {
  public:
    explicit HexEditorScene(vde::tools::ToolMode mode = vde::tools::ToolMode::INTERACTIVE);

    void onEnter() override;
    void executeCommand(const std::string& cmdLine) override;

    std::string getToolName() const override;
    std::string getToolDescription() const override;

    void drawDebugUI() override;

  private:
    // ----- data -----
    std::vector<HexFile> m_files;
    int m_selectedA = -1;  // Left panel file index
    int m_selectedB = -1;  // Right panel file index (compare)
    int m_bytesPerRow = 16;
    bool m_showCompare = false;
    bool m_showAscii = true;
    char m_openPathBuf[1024] = {};

    // ----- UI helpers -----
    void drawToolbar();
    void drawFileSelector();
    void drawHexPanel(const char* panelId, int fileIdx, bool highlightDiffs, int otherIdx);
    void drawComparePanel();
    void drawStatusBar();

    // ----- command handlers -----
    void cmdHelp();
    void cmdOpen(std::istringstream& iss);
    void cmdClose(std::istringstream& iss);
    void cmdList();

    // ----- internal helpers -----
    bool loadFile(const std::string& path);
    static std::string shortName(const std::string& path);
};
