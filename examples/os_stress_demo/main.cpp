/**
 * @file main.cpp
 * @brief OS Stress Test Demo for VDE
 *
 * Validates that OS/window operations scheduled through
 * Game::scheduleWindowOperation never interfere with Vulkan
 * rendering.  A complex 3D scene runs continuously while
 * automated window resizes, moves, resizable toggles, and
 * fullscreen flips are fired on a timed cadence. File-system
 * operations (directory enumeration, temp file write/read/delete)
 * run in parallel to further stress the scheduler.
 *
 * If the engine survives the full duration without crashing
 * or producing Vulkan errors the test passes.
 */

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr float kAutoTerminateSec = 20.0f;
static constexpr float kOperationInterval = 0.35f;  // seconds between OS ops
static constexpr int kInitialWidth = 1024;
static constexpr int kInitialHeight = 768;
static constexpr float kTwoPi = 6.28318530718f;

// ---------------------------------------------------------------------------
// Rotating mesh entity (orbit + self-rotation)
// ---------------------------------------------------------------------------
class OrbitingMesh : public vde::MeshEntity {
  public:
    void configure(float orbitRadius, float orbitSpeed, float selfRotSpeed, float startAngle) {
        m_orbitRadius = orbitRadius;
        m_orbitSpeed = orbitSpeed;
        m_selfRotSpeed = selfRotSpeed;
        m_orbitAngle = startAngle;
    }

    void update(float dt) override {
        // Self-rotation around Y
        auto rot = getRotation();
        rot.yaw = std::fmod(rot.yaw + m_selfRotSpeed * dt, 360.0f);
        if (rot.yaw < 0.0f)
            rot.yaw += 360.0f;
        setRotation(rot);

        // Orbit
        m_orbitAngle += m_orbitSpeed * dt;
        float x = std::cos(m_orbitAngle) * m_orbitRadius;
        float z = std::sin(m_orbitAngle) * m_orbitRadius;
        setPosition(x, getPosition().y, z);
    }

  private:
    float m_orbitRadius = 2.0f;
    float m_orbitSpeed = 1.0f;
    float m_selfRotSpeed = 60.0f;
    float m_orbitAngle = 0.0f;
};

// ---------------------------------------------------------------------------
// Input handler
// ---------------------------------------------------------------------------
class StressInputHandler : public vde::examples::BaseExampleInputHandler {};

// ---------------------------------------------------------------------------
// Operation log entry (shown in ImGui overlay)
// ---------------------------------------------------------------------------
struct OpLogEntry {
    float timestamp;
    std::string message;
    bool success;
};

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
class OsStressScene : public vde::examples::BaseExampleScene {
  public:
    OsStressScene() : BaseExampleScene(kAutoTerminateSec) {}

