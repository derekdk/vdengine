/**
 * @file main.cpp
 * @brief Geometry REPL Tool - Interactive geometry creation and export
 *
 * An interactive tool that allows users to create custom 3D geometry through a
 * REPL (Read-Eval-Print Loop) interface using ImGui. Users can define geometry
 * by adding points, setting colors, and then visualize or export to OBJ format.
 *
 * Features demonstrated:
 * - ImGui-based command-line interface
 * - Dynamic mesh creation from user input
 * - 3D visualization of user-created geometry
 * - OBJ file export
 * - Real-time geometry editing
 *
 * Commands:
 *   create <name> <type>           - Create new geometry (<type>: polygon, line)
 *   addpoint <name> <x> <y> <z>    - Add a point to geometry
 *   setcolor <name> <r> <g> <b>    - Set fill color (0-1 range)
 *   setvisible <name>              - Make geometry visible in scene
 *   hide <name>                    - Hide geometry from scene
 *   export <name> <filename>       - Export geometry to OBJ file
 *   list                           - List all geometry objects
 *   clear <name>                   - Delete a geometry object
 *   help                           - Show command reference
 *
 * Controls:
 *   Left Mouse Drag - Rotate camera (when not over UI)
 *   Mouse Wheel     - Zoom camera in/out
 *   Mouse           - Interact with ImGui panels
 *   ESC             - Exit
 *   F               - Fail test
 */

#include <vde/VulkanContext.h>
#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// =============================================================================
// Geometry Object - represents user-created geometry
// =============================================================================

enum class GeometryType { POLYGON, LINE };

struct GeometryObject {
    std::string name;
    GeometryType type;
    std::vector<glm::vec3> points;
    glm::vec3 color{1.0f, 1.0f, 1.0f};  // Default white
    bool visible = false;
    std::shared_ptr<vde::MeshEntity> entity;

    // Create a mesh from the current points
    vde::ResourcePtr<vde::Mesh> createMesh() const {
        if (points.size() < 3 && type == GeometryType::POLYGON) {
            return nullptr;  // Need at least 3 points for a polygon
        }
        if (points.size() < 2 && type == GeometryType::LINE) {
            return nullptr;  // Need at least 2 points for a line
        }

        std::vector<vde::Vertex> vertices;
        std::vector<uint32_t> indices;

        if (type == GeometryType::POLYGON) {
            // Create vertices from points
            for (const auto& point : points) {
                vde::Vertex v;
                v.position = point;
                v.color = color;
                v.texCoord = glm::vec2(0.0f, 0.0f);
                vertices.push_back(v);
            }

            // Create triangle fan indices (assumes convex polygon)
            for (size_t i = 1; i < points.size() - 1; ++i) {
                indices.push_back(0);
                indices.push_back(static_cast<uint32_t>(i));
                indices.push_back(static_cast<uint32_t>(i + 1));
            }

            // Add reverse faces for double-sided rendering
            size_t baseIndex = vertices.size();
            for (const auto& point : points) {
                vde::Vertex v;
                v.position = point;
                v.color = color;
                v.texCoord = glm::vec2(0.0f, 0.0f);
                vertices.push_back(v);
            }

            for (size_t i = 1; i < points.size() - 1; ++i) {
                indices.push_back(static_cast<uint32_t>(baseIndex));
                indices.push_back(static_cast<uint32_t>(baseIndex + i + 1));
                indices.push_back(static_cast<uint32_t>(baseIndex + i));
            }
        } else if (type == GeometryType::LINE) {
            // Create line segments as thin rectangles
            const float lineWidth = 0.02f;
            for (size_t i = 0; i < points.size() - 1; ++i) {
                glm::vec3 p1 = points[i];
                glm::vec3 p2 = points[i + 1];
                glm::vec3 dir = glm::normalize(p2 - p1);

                // Create perpendicular vector
                glm::vec3 perp;
                if (std::abs(dir.y) < 0.9f) {
                    perp = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
                } else {
                    perp = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
                }
                perp *= lineWidth;

                uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

                // Create 4 vertices for the line segment
                vde::Vertex v1, v2, v3, v4;
                v1.position = p1 - perp;
                v1.color = color;
                v1.texCoord = glm::vec2(0, 0);

                v2.position = p1 + perp;
                v2.color = color;
                v2.texCoord = glm::vec2(0, 0);

                v3.position = p2 + perp;
                v3.color = color;
                v3.texCoord = glm::vec2(0, 0);

                v4.position = p2 - perp;
                v4.color = color;
                v4.texCoord = glm::vec2(0, 0);

                vertices.push_back(v1);
                vertices.push_back(v2);
                vertices.push_back(v3);
                vertices.push_back(v4);

                // Two triangles for the rectangle
                indices.push_back(baseIdx + 0);
                indices.push_back(baseIdx + 1);
                indices.push_back(baseIdx + 2);

                indices.push_back(baseIdx + 0);
                indices.push_back(baseIdx + 2);
                indices.push_back(baseIdx + 3);
            }
        }

        if (vertices.empty() || indices.empty()) {
            return nullptr;
        }

        auto mesh = std::make_shared<vde::Mesh>();
        mesh->setData(vertices, indices);
        return mesh;
    }

