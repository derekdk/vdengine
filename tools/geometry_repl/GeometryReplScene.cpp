/**
 * @file GeometryReplScene.cpp
 * @brief Implementation of Geometry REPL Scene
 */

#include "GeometryReplScene.h"

#include <algorithm>
#include <sstream>

#include <imgui.h>

namespace vde {
namespace tools {

GeometryReplScene::GeometryReplScene(ToolMode mode) : BaseToolScene(mode) {}

void GeometryReplScene::onEnter() {
    // Store DPI scale for UI scaling
    auto* game = getGame();
    if (game) {
        m_dpiScale = game->getDPIScale();
    }

    // --- Camera ---
    setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 10.0f, 30.0f, 20.0f));

    // --- Lighting ---
    auto lightBox = std::make_unique<vde::LightBox>();
    lightBox->setAmbientColor(vde::Color(0.3f, 0.3f, 0.35f));

    vde::Light sun = vde::Light::directional(vde::Direction(-0.5f, -1.0f, -0.3f),
                                             vde::Color(1.0f, 0.95f, 0.85f), 1.2f);
    lightBox->addLight(sun);
    setLightBox(std::move(lightBox));

    // Add a ground plane for reference (only in interactive mode)
    if (getToolMode() == ToolMode::INTERACTIVE) {
        auto plane = addEntity<vde::MeshEntity>();
        plane->setMesh(vde::Mesh::createPlane(20.0f, 20.0f, 10, 10));
        plane->setPosition(0.0f, -2.0f, 0.0f);
        plane->setColor(vde::Color(0.2f, 0.2f, 0.25f));
        plane->setName("Ground");

        // Add coordinate axes for reference
        createReferenceAxes();
    }

    // Welcome message
    addConsoleMessage("====================================================");
    addConsoleMessage("VDE Geometry REPL Tool");
    addConsoleMessage(
        "Mode: " + std::string(getToolMode() == ToolMode::INTERACTIVE ? "Interactive" : "Script"));
    addConsoleMessage("====================================================");
    addConsoleMessage("Type 'help' for command reference");
    addConsoleMessage("");
}

void GeometryReplScene::executeCommand(const std::string& cmdLine) {
    std::istringstream iss(cmdLine);
    std::string cmd;
    iss >> cmd;

    // Convert to lowercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "create") {
        cmdCreate(iss);
    } else if (cmd == "addpoint") {
        cmdAddPoint(iss);
    } else if (cmd == "setcolor") {
        cmdSetColor(iss);
    } else if (cmd == "setvisible") {
        cmdSetVisible(iss);
    } else if (cmd == "hide") {
        cmdHide(iss);
    } else if (cmd == "export") {
        cmdExport(iss);
    } else if (cmd == "list") {
        cmdList();
    } else if (cmd == "clear") {
        cmdClear(iss);
    } else if (cmd.empty()) {
        // Ignore empty commands
    } else {
        addConsoleMessage("ERROR: Unknown command '" + cmd + "'. Type 'help' for usage.");
    }
}