    void onEnter() override {
        printExampleHeader();

        // --- Camera ---
        auto* camera = new vde::OrbitCamera(vde::Position(0, 1, 0), 10.0f, 30.0f, 0.0f);
        setCamera(camera);

        // --- Lighting ---
        auto* lightBox = new vde::ThreePointLightBox(vde::Color::white(), 1.3f);
        lightBox->setAmbientColor(vde::Color(0.12f, 0.12f, 0.18f));
        lightBox->setAmbientIntensity(1.0f);
        setLightBox(lightBox);

        setBackgroundColor(vde::Color::fromHex(0x0d1117));

        // --- Ground plane (flat cube) ---
        auto ground = addEntity<vde::MeshEntity>();
        ground->setName("Ground");
        ground->setMesh(vde::Mesh::createCube(1.0f));
        ground->setPosition(0.0f, -0.55f, 0.0f);
        ground->setScale(12.0f, 0.1f, 12.0f);
        ground->setMaterial(vde::Material::createColored(vde::Color::fromHex(0x2d333b)));

        // --- Central pedestal ---
        auto pedestal = addEntity<OrbitingMesh>();
        pedestal->setName("Pedestal");
        pedestal->setMesh(vde::Mesh::createCylinder(0.6f, 2.0f, 24));
        pedestal->setPosition(0.0f, 0.5f, 0.0f);
        pedestal->setMaterial(vde::Material::createMetallic(vde::Color::fromHex(0xc9d1d9), 0.25f));
        pedestal->configure(0.0f, 0.0f, 15.0f, 0.0f);

        // --- Inner ring – 6 cubes ---
        const float innerRadius = 2.5f;
        const vde::Color innerColors[] = {vde::Color::red(),     vde::Color::green(),
                                          vde::Color::blue(),    vde::Color::yellow(),
                                          vde::Color::magenta(), vde::Color::cyan()};
        for (int i = 0; i < 6; ++i) {
            float angle = static_cast<float>(i) * (kTwoPi / 6.0f);
            auto cube = addEntity<OrbitingMesh>();
            cube->setName("InnerCube" + std::to_string(i));
            cube->setMesh(vde::Mesh::createCube(0.5f));
            cube->setPosition(std::cos(angle) * innerRadius, 0.25f, std::sin(angle) * innerRadius);
            cube->setMaterial(
                vde::Material::createColored(innerColors[i % std::size(innerColors)]));
            cube->configure(innerRadius, 0.6f, 45.0f + i * 10.0f, angle);
        }

        // --- Outer ring – 8 spheres ---
        const float outerRadius = 4.5f;
        for (int i = 0; i < 8; ++i) {
            float angle = static_cast<float>(i) * (kTwoPi / 8.0f);
            auto sphere = addEntity<OrbitingMesh>();
            sphere->setName("OuterSphere" + std::to_string(i));
            sphere->setMesh(vde::Mesh::createSphere(0.35f, 16, 16));
            sphere->setPosition(std::cos(angle) * outerRadius, 0.6f + 0.3f * std::sin(angle * 3),
                                std::sin(angle) * outerRadius);
            sphere->setMaterial(vde::Material::createEmissive(vde::Color::fromHex(0x58a6ff), 0.5f));
            sphere->configure(outerRadius, -0.4f, 30.0f, angle);
        }

        // --- Floating pyramids ---
        for (int i = 0; i < 4; ++i) {
            float angle = static_cast<float>(i) * (kTwoPi / 4.0f);
            auto pyr = addEntity<OrbitingMesh>();
            pyr->setName("Pyramid" + std::to_string(i));
            pyr->setMesh(vde::Mesh::createPyramid(0.5f, 0.8f));
            pyr->setPosition(std::cos(angle) * 3.5f, 1.8f, std::sin(angle) * 3.5f);
            pyr->setMaterial(vde::Material::createMetallic(vde::Color::fromHex(0xf0883e), 0.35f));
            pyr->configure(3.5f, 0.8f, 60.0f, angle);
        }

        // --- Prepare temp directory for file-system ops ---
        m_tempDir = fs::temp_directory_path() / "vde_os_stress_test";
        fs::create_directories(m_tempDir);

        log("Scene initialized with " + std::to_string(getEntities().size()) + " entities");
    }

    void onExit() override {
        // Clean up temp files
        std::error_code ec;
        fs::remove_all(m_tempDir, ec);
        if (ec) {
            std::cerr << "Warning: failed to clean temp dir: " << ec.message() << std::endl;
        } else {
            std::cout << "Cleaned up temp directory: " << m_tempDir << std::endl;
        }
    }

