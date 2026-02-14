/**
 * @file GeometryReplScene.cpp
 * @brief Implementation of Geometry REPL Scene
 */

#include "GeometryReplScene.h"

#include <algorithm>
#include <optional>
#include <sstream>

#include "FileDialog.h"
#include <imgui.h>

namespace vde {
namespace tools {

// ============================================================================
// Construction / setup
// ============================================================================

GeometryReplScene::GeometryReplScene(ToolMode mode) : BaseToolScene(mode) {}

void GeometryReplScene::onEnter() {
    // Store DPI scale for UI scaling
    auto* game = getGame();
    if (game) {
        m_dpiScale = game->getDPIScale();
    }

    // --- Camera ---
    setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 10.0f, 30.0f, 20.0f));

    // --- Background color to match ground plane ---
    setBackgroundColor(vde::Color(0.0f, 0.0f, 0.0f));

    // --- Lighting ---
    auto lightBox = std::make_unique<vde::LightBox>();
    lightBox->setAmbientColor(vde::Color(0.3f, 0.3f, 0.35f));

    vde::Light sun = vde::Light::directional(vde::Direction(-0.5f, -1.0f, -0.3f),
                                             vde::Color(1.0f, 0.95f, 0.85f), 1.2f);
    lightBox->addLight(sun);
    setLightBox(std::move(lightBox));

    // Add coordinate axes for reference (only in interactive mode)
    if (getToolMode() == ToolMode::INTERACTIVE) {
        createReferenceAxes();
    }

    // --- Register commands ---
    registerCoreCommands();

    // --- Wire up the REPL console ---
    m_console.setCommandRegistry(&m_commands);
    m_console.setSubmitCallback([this](const std::string& cmdLine) {
        addConsoleMessage("> " + cmdLine);
        executeCommand(cmdLine);
    });

    // Welcome message
    addConsoleMessage("====================================================");
    addConsoleMessage("VDE Geometry REPL Tool");
    addConsoleMessage(
        "Mode: " + std::string(getToolMode() == ToolMode::INTERACTIVE ? "Interactive" : "Script"));
    addConsoleMessage("====================================================");
    addConsoleMessage("Type 'help' for command reference.  Press TAB to auto-complete.");
    addConsoleMessage("");
}

