/**
 * @file GeometryReplScene.h
 * @brief Geometry REPL Scene - interactive and scriptable geometry creation
 */

#pragma once

#include <map>
#include <string>

#include "../ToolBase.h"
#include "GeometryObject.h"

namespace vde {
namespace tools {

/**
 * @brief Geometry REPL scene for creating and managing geometry objects
 */
class GeometryReplScene : public BaseToolScene {
  public:
    explicit GeometryReplScene(ToolMode mode = ToolMode::INTERACTIVE);
    ~GeometryReplScene() override = default;

    void onEnter() override;
    void executeCommand(const std::string& cmdLine) override;
    void drawDebugUI() override;

    std::string getToolName() const override { return "Geometry REPL"; }

    std::string getToolDescription() const override {
        return "Interactive geometry creation and OBJ export tool";
    }

  private:
    std::map<std::string, GeometryObject> m_geometryObjects;
    char m_commandBuffer[256] = {0};
    float m_dpiScale = 1.0f;

    // Command handlers
    void cmdHelp();
    void cmdCreate(std::istringstream& iss);
    void cmdAddPoint(std::istringstream& iss);
    void cmdSetColor(std::istringstream& iss);
    void cmdSetVisible(std::istringstream& iss);
    void cmdHide(std::istringstream& iss);
    void cmdExport(std::istringstream& iss);
    void cmdList();
    void cmdClear(std::istringstream& iss);

    // Helper methods
    void setGeometryVisible(const std::string& name, bool visible);
    void updateGeometryMesh(const std::string& name);
    size_t countVisibleGeometry() const;
    void createReferenceAxes();
};

}  // namespace tools
}  // namespace vde