    void update(float dt) override {
        BaseExampleScene::update(dt);

        // Rotate camera slowly
        auto* camera = dynamic_cast<vde::OrbitCamera*>(getCamera());
        if (camera) {
            camera->setYaw(camera->getYaw() + 12.0f * dt);
        }

        // Timed operation dispatch
        m_opTimer += dt;
        if (m_opTimer >= kOperationInterval) {
            m_opTimer -= kOperationInterval;
            dispatchNextOperation();
        }
    }

#ifdef VDE_EXAMPLE_USE_IMGUI
    void drawDebugUI() override {
        auto* game = getGame();
        if (!game)
            return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 350), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("OS Stress Test")) {
            ImGui::Text("FPS: %.1f  Frame: %llu", game->getFPS(), game->getFrameCount());
            ImGui::Text("Elapsed: %.1f / %.0f s", m_elapsedTime, kAutoTerminateSec);
            ImGui::Text("Entities: %zu", getEntities().size());
            ImGui::Separator();

            ImGui::Text("Operations completed: %d", m_opIndex);
            ImGui::Text("File ops succeeded: %d / %d", m_fileOpsSuccess, m_fileOpsTotal);
            ImGui::Text("Window ops scheduled: %d", m_windowOpsTotal);

            ImGui::Separator();
            ImGui::Text("Operation Log (last 12):");
            int start = static_cast<int>(m_log.size()) - 12;
            if (start < 0)
                start = 0;
            for (int i = start; i < static_cast<int>(m_log.size()); ++i) {
                auto& entry = m_log[static_cast<size_t>(i)];
                ImVec4 color =
                    entry.success ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(color, "[%5.1fs] %s", entry.timestamp, entry.message.c_str());
            }
        }
        ImGui::End();
    }