void GeometryReplScene::update(float deltaTime) {
    // Call base update first
    BaseToolScene::update(deltaTime);

    // Handle pending file dialog (deferred to between frames to avoid ImGui focus issues)
    if (m_pendingLoadDialog.has_value()) {
        auto request = *m_pendingLoadDialog;
        m_pendingLoadDialog.reset();

        // Clear ImGui input state before opening dialog
        ImGuiIO& io = ImGui::GetIO();
        io.AddFocusEvent(false);

        // Open the file dialog
        std::string filename = request.filename;
        if (filename.empty()) {
            filename =
                openFileDialog("Open OBJ File", {{"OBJ Files", "*.obj"}, {"All Files", "*.*"}});
        }

        // Restore focus and clear all key states after dialog closes
        io.AddFocusEvent(true);
        io.ClearInputKeys();
        io.ClearInputCharacters();

        // Process the file load if not cancelled
        if (!filename.empty()) {
            std::string name = request.name;

            // If no name provided, derive from filename
            if (name.empty()) {
                size_t lastSlash = filename.find_last_of("/\\");
                std::string stem =
                    (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
                size_t dot = stem.rfind('.');
                if (dot != std::string::npos) {
                    stem = stem.substr(0, dot);
                }
                name = stem;
            }

            // Check for duplicate name
            if (m_geometryObjects.find(name) != m_geometryObjects.end()) {
                addConsoleMessage("ERROR: Geometry '" + name + "' already exists. Use 'clear " +
                                  name + "' first.");
            } else {
                // Load the mesh via Mesh::loadFromFile
                auto mesh = std::make_shared<vde::Mesh>();
                if (!mesh->loadFromFile(filename)) {
                    addConsoleMessage("ERROR: Failed to load OBJ file: " + filename);
                } else {
                    // Create a geometry object to track it
                    GeometryObject geo;
                    geo.name = name;
                    geo.type = GeometryType::POLYGON;
                    geo.visible = true;
                    geo.isLoadedMesh = true;

                    // Store the original mesh data to preserve triangulation
                    const auto& verts = mesh->getVertices();
                    const auto& inds = mesh->getIndices();
                    geo.loadedVertices = verts;
                    geo.loadedIndices = inds;

                    // Copy vertex positions into points for the inspector
                    geo.points.reserve(verts.size());
                    for (const auto& v : verts) {
                        geo.points.push_back(v.position);
                    }

                    m_geometryObjects[name] = std::move(geo);

                    addConsoleMessage("Loaded '" + name + "' from " + filename + " (" +
                                      std::to_string(verts.size()) + " vertices)");

                    // Defer entity creation to the update phase so we don't add entities
                    // while the Vulkan command buffer is being recorded.
                    deferCommand([this, name]() {
                        auto it = m_geometryObjects.find(name);
                        if (it == m_geometryObjects.end()) {
                            return;
                        }

                        // Create mesh from stored geometry data
                        auto mesh = it->second.createMesh();
                        if (!mesh) {
                            return;
                        }

                        auto entity = addEntity<vde::MeshEntity>();
                        entity->setMesh(mesh);
                        entity->setColor(
                            vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
                        entity->setPosition(it->second.position);
                        entity->setName(name);
                        it->second.entity = entity;
                    });
                }
            }
        } else {
            addConsoleMessage("Load cancelled.");
        }

        // Refocus the console input
        m_console.focusInput();
    }
}

// ============================================================================
// Command dispatch (delegates to CommandRegistry)
// ============================================================================

void GeometryReplScene::executeCommand(const std::string& cmdLine) {
    if (!m_commands.execute(cmdLine)) {
        // Trim to get the first token for the error message
        std::istringstream iss(cmdLine);
        std::string cmd;
        iss >> cmd;
        if (!cmd.empty()) {
            addConsoleMessage("ERROR: Unknown command '" + cmd + "'. Type 'help' for usage.");
        }
    }
}

void GeometryReplScene::addConsoleMessage(const std::string& message) {
    // Forward to base class (stdout + base log)
    BaseToolScene::addConsoleMessage(message);
    // Also forward to the ReplConsole widget for display
    m_console.addMessage(message);
}

// ============================================================================
// Object name helper
// ============================================================================

std::vector<std::string> GeometryReplScene::getObjectNames() const {
    std::vector<std::string> names;
    names.reserve(m_geometryObjects.size());
    for (const auto& [name, _] : m_geometryObjects) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Command registration
// ============================================================================

void GeometryReplScene::registerCoreCommands() {
    using namespace std::placeholders;

    // help
    m_commands.add("help", "help [command]", "Show command reference",
                   [this](const std::string& a) { cmdHelp(a); });

    // create <name> <type>
    m_commands.add(
        "create", "create <name> <type>", "Create geometry (polygon/line)",
        [this](const std::string& a) { cmdCreate(a); }, createCompleter());

    // addpoint <name> <x> <y> <z>
    m_commands.add(
        "addpoint", "addpoint <name> <x> <y> <z>", "Add a point to geometry",
        [this](const std::string& a) { cmdAddPoint(a); }, objectNameCompleter());

    // setcolor <name> <r> <g> <b>
    m_commands.add(
        "setcolor", "setcolor <name> <r> <g> <b>", "Set color (0-1 range)",
        [this](const std::string& a) { cmdSetColor(a); }, objectNameCompleter());

    // setvisible <name>
    m_commands.add(
        "setvisible", "setvisible <name>", "Show geometry in scene",
        [this](const std::string& a) { cmdSetVisible(a); }, objectNameCompleter());

    // hide <name>
    m_commands.add(
        "hide", "hide <name>", "Hide geometry from scene",
        [this](const std::string& a) { cmdHide(a); }, objectNameCompleter());

    // export <name> <filename>
    m_commands.add(
        "export", "export <name> <filename>", "Export to OBJ file",
        [this](const std::string& a) { cmdExport(a); }, objectNameCompleter());

    // load <name> [filename]
    m_commands.add("load", "load <name> [filename]",
                   "Load OBJ file (omit filename for file browser)",
                   [this](const std::string& a) { cmdLoad(a); });

    // list
    m_commands.add("list", "list", "List all objects",
                   [this](const std::string& a) { cmdList(a); });

    // clear <name>
    m_commands.add(
        "clear", "clear <name>", "Delete geometry object",
        [this](const std::string& a) { cmdClear(a); }, objectNameCompleter());

    // move <name> <x> <y> <z>
    m_commands.add(
        "move", "move <name> <x> <y> <z>", "Move geometry by offset",
        [this](const std::string& a) { cmdMove(a); }, objectNameCompleter());

    // show-wireframe <name>
    m_commands.add(
        "show-wireframe", "show-wireframe <name>", "Show wireframe overlay",
        [this](const std::string& a) { cmdShowWireframe(a); }, objectNameCompleter());

    // wireframe-color <name> <r> <g> <b>
    m_commands.add(
        "wireframe-color", "wireframe-color <name> <r> <g> <b>", "Set wireframe color (0-1 range)",
        [this](const std::string& a) { cmdWireframeColor(a); }, objectNameCompleter());
}

// ============================================================================
// Completion helpers
// ============================================================================

CompletionCallback GeometryReplScene::objectNameCompleter() const {
    // Capture 'this' to access live object names at completion time
    return
        [this](
            const std::string& partial,
            [[maybe_unused]] const std::vector<std::string>& tokens) -> std::vector<std::string> {
            std::vector<std::string> results;
            std::string prefix = partial;
            std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);

            for (const auto& [name, _] : m_geometryObjects) {
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (prefix.empty() || lower.substr(0, prefix.size()) == prefix) {
                    results.push_back(name);
                }
            }
            return results;
        };
}

CompletionCallback GeometryReplScene::createCompleter() const {
    return []([[maybe_unused]] const std::string& partial,
              const std::vector<std::string>& tokens) -> std::vector<std::string> {
        // Second argument is the type
        if (tokens.size() >= 2) {
            // Complete type names
            std::vector<std::string> types = {"polygon", "line"};
            std::string prefix = partial;
            std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);

            std::vector<std::string> results;
            for (const auto& t : types) {
                if (prefix.empty() || t.substr(0, prefix.size()) == prefix) {
                    results.push_back(t);
                }
            }
            return results;
        }
        return {};
    };
}

// ============================================================================
// ImGui UI
// ============================================================================

void GeometryReplScene::drawDebugUI() {
    float scale = m_dpiScale;

    // --- Main REPL Console Window ---
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Geometry REPL Console", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Commands")) {
                // Build menu from enabled commands
                for (const auto* cmd : m_commands.getEnabledCommands()) {
                    if (ImGui::MenuItem(cmd->name.c_str())) {
                        if (cmd->name == "help" || cmd->name == "list") {
                            m_console.addMessage("> " + cmd->name);
                            executeCommand(cmd->name);
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", cmd->description.c_str());
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load OBJ...")) {
                    // Trigger load with no filename to open file browser
                    m_console.addMessage("> load (browse...)");
                    cmdLoad("");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Draw the console widget (output + input + completion)
        m_console.draw();
    }
    ImGui::End();

    // --- Geometry Inspector Window ---
    ImGui::SetNextWindowPos(ImVec2(620 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300 * scale, 400 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Geometry Inspector")) {
        if (m_geometryObjects.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No geometry created yet");
            ImGui::TextWrapped(
                "Use 'create <name> <type>' or 'load <name> <file>' to add geometry");
        } else {
            for (auto& [name, geo] : m_geometryObjects) {
                if (ImGui::CollapsingHeader(name.c_str())) {
                    ImGui::Indent();

                    ImGui::Text("Type: %s", geo.type == GeometryType::POLYGON ? "Polygon" : "Line");
                    ImGui::Text("Points: %zu", geo.points.size());

                    if (ImGui::ColorEdit3(("Color##" + name).c_str(), &geo.color.x)) {
                        if (geo.visible && geo.entity) {
                            geo.entity->setColor(vde::Color(geo.color.x, geo.color.y, geo.color.z));
                        }
                    }

                    bool wasVisible = geo.visible;
                    ImGui::Checkbox(("Visible##" + name).c_str(), &geo.visible);

                    if (geo.visible != wasVisible) {
                        setGeometryVisible(name, geo.visible);
                    }

                    bool wireframeEnabled = geo.showWireframe;
                    if (ImGui::Checkbox(("Wireframe##" + name).c_str(), &wireframeEnabled)) {
                        cmdShowWireframe(name);
                    }

                    ImGui::BeginDisabled(!geo.showWireframe);
                    if (ImGui::ColorEdit3(("Wire Color##" + name).c_str(), &geo.wireframeColor.x)) {
                        cmdWireframeColor(name + " " + std::to_string(geo.wireframeColor.x) + " " +
                                          std::to_string(geo.wireframeColor.y) + " " +
                                          std::to_string(geo.wireframeColor.z));
                    }
                    ImGui::EndDisabled();

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

    // --- Stats Window ---
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 420 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280 * scale, 140 * scale), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Stats")) {
        auto* game = getGame();
        ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
        ImGui::Text("Geometry Objects: %zu", m_geometryObjects.size());
        ImGui::Text("Visible Objects: %zu", countVisibleGeometry());
        ImGui::Text("Registered Commands: %zu", m_commands.getEnabledCommands().size());
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press F1 to toggle UI");
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press TAB in console to complete");
    }
    ImGui::End();
}

// ============================================================================
// Command implementations
// ============================================================================

void GeometryReplScene::cmdHelp(const std::string& args) {
    std::istringstream iss(args);
    std::string topic;
    iss >> topic;

    if (!topic.empty()) {
        // Show detailed help for a specific command
        const auto* cmd = m_commands.getCommand(topic);
        if (cmd) {
            addConsoleMessage("  " + cmd->usage);
            addConsoleMessage("  " + cmd->description);
            addConsoleMessage("  Status: " + std::string(cmd->enabled ? "enabled" : "DISABLED"));
        } else {
            addConsoleMessage("ERROR: Unknown command '" + topic + "'");
        }
        return;
    }

    addConsoleMessage("====================================================");
    addConsoleMessage("GEOMETRY REPL COMMANDS:");
    for (const auto* cmd : m_commands.getAllCommands()) {
        std::string status = cmd->enabled ? "" : " [DISABLED]";
        addConsoleMessage("  " + cmd->usage + "   - " + cmd->description + status);
    }
    addConsoleMessage("");
    addConsoleMessage("Type 'help <command>' for detailed info.");
    addConsoleMessage("Press TAB in the input field to auto-complete.");
    addConsoleMessage("====================================================");
}

void GeometryReplScene::cmdCreate(const std::string& args) {
    std::istringstream iss(args);
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

void GeometryReplScene::cmdAddPoint(const std::string& args) {
    std::istringstream iss(args);
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

    if (it->second.visible) {
        updateGeometryMesh(name);
    }
}

void GeometryReplScene::cmdSetColor(const std::string& args) {
    std::istringstream iss(args);
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

    if (it->second.visible) {
        updateGeometryMesh(name);
    }
}

void GeometryReplScene::cmdSetVisible(const std::string& args) {
    std::istringstream iss(args);
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: setvisible <name>");
        return;
    }

    setGeometryVisible(name, true);
}

void GeometryReplScene::cmdHide(const std::string& args) {
    std::istringstream iss(args);
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: hide <name>");
        return;
    }

    setGeometryVisible(name, false);
}

void GeometryReplScene::cmdExport(const std::string& args) {
    std::istringstream iss(args);
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

void GeometryReplScene::cmdLoad(const std::string& args) {
    std::istringstream iss(args);
    std::string name, filename;
    iss >> name >> filename;

    // Defer the file dialog to the update phase (between frames)
    // This prevents ImGui focus/input issues when the OS dialog steals focus
    PendingLoadDialog request;
    request.name = name;
    request.filename = filename;
    m_pendingLoadDialog = request;
}

void GeometryReplScene::cmdList(const std::string& /*args*/) {
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

void GeometryReplScene::cmdClear(const std::string& args) {
    std::istringstream iss(args);
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

    // Defer entity removal — the entity's GPU buffers may still be
    // referenced by the in-flight command buffer.
    if (it->second.entity) {
        auto entity = std::move(it->second.entity);
        EntityId eid = entity->getId();
        deferCommand([this, eid]() { removeEntity(eid); });
        retireResource(std::move(entity));
        it->second.entity = nullptr;
    }
    if (it->second.wireframeEntity) {
        auto wireEntity = std::move(it->second.wireframeEntity);
        EntityId wireId = wireEntity->getId();
        deferCommand([this, wireId]() { removeEntity(wireId); });
        retireResource(std::move(wireEntity));
        it->second.wireframeEntity = nullptr;
    }

    m_geometryObjects.erase(it);
    addConsoleMessage("Deleted geometry '" + name + "'");
}

void GeometryReplScene::cmdMove(const std::string& args) {
    std::istringstream iss(args);
    std::string name;
    float x, y, z;
    iss >> name >> x >> y >> z;

    if (name.empty() || iss.fail()) {
        addConsoleMessage("ERROR: Usage: move <name> <x> <y> <z>");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    it->second.position = glm::vec3(x, y, z);
    addConsoleMessage("Moved '" + name + "' to offset (" + std::to_string(x) + ", " +
                      std::to_string(y) + ", " + std::to_string(z) + ")");

    // Update entity position if it exists
    if (it->second.entity) {
        it->second.entity->setPosition(it->second.position);
    }
    if (it->second.wireframeEntity) {
        it->second.wireframeEntity->setPosition(it->second.position);
    }
}

void GeometryReplScene::cmdShowWireframe(const std::string& args) {
    std::istringstream iss(args);
    std::string name;
    iss >> name;

    if (name.empty()) {
        addConsoleMessage("ERROR: Usage: show-wireframe <name>");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    if (!it->second.visible) {
        addConsoleMessage("ERROR: '" + name + "' must be visible first. Use 'setvisible " + name +
                          "'");
        return;
    }

    it->second.showWireframe = !it->second.showWireframe;

    if (it->second.showWireframe) {
        addConsoleMessage("Enabled wireframe for '" + name + "'");
        updateGeometryMesh(name);  // This will create the wireframe entity
    } else {
        addConsoleMessage("Disabled wireframe for '" + name + "'");
        // Remove wireframe entity
        if (it->second.wireframeEntity) {
            auto entity = std::move(it->second.wireframeEntity);
            EntityId eid = entity->getId();
            deferCommand([this, eid]() { removeEntity(eid); });
            retireResource(std::move(entity));
            it->second.wireframeEntity = nullptr;
        }
    }
}

void GeometryReplScene::cmdWireframeColor(const std::string& args) {
    std::istringstream iss(args);
    std::string name;
    float r, g, b;
    iss >> name >> r >> g >> b;

    if (name.empty() || iss.fail()) {
        addConsoleMessage("ERROR: Usage: wireframe-color <name> <r> <g> <b>");
        addConsoleMessage("       Colors are in 0-1 range");
        return;
    }

    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    it->second.wireframeColor = glm::vec3(r, g, b);
    addConsoleMessage("Set wireframe color of '" + name + "' to (" + std::to_string(r) + ", " +
                      std::to_string(g) + ", " + std::to_string(b) + ")");

    if (it->second.showWireframe && it->second.visible) {
        updateGeometryMesh(name);
    }
}

// ============================================================================
// Scene helpers
// ============================================================================

void GeometryReplScene::setGeometryVisible(const std::string& name, bool visible) {
    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end()) {
        addConsoleMessage("ERROR: Geometry '" + name + "' not found");
        return;
    }

    if (visible) {
        // Early validation so the user gets immediate feedback
        if ((it->second.type == GeometryType::POLYGON && it->second.points.size() < 3) ||
            (it->second.type == GeometryType::LINE && it->second.points.size() < 2)) {
            addConsoleMessage("ERROR: '" + name + "' needs more points");
            it->second.visible = false;  // revert the ImGui checkbox
            return;
        }
        addConsoleMessage("Made '" + name + "' visible");
    } else {
        addConsoleMessage("Hid '" + name + "'");
    }

    // Defer the actual entity add/remove to the next update phase via
    // Scene::deferCommand(), so we never mutate entities while the Vulkan
    // command buffer is being recorded.
    deferCommand([this, name, visible]() {
        auto it = m_geometryObjects.find(name);
        if (it == m_geometryObjects.end()) {
            return;
        }

        if (visible) {
            // Re-check point count (could have changed between queue and apply)
            if ((it->second.type == GeometryType::POLYGON && it->second.points.size() < 3) ||
                (it->second.type == GeometryType::LINE && it->second.points.size() < 2)) {
                it->second.visible = false;
                return;
            }

            auto mesh = it->second.createMesh();
            if (!mesh) {
                it->second.visible = false;
                return;
            }

            if (!it->second.entity) {
                auto entity = addEntity<vde::MeshEntity>();
                entity->setMesh(mesh);
                entity->setColor(
                    vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
                entity->setPosition(it->second.position);
                entity->setName(name);
                it->second.entity = entity;
            } else {
                it->second.entity->setMesh(mesh);
                it->second.entity->setColor(
                    vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
                it->second.entity->setPosition(it->second.position);
            }
            it->second.visible = true;
        } else {
            if (it->second.entity) {
                removeEntity(it->second.entity->getId());
                retireResource(std::move(it->second.entity));
                it->second.entity = nullptr;
            }
            if (it->second.wireframeEntity) {
                removeEntity(it->second.wireframeEntity->getId());
                retireResource(std::move(it->second.wireframeEntity));
                it->second.wireframeEntity = nullptr;
            }
            it->second.visible = false;
        }
    });
}

void GeometryReplScene::updateGeometryMesh(const std::string& name) {
    auto it = m_geometryObjects.find(name);
    if (it == m_geometryObjects.end() || !it->second.visible) {
        return;
    }

    // Defer the mesh swap to the update phase so we don't free GPU buffers
    // while the command buffer is being recorded.
    deferCommand([this, name]() {
        auto it = m_geometryObjects.find(name);
        if (it == m_geometryObjects.end() || !it->second.visible || !it->second.entity) {
            return;
        }

        auto mesh = it->second.createMesh();
        if (mesh) {
            // Retire the old mesh so its GPU buffers stay alive until next frame
            auto oldMesh = it->second.entity->getMesh();
            if (oldMesh) {
                retireResource(std::move(oldMesh));
            }
            it->second.entity->setMesh(mesh);
            it->second.entity->setColor(
                vde::Color(it->second.color.x, it->second.color.y, it->second.color.z));
            it->second.entity->setPosition(it->second.position);

            // Handle wireframe overlay
            if (it->second.showWireframe) {
                auto wireframeMesh = it->second.createWireframeMesh(0.015f);
                if (wireframeMesh) {
                    if (!it->second.wireframeEntity) {
                        auto wireEntity = addEntity<vde::MeshEntity>();
                        wireEntity->setMesh(wireframeMesh);
                        wireEntity->setColor(vde::Color(it->second.wireframeColor.x,
                                                        it->second.wireframeColor.y,
                                                        it->second.wireframeColor.z));
                        wireEntity->setPosition(it->second.position);
                        wireEntity->setName(name + "_wireframe");
                        it->second.wireframeEntity = wireEntity;
                    } else {
                        auto oldWireMesh = it->second.wireframeEntity->getMesh();
                        if (oldWireMesh) {
                            retireResource(std::move(oldWireMesh));
                        }
                        it->second.wireframeEntity->setMesh(wireframeMesh);
                        it->second.wireframeEntity->setColor(
                            vde::Color(it->second.wireframeColor.x, it->second.wireframeColor.y,
                                       it->second.wireframeColor.z));
                        it->second.wireframeEntity->setPosition(it->second.position);
                    }
                }
            }
        }
    });
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