    // Export to OBJ format
    bool exportToOBJ(const std::string& filename) const {
        if (points.empty()) {
            return false;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "# Geometry: " << name << "\n";
        file << "# Type: " << (type == GeometryType::POLYGON ? "Polygon" : "Line") << "\n";
        file << "# Created with VDE Geometry REPL Tool\n\n";

        // Write vertices
        for (const auto& point : points) {
            file << "v " << point.x << " " << point.y << " " << point.z << "\n";
        }

        file << "\n";

        // Write faces (OBJ uses 1-based indexing)
        if (type == GeometryType::POLYGON && points.size() >= 3) {
            file << "# Face\nf";
            for (size_t i = 0; i < points.size(); ++i) {
                file << " " << (i + 1);
            }
            file << "\n";
        } else if (type == GeometryType::LINE && points.size() >= 2) {
            // Write line segments
            for (size_t i = 0; i < points.size() - 1; ++i) {
                file << "l " << (i + 1) << " " << (i + 2) << "\n";
            }
        }

        file.close();
        return true;
    }
};

// =============================================================================
// Input Handler
// =============================================================================

class GeometryReplInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Let base handler process ESC / F keys
        BaseExampleInputHandler::onKeyPress(key);
    }

    void onMouseButtonPress(int button, double x, double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = true;
            m_lastMouseX = x;
            m_lastMouseY = y;
        }
    }

    void onMouseButtonRelease(int button, [[maybe_unused]] double x,
                              [[maybe_unused]] double y) override {
        if (button == vde::MOUSE_BUTTON_LEFT) {
            m_mouseDown = false;
        }
    }

    void onMouseMove(double x, double y) override {
        if (m_mouseDown) {
            m_mouseDeltaX = x - m_lastMouseX;
            m_mouseDeltaY = y - m_lastMouseY;
        }
        m_lastMouseX = x;
        m_lastMouseY = y;
    }

    void onMouseScroll([[maybe_unused]] double xOffset, double yOffset) override {
        m_scrollDelta += static_cast<float>(yOffset);
    }

    bool isMouseDown() const { return m_mouseDown; }

    void getMouseDelta(double& dx, double& dy) {
        dx = m_mouseDeltaX;
        dy = m_mouseDeltaY;
        m_mouseDeltaX = 0.0;
        m_mouseDeltaY = 0.0;
    }

    float consumeScrollDelta() {
        float delta = m_scrollDelta;
        m_scrollDelta = 0.0f;
        return delta;
    }

  private:
    bool m_mouseDown = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    double m_mouseDeltaX = 0.0;
    double m_mouseDeltaY = 0.0;
    float m_scrollDelta = 0.0f;
};

// =============================================================================
// Scene
// =============================================================================

class GeometryReplScene : public vde::examples::BaseExampleScene {
  public:
    GeometryReplScene() : BaseExampleScene(300.0f) {}  // 5 minute timeout for tool usage

    void onEnter() override {
        printExampleHeader();

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

        // Add a ground plane for reference
        auto plane = addEntity<vde::MeshEntity>();
        plane->setMesh(vde::Mesh::createPlane(20.0f, 20.0f, 10, 10));
        plane->setPosition(0.0f, -2.0f, 0.0f);
        plane->setColor(vde::Color(0.2f, 0.2f, 0.25f));
        plane->setName("Ground");

        // Add coordinate axes for reference
        createReferenceAxes();

        // Add welcome message to console log
        addConsoleMessage("Geometry REPL Tool - Type 'help' for command reference");
        addConsoleMessage("====================================================");
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<GeometryReplInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Handle camera rotation with mouse drag (only if not over ImGui window)
        if (input->isMouseDown() && !ImGui::GetIO().WantCaptureMouse) {
            double dx, dy;
            input->getMouseDelta(dx, dy);
            if (dx != 0.0 || dy != 0.0) {
                auto* cam = dynamic_cast<vde::OrbitCamera*>(getCamera());
                if (cam) {
                    // Rotate camera based on mouse movement
                    cam->rotate(static_cast<float>(-dy) * 0.2f,  // pitch (inverted)
                                static_cast<float>(dx) * 0.2f);  // yaw
                }
            }
        }

        // Handle camera zoom with mouse wheel
        float scrollDelta = input->consumeScrollDelta();
        if (scrollDelta != 0.0f) {
            auto* cam = dynamic_cast<vde::OrbitCamera*>(getCamera());
            if (cam) {
                cam->zoom(scrollDelta * 0.8f);
            }
        }
    }

    void drawDebugUI() override {
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
                for (const auto& msg : m_consoleLog) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }

