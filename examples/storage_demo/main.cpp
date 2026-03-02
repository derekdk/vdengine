/**
 * @file main.cpp
 * @brief Persistent Storage Demo for VDE
 *
 * Demonstrates the StorageManager API using an ImGui panel UI.
 * Five data-type panels let you save and load values to a local SQLite database:
 *
 *   • String   – free text key/value
 *   • Integer  – 32-bit signed integer
 *   • Float    – single-precision float
 *   • Bool     – boolean toggle
 *   • Color    – RGB triple stored as a binary float[3] blob
 *   • Struct   – a PlayerProfile POD stored as a typed binary blob
 *
 * A scrollable operation log shows every save/load result so you can verify
 * persistence across runs – restart the demo and reload your keys.
 *
 * Controls:
 *   Mouse / keyboard – interact with ImGui panels
 *   ESC              – exit
 *   F                – mark test as failed
 */

#include <vde/VulkanContext.h>
#include <vde/Window.h>
#include <vde/api/GameAPI.h>
#include <vde/api/StorageManager.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "../ExampleBase.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// =============================================================================
// POD struct used for the "Struct" panel
// =============================================================================

struct PlayerProfile {
    char name[64];
    int score;
    int level;
    float health;
};
static_assert(std::is_trivially_copyable_v<PlayerProfile>);

// =============================================================================
// Helpers
// =============================================================================

namespace {

static constexpr const char* kAppName = "vde_storage_demo";

/// Colour constants for log messages
static constexpr ImVec4 kColOK = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
static constexpr ImVec4 kColErr = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
static constexpr ImVec4 kColInfo = ImVec4(0.8f, 0.8f, 0.5f, 1.0f);

}  // namespace

// =============================================================================
// Log
// =============================================================================

struct LogEntry {
    std::string message;
    ImVec4 colour;
};

class OperationLog {
  public:
    void add(const std::string& msg, const ImVec4& colour = kColInfo) {
        // Timestamp
        using namespace std::chrono;
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm_info{};
#if defined(_WIN32)
        localtime_s(&tm_info, &t);
#else
        localtime_r(&t, &tm_info);
#endif
        std::ostringstream ss;
        ss << std::setfill('0') << std::setw(2) << tm_info.tm_hour << ":" << std::setw(2)
           << tm_info.tm_min << ":" << std::setw(2) << tm_info.tm_sec << "." << std::setw(3)
           << ms.count() << "  " << msg;
        m_entries.push_back({ss.str(), colour});
        if (m_entries.size() > kMaxEntries) {
            m_entries.pop_front();
        }
        m_scrollToBottom = true;
    }

    void drawPanel(float scale) {
        ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(560 * scale, 160 * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Operation Log")) {
            if (ImGui::Button("Clear")) {
                m_entries.clear();
            }
            ImGui::Separator();
            ImGui::BeginChild("log_scroll", ImVec2(0, 0), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& e : m_entries) {
                ImGui::TextColored(e.colour, "%s", e.message.c_str());
            }
            if (m_scrollToBottom) {
                ImGui::SetScrollHereY(1.0f);
                m_scrollToBottom = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

  private:
    static constexpr std::size_t kMaxEntries = 200;
    std::deque<LogEntry> m_entries;
    bool m_scrollToBottom = false;
};

// =============================================================================
// Input Handler
// =============================================================================

class StorageDemoInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Suppress all base-class hotkeys (ESC, F, F1, F11, …) while ImGui
        // has keyboard focus so typing into text fields works normally.
        if (ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }
        BaseExampleInputHandler::onKeyPress(key);
    }
};

// =============================================================================
// Scene
// =============================================================================

class StorageDemoScene : public vde::examples::BaseExampleScene {
  public:
    StorageDemoScene() : BaseExampleScene(300.0f) {}  // 5-minute interactive timeout

    void onEnter() override {
        printExampleHeader();

        auto* game = getGame();
        if (game) {
            m_dpiScale = game->getDPIScale();
        }

        // Initialise storage
        bool ok = vde::StorageManager::getInstance().init_storage(kAppName);
        if (ok) {
            m_log.add(std::string("Storage opened  (app='") + kAppName + "')", kColOK);
        } else {
            m_log.add("ERROR: Failed to open storage!", kColErr);
        }

        // Set a minimal camera so the engine is happy (nothing actually renders)
        setCamera(new vde::OrbitCamera(vde::Position(0, 0, 0), 10.0f, 0.0f, 0.0f));
    }

    void update(float dt) override { BaseExampleScene::update(dt); }