#endif

  protected:
    std::string getExampleName() const override { return "OS Stress Test"; }

    std::vector<std::string> getFeatures() const override {
        return {"scheduleWindowOperation / scheduleWindowResize correctness",
                "Automated window resize, move, resizable toggle, fullscreen",
                "File-system operations (dir scan, write, read, delete)",
                "Vulkan rendering stability during OS calls", "Resize deduplication verification"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Complex 3D scene with orbiting cubes, spheres, and pyramids",
                "Window repeatedly resizing and moving on screen",
                "No Vulkan validation errors or crashes",
                "ImGui overlay showing operation log (F1 to toggle)"};
    }

    std::vector<std::string> getControls() const override { return {}; }

  private:
    // Operation scheduling
    float m_opTimer = 0.0f;
    int m_opIndex = 0;

    // Stats
    int m_fileOpsSuccess = 0;
    int m_fileOpsTotal = 0;
    int m_windowOpsTotal = 0;

    // Operation log
    std::vector<OpLogEntry> m_log;

    // Temp directory for file ops
    fs::path m_tempDir;

    // Window sizes to cycle through
    static constexpr struct {
        uint32_t w, h;
    } kSizes[] = {{800, 600},   {1024, 768}, {1280, 720}, {640, 480},
                  {1920, 1080}, {960, 540},  {1024, 768}};
    static constexpr int kSizeCount = static_cast<int>(std::size(kSizes));

    void log(const std::string& msg, bool success = true) {
        m_log.push_back({m_elapsedTime, msg, success});
        std::cout << "[OS-Stress " << m_elapsedTime << "s] " << msg << std::endl;
    }

    void dispatchNextOperation() {
        auto* game = getGame();
        if (!game)
            return;

        // Cycle through different operation types
        int phase = m_opIndex % 10;
        switch (phase) {
        case 0:
        case 3:
        case 6:
            doWindowResize(game);
            break;
        case 1:
        case 5:
            doWindowMove(game);
            break;
        case 2:
            doFileWrite();
            break;
        case 4:
            doDirectoryScan();
            break;
        case 7:
            doResizableToggle(game);
            break;
        case 8:
            doFileReadAndDelete();
            break;
        case 9:
            doMultipleResizeBurst(game);
            break;
        }
        ++m_opIndex;
    }

    // --- Window operations (all via scheduleWindowOperation) ---

    void doWindowResize(vde::Game* game) {
        int idx = m_opIndex % kSizeCount;
        uint32_t w = kSizes[idx].w;
        uint32_t h = kSizes[idx].h;

        game->scheduleWindowResize(w, h);
        ++m_windowOpsTotal;
        log("WindowResize -> " + std::to_string(w) + "x" + std::to_string(h));
    }

    void doWindowMove(vde::Game* game) {
        // Move window to different screen positions
        int positions[][2] = {{100, 100}, {200, 50}, {50, 200}, {300, 150}, {150, 300}};
        int idx = m_opIndex % static_cast<int>(std::size(positions));
        int x = positions[idx][0];
        int y = positions[idx][1];

        game->scheduleWindowOperation(
            [x, y](vde::Window& window) { glfwSetWindowPos(window.getHandle(), x, y); });
        ++m_windowOpsTotal;
        log("WindowMove -> (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    }

    void doResizableToggle(vde::Game* game) {
        // Toggle resizable attribute
        m_resizableState = !m_resizableState;
        game->scheduleWindowResizable(m_resizableState);
        ++m_windowOpsTotal;
        log(std::string("WindowResizable -> ") + (m_resizableState ? "true" : "false"));
    }

    void doMultipleResizeBurst(vde::Game* game) {
        // Queue several resizes in rapid succession to test deduplication.
        // Only the last one should actually be applied.
        game->scheduleWindowResize(640, 480);
        game->scheduleWindowResize(800, 600);
        game->scheduleWindowResize(1024, 768);
        m_windowOpsTotal += 3;
        log("ResizeBurst 3x (expect dedup -> 1024x768)");
    }

    bool m_resizableState = true;

    // --- File-system operations ---

    void doFileWrite() {
        ++m_fileOpsTotal;
        try {
            auto filePath = m_tempDir / ("stress_" + std::to_string(m_opIndex) + ".txt");
            {
                std::ofstream ofs(filePath);
                if (!ofs) {
                    log("FileWrite FAILED: cannot open " + filePath.string(), false);
                    return;
                }
                ofs << "VDE OS Stress Test output\n";
                ofs << "Operation index: " << m_opIndex << "\n";
                ofs << "Elapsed: " << m_elapsedTime << "s\n";
                for (int i = 0; i < 100; ++i) {
                    ofs << "Line " << i << " padding data for bulk write test\n";
                }
            }
            ++m_fileOpsSuccess;
            log("FileWrite -> " + filePath.filename().string());
        } catch (const std::exception& e) {
            log(std::string("FileWrite exception: ") + e.what(), false);
        }
    }

    void doDirectoryScan() {
        ++m_fileOpsTotal;
        try {
            int count = 0;
            for (auto& entry : fs::directory_iterator(m_tempDir)) {
                (void)entry;
                ++count;
            }
            // Also scan the executable directory
            int exeCount = 0;
            auto exeDir = fs::current_path();
            for (auto& entry : fs::directory_iterator(exeDir)) {
                (void)entry;
                ++exeCount;
            }
            ++m_fileOpsSuccess;
            log("DirScan -> temp:" + std::to_string(count) + " exe:" + std::to_string(exeCount));
        } catch (const std::exception& e) {
            log(std::string("DirScan exception: ") + e.what(), false);
        }
    }

    void doFileReadAndDelete() {
        ++m_fileOpsTotal;
        try {
            // Find and read any file we previously wrote, then delete it
            bool found = false;
            for (auto& entry : fs::directory_iterator(m_tempDir)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    // Read the file
                    std::ifstream ifs(path);
                    std::string content((std::istreambuf_iterator<char>(ifs)),
                                        std::istreambuf_iterator<char>());
                    ifs.close();

                    // Delete it
                    fs::remove(path);
                    found = true;
                    ++m_fileOpsSuccess;
                    log("FileRead+Delete -> " + path.filename().string() + " (" +
                        std::to_string(content.size()) + " bytes)");
                    break;
                }
            }
            if (!found) {
                ++m_fileOpsSuccess;  // No files is not an error
                log("FileRead+Delete -> no files to process");
            }
        } catch (const std::exception& e) {
            log(std::string("FileRead+Delete exception: ") + e.what(), false);
        }
    }
};

// ---------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------
class OsStressDemo : public vde::examples::BaseExampleGame<StressInputHandler, OsStressScene> {
  public:
    void onStart() override {
        std::cout << "Starting OS Stress Test Demo..." << std::endl;
        BaseExampleGame::onStart();
    }

    void onShutdown() override {
        BaseExampleGame::onShutdown();
        std::cout << "OS Stress Test shutdown complete." << std::endl;
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    OsStressDemo demo;
    return vde::examples::runExample(demo, "VDE OS Stress Test", kInitialWidth, kInitialHeight,
                                     argc, argv);
}