void GeometryReplScene::drawDebugUI() {
    float scale = m_dpiScale;

    // Main REPL Console Window
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Geometry REPL Console", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Commands")) {
                if (ImGui::MenuItem("Help")) {
                    executeCommand("help");
                }
                if (ImGui::MenuItem("List Objects")) {
                    executeCommand("list");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Console output area (scrollable)
        ImVec2 consoleSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2);
        if (ImGui::BeginChild("ConsoleOutput", consoleSize, true,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            for (const auto& msg : getConsoleLog()) {
                ImGui::TextWrapped("%s", msg.c_str());
            }

            // Auto-scroll to bottom when new messages arrive
            if (shouldScrollToBottom()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        // Command input area
        ImGui::Separator();
        ImGui::Text(">");
        ImGui::SameLine();

        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (ImGui::InputText("##input", m_commandBuffer, sizeof(m_commandBuffer), inputFlags)) {
            std::string command(m_commandBuffer);
            if (!command.empty()) {
                addConsoleMessage("> " + command);
                executeCommand(command);
                m_commandBuffer[0] = '\0';  // Clear input
            }
            ImGui::SetKeyboardFocusHere(-1);  // Keep focus on input
        }

        // Keep focus on input field
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();

    // Geometry Inspector Window
    ImGui::SetNextWindowPos(ImVec2(620 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Geometry Inspector")) {
        if (m_geometryObjects.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No geometry created yet");
            ImGui::TextWrapped("Use 'create <name> <type>' to create geometry");
        } else {
            for (auto& [name, geo] : m_geometryObjects) {
                if (ImGui::CollapsingHeader(name.c_str())) {
                    ImGui::Indent();

                    ImGui::Text("Type: %s", geo.type == GeometryType::POLYGON ? "Polygon" : "Line");
                    ImGui::Text("Points: %zu", geo.points.size());

                    if (ImGui::ColorEdit3(("Color##" + name).c_str(), &geo.color.x)) {
                        // Update mesh if visible
                        if (geo.visible && geo.entity) {
                            geo.entity->setColor(vde::Color(geo.color.x, geo.color.y, geo.color.z));
                        }
                    }

                    bool wasVisible = geo.visible;
                    ImGui::Checkbox(("Visible##" + name).c_str(), &geo.visible);

                    if (geo.visible != wasVisible) {
                        setGeometryVisible(name, geo.visible);
                    }

                    if (ImGui::Button(("Export##" + name).c_str())) {
                        std::string filename = name + ".obj";
                        if (geo.exportToOBJ(filename)) {
                            addConsoleMessage("Exported '" + name + "' to " + filename);
                        } else {
                            addConsoleMessage("ERROR: Failed to export '" + name + "'");
                        }
                    }

                    ImGui::Unindent();
                }
            }
        }
    }
    ImGui::End();

    // Stats Window
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 420 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280 * scale, 120 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Stats")) {
        auto* game = getGame();
        ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
        ImGui::Text("Geometry Objects: %zu", m_geometryObjects.size());
        ImGui::Text("Visible Objects: %zu", countVisibleGeometry());
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press F1 to toggle UI");
    }
    ImGui::End();
}

// Command implementations

void GeometryReplScene::cmdHelp() {
    addConsoleMessage("====================================================");
    addConsoleMessage("GEOMETRY REPL COMMANDS:");
    addConsoleMessage("  create <name> <type>        - Create geometry (polygon/line)");
    addConsoleMessage("  addpoint <name> <x> <y> <z> - Add point to geometry");
    addConsoleMessage("  setcolor <name> <r> <g> <b> - Set color (0-1 range)");
    addConsoleMessage("  setvisible <name>           - Show geometry in scene");
    addConsoleMessage("  hide <name>                 - Hide geometry from scene");
    addConsoleMessage("  export <name> <filename>    - Export to OBJ file");
    addConsoleMessage("  list                        - List all objects");
    addConsoleMessage("  clear <name>                - Delete geometry object");
    addConsoleMessage("  help                        - Show this help");
    addConsoleMessage("====================================================");
}

void GeometryReplScene::cmdCreate(std::istringstream& iss) {
    std::string name, typeStr;
    iss >> name >> typeStr;

    if (name.empty() || typeStr.empty()) {
        addConsoleMessage("ERROR: Usage: create <name> <type>");
        addConsoleMessage("       Types: polygon, line");
        return;
    }

    std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::tolower);

    GeometryType type;
    if (typeStr == "polygon") {
        type = GeometryType::POLYGON;
    } else if (typeStr == "line") {
        type = GeometryType::LINE;
    } else {
        addConsoleMessage("ERROR: Invalid type '" + typeStr + "'. Use: polygon, line");
        return;
    }

    if (m_geometryObjects.find(name) != m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' already exists");
        return;
    }

    GeometryObject geo;
    geo.name = name;
    geo.type = type;
    m_geometryObjects[name] = geo;

    addConsoleMessage("Created " + typeStr + " geometry '" + name + "'");
}

void GeometryReplScene::cmdAddPoint(std::istringstream& iss) {
    std::string name;
    float x, y, z;
    iss >> name >> x >> y >> z;

    if (name.empty() || iss.fail()) {
        addConsoleMessage("ERROR: Usage: addpoint <name> <x> <y> <z>");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    it->second.points.push_back(glm::vec3(x, y, z));
    addConsoleMessage("Added point (" + std::to_string(x) + ", " + std::to_string(y) + ", " +
                      std::to_string(z) + ") to '" + name + "'");

    // Update mesh if visible
    if (it->second.visible) {
        updateGeometryMesh(name);
    }
}

void GeometryReplScene::cmdSetColor(std::istringstream& iss) {
    std::string name;
    float r, g, b;
    iss >> name >> r >> g >> b;

    if (name.empty() || iss.fail()) {
        addConsoleMessage("ERROR: Usage: setcolor <name> <r> <g> <b>");
        addConsoleMessage("       Colors are in 0-1 range");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    it->second.color = glm::vec3(r, g, b);
    addConsoleMessage("Set color of '" + name + "' to (" + std::to_string(r) + ", " +
                      std::to_string(g) + ", " + std::to_string(b) + ")");

    // Update mesh if visible
    if (it->second.visible) {
        updateGeometryMesh(name);
    }
}

void GeometryReplScene::cmdSetVisible(std::istringstream& iss) {
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: setvisible <name>");
        return;
    }

    setGeometryVisible(name, true);
}

void GeometryReplScene::cmdHide(std::istringstream& iss) {
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: hide <name>");
        return;
    }

    setGeometryVisible(name, false);
}

void GeometryReplScene::cmdExport(std::istringstream& iss) {
    std::string name, filename;
    iss >> name >> filename;

    if (name.empty() || filename.empty()) {
        addConsoleMessage("ERROR: Usage: export <name> <filename>");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    if (it->second.exportToOBJ(filename)) {
        addConsoleMessage("Exported '" + name + "' to " + filename);
    } else {
        addConsoleMessage("ERROR: Failed to export '" + name + "'");
    }
}

void GeometryReplScene::cmdList() {
    if (m_geometryObjects.empty()) {
        addConsoleMessage("No geometry objects created");
        return;
    }

    addConsoleMessage("====================================================");
    addConsoleMessage("GEOMETRY OBJECTS:");
    for (const auto& [name, geo] : m_geometryObjects) {
        std::string typeStr = (geo.type == GeometryType::POLYGON) ? "polygon" : "line";
        std::string visStr = geo.visible ? "[VISIBLE]" : "[hidden]";
        addConsoleMessage("  " + name + " (" + typeStr + ", " + std::to_string(geo.points.size()) +
                          " points) " + visStr);
    }
    addConsoleMessage("====================================================");
}

void GeometryReplScene::cmdClear(std::istringstream& iss) {
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: clear <name>");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    // Remove entity from scene if visible
    if (it->second.entity) {
        removeEntity(it->second.entity->getId());
    }

    m_geometryObjects.erase(it);
    addConsoleMessage("Deleted geometry '" + name + "'");
}

// Helper methods

void GeometryReplScene::setGeometryVisible(const std::string& name, bool visible) {
    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    if (visible) {
        // Check if geometry has enough points
        if ((it->second.type == GeometryType::POLYGON && it->second.points.size() < 3) ||
            (it->second.type == GeometryType::LINE && it->second.points.size() < 2)) {
            addConsoleMessage("ERROR: '" + name + "' needs more points");
            return;
        }

        // Create or update mesh
        auto mesh = it->second.createMesh();
        if (!mesh) {
            addConsoleMessage("ERROR: Failed to create mesh for '" + name + "'");
            return;
        }

        if (!it->second.entity) {
            // Create new entity
            auto entity = addEntity<vde::MeshEntity>();
            entity->setMesh(mesh);
            entity->setColor(
                vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
            entity->setName(name);
            it->second.entity = entity;
        } else {
            // Update existing entity
            it->second.entity->setMesh(mesh);
            it->second.entity->setColor(
                vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
        }

        it->second.visible = true;
        addConsoleMessage("Made '" + name + "' visible");
    } else {
        // Hide geometry
        if (it->second.entity) {
            removeEntity(it->second.entity->getId());
            it->second.entity = nullptr;
        }
        it->second.visible = false;
        addConsoleMessage("Hid '" + name + "'");
    }
}

void GeometryReplScene::updateGeometryMesh(const std::string& name) {
    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end() || !it->second.visible) {
        return;
    }

    auto mesh = it->second.createMesh();
    if (mesh && it->second.entity) {
        it->second.entity->setMesh(mesh);
        it->second.entity->setColor(
            vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
    }
}

size_t GeometryReplScene::countVisibleGeometry() const {
    size_t count = 0;
    for (const auto& [name, geo] : m_geometryObjects) {
        if (geo.visible) {
            ++count;
        }
    }
    return count;
}

void GeometryReplScene::createReferenceAxes() {
    // X axis (red)
    auto xAxis = addEntity<vde::MeshEntity>();
    xAxis->setMesh(vde::Mesh::createCylinder(0.02f, 3.0f, 8));
    xAxis->setPosition(1.5f, 0.0f, 0.0f);
    xAxis->setRotation(0.0f, 0.0f, 90.0f);
    xAxis->setColor(vde::Color(1.0f, 0.0f, 0.0f));

    // Y axis (green)
    auto yAxis = addEntity<vde::MeshEntity>();
    yAxis->setMesh(vde::Mesh::createCylinder(0.02f, 3.0f, 8));
    yAxis->setPosition(0.0f, 1.5f, 0.0f);
    yAxis->setColor(vde::Color(0.0f, 1.0f, 0.0f));

    // Z axis (blue)
    auto zAxis = addEntity<vde::MeshEntity>();
    zAxis->setMesh(vde::Mesh::createCylinder(0.02f, 3.0f, 8));
    zAxis->setPosition(0.0f, 0.0f, 1.5f);
    zAxis->setRotation(90.0f, 0.0f, 0.0f);
    zAxis->setColor(vde::Color(0.0f, 0.0f, 1.0f));
}

}  // namespace tools
}  // namespace vde