    void drawDebugUI() override {
        float s = m_dpiScale;
        auto& storage = vde::StorageManager::getInstance();

        // ── Log panel (top) ──────────────────────────────────────────────────
        m_log.drawPanel(s);

        // ── String panel ────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(10 * s, 180 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 130 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("String")) {
            ImGui::InputText("Key##str", m_strKey, sizeof(m_strKey));
            ImGui::InputText("Value##str", m_strValue, sizeof(m_strValue));
            ImGui::Spacing();
            if (ImGui::Button("Save##str")) {
                if (storage.setStringData(m_strKey, m_strValue)) {
                    m_log.add(std::string("[string] Saved  key='") + m_strKey + "'  value='" +
                                  m_strValue + "'",
                              kColOK);
                } else {
                    m_log.add(std::string("[string] Save FAILED  key='") + m_strKey + "'", kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##str")) {
                auto result = storage.getStringData(m_strKey);
                if (result.has_value()) {
                    std::snprintf(m_strValue, sizeof(m_strValue), "%s", result->c_str());
                    m_log.add(std::string("[string] Loaded key='") + m_strKey + "'  value='" +
                                  m_strValue + "'",
                              kColOK);
                } else {
                    m_log.add(std::string("[string] Load FAILED – key not found: '") + m_strKey +
                                  "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── Integer panel ────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(290 * s, 180 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 130 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Integer")) {
            ImGui::InputText("Key##int", m_intKey, sizeof(m_intKey));
            ImGui::DragInt("Value##int", &m_intValue, 1, -1'000'000, 1'000'000);
            ImGui::Spacing();
            if (ImGui::Button("Save##int")) {
                if (storage.setBinData<int>(m_intKey, m_intValue)) {
                    m_log.add(std::string("[int] Saved  key='") + m_intKey +
                                  "'  value=" + std::to_string(m_intValue),
                              kColOK);
                } else {
                    m_log.add(std::string("[int] Save FAILED  key='") + m_intKey + "'", kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##int")) {
                auto result = storage.getBinData<int>(m_intKey);
                if (result.has_value()) {
                    m_intValue = *result;
                    m_log.add(std::string("[int] Loaded key='") + m_intKey +
                                  "'  value=" + std::to_string(m_intValue),
                              kColOK);
                } else {
                    m_log.add(std::string("[int] Load FAILED – key not found: '") + m_intKey + "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── Float panel ──────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(10 * s, 320 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 130 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Float")) {
            ImGui::InputText("Key##flt", m_fltKey, sizeof(m_fltKey));
            ImGui::DragFloat("Value##flt", &m_fltValue, 0.01f, -10'000.0f, 10'000.0f, "%.4f");
            ImGui::Spacing();
            if (ImGui::Button("Save##flt")) {
                if (storage.setBinData<float>(m_fltKey, m_fltValue)) {
                    m_log.add(std::string("[float] Saved  key='") + m_fltKey +
                                  "'  value=" + std::to_string(m_fltValue),
                              kColOK);
                } else {
                    m_log.add(std::string("[float] Save FAILED  key='") + m_fltKey + "'", kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##flt")) {
                auto result = storage.getBinData<float>(m_fltKey);
                if (result.has_value()) {
                    m_fltValue = *result;
                    m_log.add(std::string("[float] Loaded key='") + m_fltKey +
                                  "'  value=" + std::to_string(m_fltValue),
                              kColOK);
                } else {
                    m_log.add(std::string("[float] Load FAILED – key not found: '") + m_fltKey +
                                  "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── Bool panel ────────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(290 * s, 320 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 130 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Bool")) {
            ImGui::InputText("Key##bool", m_boolKey, sizeof(m_boolKey));
            ImGui::Checkbox("Value##bool", &m_boolValue);
            ImGui::Spacing();
            if (ImGui::Button("Save##bool")) {
                if (storage.setBinData<bool>(m_boolKey, m_boolValue)) {
                    m_log.add(std::string("[bool] Saved  key='") + m_boolKey +
                                  "'  value=" + (m_boolValue ? "true" : "false"),
                              kColOK);
                } else {
                    m_log.add(std::string("[bool] Save FAILED  key='") + m_boolKey + "'", kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##bool")) {
                auto result = storage.getBinData<bool>(m_boolKey);
                if (result.has_value()) {
                    m_boolValue = *result;
                    m_log.add(std::string("[bool] Loaded key='") + m_boolKey +
                                  "'  value=" + (*result ? "true" : "false"),
                              kColOK);
                } else {
                    m_log.add(std::string("[bool] Load FAILED – key not found: '") + m_boolKey +
                                  "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── Color panel (float[3] stored as raw blob) ─────────────────────────
        ImGui::SetNextWindowPos(ImVec2(10 * s, 460 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 130 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Color (float[3])")) {
            ImGui::InputText("Key##col", m_colKey, sizeof(m_colKey));
            ImGui::ColorEdit3("Value##col", m_colValue);
            ImGui::Spacing();

            struct RGB {
                float r, g, b;
            };
            static_assert(std::is_trivially_copyable_v<RGB>);

            if (ImGui::Button("Save##col")) {
                RGB v{m_colValue[0], m_colValue[1], m_colValue[2]};
                if (storage.setBinData<RGB>(m_colKey, v)) {
                    m_log.add(std::string("[color] Saved  key='") + m_colKey + "'  rgb=(" +
                                  std::to_string(v.r) + "," + std::to_string(v.g) + "," +
                                  std::to_string(v.b) + ")",
                              kColOK);
                } else {
                    m_log.add(std::string("[color] Save FAILED  key='") + m_colKey + "'", kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##col")) {
                auto result = storage.getBinData<RGB>(m_colKey);
                if (result.has_value()) {
                    m_colValue[0] = result->r;
                    m_colValue[1] = result->g;
                    m_colValue[2] = result->b;
                    m_log.add(std::string("[color] Loaded key='") + m_colKey + "'  rgb=(" +
                                  std::to_string(result->r) + "," + std::to_string(result->g) +
                                  "," + std::to_string(result->b) + ")",
                              kColOK);
                } else {
                    m_log.add(std::string("[color] Load FAILED – key not found: '") + m_colKey +
                                  "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── PlayerProfile struct panel ────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(290 * s, 460 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(270 * s, 230 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Struct (PlayerProfile)")) {
            ImGui::InputText("Key##prof", m_profKey, sizeof(m_profKey));
            ImGui::Separator();
            ImGui::InputText("Name", m_profile.name, sizeof(m_profile.name));
            ImGui::DragInt("Score", &m_profile.score, 1, 0, 999'999);
            ImGui::DragInt("Level", &m_profile.level, 1, 1, 100);
            ImGui::SliderFloat("Health", &m_profile.health, 0.0f, 100.0f);
            ImGui::Spacing();
            if (ImGui::Button("Save##prof")) {
                if (storage.setBinData<PlayerProfile>(m_profKey, m_profile)) {
                    m_log.add(std::string("[struct] Saved  key='") + m_profKey + "'  name='" +
                                  m_profile.name + "'  score=" + std::to_string(m_profile.score) +
                                  "  level=" + std::to_string(m_profile.level) +
                                  "  hp=" + std::to_string(m_profile.health),
                              kColOK);
                } else {
                    m_log.add(std::string("[struct] Save FAILED  key='") + m_profKey + "'",
                              kColErr);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Load##prof")) {
                auto result = storage.getBinData<PlayerProfile>(m_profKey);
                if (result.has_value()) {
                    m_profile = *result;
                    m_log.add(std::string("[struct] Loaded key='") + m_profKey + "'  name='" +
                                  m_profile.name + "'  score=" + std::to_string(m_profile.score) +
                                  "  level=" + std::to_string(m_profile.level) +
                                  "  hp=" + std::to_string(m_profile.health),
                              kColOK);
                } else {
                    m_log.add(std::string("[struct] Load FAILED – key not found: '") + m_profKey +
                                  "'",
                              kColErr);
                }
            }
        }
        ImGui::End();

        // ── Status bar ────────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(570 * s, 180 * s), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260 * s, 100 * s), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Storage Status")) {
            if (storage.isInitialized()) {
                ImGui::TextColored(kColOK, "OPEN");
            } else {
                ImGui::TextColored(kColErr, "CLOSED");
            }
            ImGui::TextDisabled("App: %s", kAppName);
            ImGui::Separator();
            auto* game = getGame();
            ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
        }
        ImGui::End();
    }

  protected:
    std::string getExampleName() const override { return "Persistent Storage"; }

    std::vector<std::string> getFeatures() const override {
        return {
            "StorageManager API demonstration",
            "SQLite-backed persistent key/value store",
            "String, int, float, bool, color, and struct types",
            "Data survives demo restarts",
        };
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {
            "Dark background with six ImGui panels",
            "Each panel has a key field, value editor, Save and Load buttons",
            "Operation log at top shows every save/load result with timestamp",
            "Values reload correctly after restarting the demo",
        };
    }

    std::vector<std::string> getControls() const override {
        return {
            "Mouse / keyboard – interact with panels",
            "Type a key, edit a value, press Save",
            "Change the value, press Load to restore from storage",
        };
    }

  private:
    float m_dpiScale = 1.0f;
    OperationLog m_log;

    // String
    char m_strKey[128] = "greeting";
    char m_strValue[256] = "Hello, world!";

    // Integer
    char m_intKey[128] = "high_score";
    int m_intValue = 0;

    // Float
    char m_fltKey[128] = "gravity";
    float m_fltValue = 9.81f;

    // Bool
    char m_boolKey[128] = "sound_enabled";
    bool m_boolValue = true;

    // Color
    char m_colKey[128] = "ui_tint";
    float m_colValue[3] = {0.2f, 0.6f, 1.0f};

    // PlayerProfile struct
    char m_profKey[128] = "player1";
    PlayerProfile m_profile = {"Hero", 0, 1, 100.0f};
};

// =============================================================================
// Game
// =============================================================================

class StorageDemoGame
    : public vde::examples::BaseExampleGame<StorageDemoInputHandler, StorageDemoScene> {
  public:
    StorageDemoGame() = default;
};

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    StorageDemoGame demo;

    float dpiScale = vde::Window::getPrimaryMonitorDPIScale();
    uint32_t width = static_cast<uint32_t>(870 * dpiScale);
    uint32_t height = static_cast<uint32_t>(720 * dpiScale);

    return vde::examples::runExample(demo, "VDE Storage Demo", width, height, argc, argv);
}