                // Auto-scroll to bottom when new messages arrive
                if (m_scrollToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                    m_scrollToBottom = false;
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

                        ImGui::Text("Type: %s",
                                    geo.type == GeometryType::POLYGON ? "Polygon" : "Line");
                        ImGui::Text("Points: %zu", geo.points.size());

                        if (ImGui::ColorEdit3(("Color##" + name).c_str(), &geo.color.x)) {
                            // Update mesh if visible
                            if (geo.visible && geo.entity) {
                                geo.entity->setColor(
                                    vde::Color(geo.color.x, geo.color.y, geo.color.z));
                            }
                        }

                        bool wasVisible = geo.visible;
                        ImGui::Checkbox(("Visible##" + name).c_str(), &geo.visible);

                        if (geo.visible != wasVisible) {
                            if (geo.visible) {
                                setGeometryVisible(name, true);
                            } else {
                                setGeometryVisible(name, false);
                            }
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

  protected:
    std::string getExampleName() const override { return "Geometry REPL Tool"; }

    std::vector<std::string> getFeatures() const override {
        return {"Interactive REPL command interface",
                "Custom geometry creation from points",
                "Dynamic mesh generation",
                "3D visualization",
                "OBJ file export",
                "Real-time color editing"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Ground plane and coordinate axes", "REPL console with command history",
                "Geometry inspector showing created objects",
                "User-created geometry visible when set visible"};
    }

    std::vector<std::string> getControls() const override {
        return {"Left Mouse Drag - Rotate camera", "Mouse Wheel - Zoom camera",
                "Mouse - Interact with UI", "Type commands in console input"};
    }

  private:
    std::map<std::string, GeometryObject> m_geometryObjects;
    std::vector<std::string> m_consoleLog;
    char m_commandBuffer[256] = {0};
    bool m_scrollToBottom = false;
    float m_dpiScale = 1.0f;

    void addConsoleMessage(const std::string& message) {
        m_consoleLog.push_back(message);

        // Limit console log size
        if (m_consoleLog.size() > 100) {
            m_consoleLog.erase(m_consoleLog.begin());
        }

        m_scrollToBottom = true;

        // Also print to stdout
        std::cout << message << std::endl;
    }

    void executeCommand(const std::string& cmdLine) {
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

    void cmdHelp() {
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

    void cmdCreate(std::istringstream& iss) {
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

    void cmdAddPoint(std::istringstream& iss) {
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

    void cmdSetColor(std::istringstream& iss) {
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

    void cmdSetVisible(std::istringstream& iss) {
        std::string name;
        iss >> name;

        if (name.empty()) {
            addConsoleMessage("ERROR: Usage: setvisible <name>");
            return;
        }

        setGeometryVisible(name, true);
    }

    void cmdHide(std::istringstream& iss) {
        std::string name;
        iss >> name;

        if (name.empty()) {
            addConsoleMessage("ERROR: Usage: hide <name>");
            return;
        }

        setGeometryVisible(name, false);
    }

    void cmdExport(std::istringstream& iss) {
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

    void cmdList() {
        if (m_geometryObjects.empty()) {
            addConsoleMessage("No geometry objects created");
            return;
        }

        addConsoleMessage("====================================================");
        addConsoleMessage("GEOMETRY OBJECTS:");
        for (const auto& [name, geo] : m_geometryObjects) {
            std::string typeStr = (geo.type == GeometryType::POLYGON) ? "polygon" : "line";
            std::string visStr = geo.visible ? "[VISIBLE]" : "[hidden]";
            addConsoleMessage("  " + name + " (" + typeStr + ", " +
                              std::to_string(geo.points.size()) + " points) " + visStr);
        }
        addConsoleMessage("====================================================");
    }

    void cmdClear(std::istringstream& iss) {
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

    void setGeometryVisible(const std::string& name, bool visible) {
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

    void updateGeometryMesh(const std::string& name) {
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

    size_t countVisibleGeometry() const {
        size_t count = 0;
        for (const auto& [name, geo] : m_geometryObjects) {
            if (geo.visible) {
                ++count;
            }
        }
        return count;
    }

    void createReferenceAxes() {
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
};

// =============================================================================
// Game – uses BaseExampleGame with built-in ImGui support
// =============================================================================

class GeometryReplGame
    : public vde::examples::BaseExampleGame<GeometryReplInputHandler, GeometryReplScene> {
  public:
    GeometryReplGame() = default;
    ~GeometryReplGame() override = default;
};

// =============================================================================
// Main
// =============================================================================

int main() {
    GeometryReplGame tool;

    // Adjust default resolution based on DPI
    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
    uint32_t width = static_cast<uint32_t>(1400 * dpiScale);
    uint32_t height = static_cast<uint32_t>(800 * dpiScale);

    return vde::examples::runExample(tool, "VDE Geometry REPL Tool", width, height);
}
