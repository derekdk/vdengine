/**
 * @file main.cpp
 * @brief Mission Control Demo — live-updating TextEntity dashboard.
 *
 * Demonstrates Phase 3 TextEntity with:
 * - Mission clock counting up in HH:MM:SS.cc format, updating every frame
 * - Telemetry grid of 12 labeled sensor readings that oscillate with noise
 * - 6-line scrolling event log with messages appended every 2 seconds
 * - Alert banner switching between NOMINAL / CAUTION / WARNING states
 * - All panels use TextEntity with lazy dirty-flag rebuilds
 */

#include <vde/api/GameAPI.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Constants
// ============================================================================

static constexpr float VIEW_W = 16.0f;
static constexpr float VIEW_H = 12.0f;
static constexpr int TELEMETRY_ROWS = 12;
static constexpr int LOG_LINES = 6;

// ============================================================================
// Input handler
// ============================================================================

class MissionInputHandler : public vde::examples::BaseExampleInputHandler {};

// ============================================================================
// Scene
// ============================================================================

class MissionScene : public vde::examples::BaseExampleScene {
  public:
    MissionScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        setup2D(VIEW_W, VIEW_H, Color(0.03f, 0.03f, 0.10f, 1.0f));

        // ---- Mission Clock (top-left) ----
        m_clockLabel = addEntity<TextEntity>();
        m_clockLabel->setText("MISSION CLOCK");
        m_clockLabel->setFont(BitmapFont::small());
        m_clockLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_clockLabel->setAnchor(0.0f, 0.5f);
        m_clockLabel->setPosition(-VIEW_W * 0.5f + 0.3f, VIEW_H * 0.5f - 0.4f, 0.0f);
        m_clockLabel->setWorldHeight(0.35f);

        m_clockValue = addEntity<TextEntity>();
        m_clockValue->setText("00:00:00.00");
        m_clockValue->setFont(BitmapFont::large());
        m_clockValue->setStyle({.color = Color::green(), .pixelScale = 2, .letterSpacing = 1});
        m_clockValue->setAnchor(0.0f, 0.5f);
        m_clockValue->setPosition(-VIEW_W * 0.5f + 0.3f, VIEW_H * 0.5f - 1.0f, 0.0f);
        m_clockValue->setWorldHeight(0.55f);

        // ---- Telemetry Grid (left column) ----
        const char* labels[TELEMETRY_ROWS] = {"ALTITUDE", "VELOCITY", "FUEL PCT", "TEMP INT",
                                              "PRESSURE", "PITCH   ", "YAW     ", "ROLL    ",
                                              "O2 LEVEL", "CO2 LEVL", "POWER   ", "SIGNAL  "};
        float baseValues[TELEMETRY_ROWS] = {408.2f, 7820.f, 87.3f, 22.1f, 1013.f, 0.5f,
                                            -0.2f,  0.1f,   21.0f, 0.04f, 98.5f,  -42.0f};

        for (int i = 0; i < TELEMETRY_ROWS; ++i) {
            m_telemetryLabels[i] = std::string(labels[i]);
            m_telemetryBase[i] = baseValues[i];

            m_telemetryEntities[i] = addEntity<TextEntity>();
            m_telemetryEntities[i]->setFont(BitmapFont::small());
            m_telemetryEntities[i]->setStyle(
                {.color = Color::green(), .pixelScale = 1, .letterSpacing = 1});
            m_telemetryEntities[i]->setAnchor(0.0f, 0.5f);
            float y = VIEW_H * 0.5f - 1.8f - i * 0.55f;
            m_telemetryEntities[i]->setPosition(-VIEW_W * 0.5f + 0.3f, y, 0.0f);
            m_telemetryEntities[i]->setText(m_telemetryLabels[i] + "  " + formatValue(i, 0.0f));
            m_telemetryEntities[i]->setWorldHeight(0.35f);
        }

        // ---- Alert Banner (top-right) ----
        m_alertLabel = addEntity<TextEntity>();
        m_alertLabel->setText("STATUS");
        m_alertLabel->setFont(BitmapFont::small());
        m_alertLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_alertLabel->setAnchor(1.0f, 0.5f);
        m_alertLabel->setPosition(VIEW_W * 0.5f - 0.3f, VIEW_H * 0.5f - 0.4f, 0.0f);
        m_alertLabel->setWorldHeight(0.35f);

        m_alertBanner = addEntity<TextEntity>();
        m_alertBanner->setText("NOMINAL");
        m_alertBanner->setFont(BitmapFont::large());
        m_alertBanner->setStyle({.color = Color::green(), .pixelScale = 3, .letterSpacing = 1});
        m_alertBanner->setAnchor(1.0f, 0.5f);
        m_alertBanner->setPosition(VIEW_W * 0.5f - 0.3f, VIEW_H * 0.5f - 1.0f, 0.0f);
        m_alertBanner->setWorldHeight(0.65f);

        // ---- Event Log (right column, lower half) ----
        m_logTitle = addEntity<TextEntity>();
        m_logTitle->setText("EVENT LOG");
        m_logTitle->setFont(BitmapFont::small());
        m_logTitle->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_logTitle->setAnchor(1.0f, 0.5f);
        m_logTitle->setPosition(VIEW_W * 0.5f - 0.3f, VIEW_H * 0.5f - 2.0f, 0.0f);
        m_logTitle->setWorldHeight(0.35f);

        for (int i = 0; i < LOG_LINES; ++i) {
            m_logEntities[i] = addEntity<TextEntity>();
            m_logEntities[i]->setFont(BitmapFont::small());
            m_logEntities[i]->setStyle(
                {.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
            m_logEntities[i]->setAnchor(1.0f, 0.5f);
            float y = VIEW_H * 0.5f - 2.6f - i * 0.5f;
            m_logEntities[i]->setPosition(VIEW_W * 0.5f - 0.3f, y, 0.0f);
            m_logEntities[i]->setText("---");
            m_logEntities[i]->setWorldHeight(0.30f);
        }

        // Seed the log
        appendLog("SYSTEM BOOT COMPLETE");
        appendLog("ALL SUBSYSTEMS NOMINAL");
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);
        m_time += deltaTime;

        // ---- Mission Clock: every frame ----
        updateClock();

        // ---- Telemetry: every 0.5s ----
        m_telemetryAccum += deltaTime;
        if (m_telemetryAccum >= 0.5f) {
            m_telemetryAccum -= 0.5f;
            updateTelemetry();
        }

        // ---- Event Log: every 2s ----
        m_logAccum += deltaTime;
        if (m_logAccum >= 2.0f) {
            m_logAccum -= 2.0f;
            generateLogEvent();
        }

        // ---- Alert Banner: threshold-based ----
        updateAlert();
    }

  protected:
    std::string getExampleName() const override { return "Mission Control"; }

    std::vector<std::string> getFeatures() const override {
        return {"TextEntity with lazy dirty-flag texture rebuilds",
                "Mission clock updating every frame (HH:MM:SS.cc)",
                "12-row telemetry grid with oscillating sensor data",
                "6-line scrolling event log (new entry every 2s)",
                "Alert banner with NOMINAL/CAUTION/WARNING color states"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Green mission clock counting up at top-left",
                "12 green telemetry readouts down the left side",
                "NOMINAL/CAUTION/WARNING banner at top-right",
                "Scrolling event log messages on the right side", "Dark blue background"};
    }

  private:
    // ---- Clock ----
    std::shared_ptr<TextEntity> m_clockLabel;
    std::shared_ptr<TextEntity> m_clockValue;

    // ---- Telemetry ----
    std::shared_ptr<TextEntity> m_telemetryEntities[TELEMETRY_ROWS];
    std::string m_telemetryLabels[TELEMETRY_ROWS];
    float m_telemetryBase[TELEMETRY_ROWS]{};
    float m_telemetryAccum = 0.0f;

    // ---- Alert ----
    std::shared_ptr<TextEntity> m_alertLabel;
    std::shared_ptr<TextEntity> m_alertBanner;
    int m_alertState = 0;  // 0=nominal, 1=caution, 2=warning

    // ---- Event Log ----
    std::shared_ptr<TextEntity> m_logTitle;
    std::shared_ptr<TextEntity> m_logEntities[LOG_LINES];
    std::deque<std::string> m_logMessages;
    float m_logAccum = 0.0f;

    float m_time = 0.0f;

    // ---- Clock update ----
    void updateClock() {
        int totalCenti = static_cast<int>(m_time * 100.0f);
        int cs = totalCenti % 100;
        int totalSec = totalCenti / 100;
        int ss = totalSec % 60;
        int mm = (totalSec / 60) % 60;
        int hh = totalSec / 3600;

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%02d", hh, mm, ss, cs);
        m_clockValue->setText(buf);
    }

    // ---- Telemetry update ----
    std::string formatValue(int idx, float t) {
        float noise = std::sin(t * (1.3f + idx * 0.7f)) * (m_telemetryBase[idx] * 0.02f);
        float val = m_telemetryBase[idx] + noise;
        char buf[32];
        if (std::fabs(val) >= 100.0f)
            std::snprintf(buf, sizeof(buf), "%7.1f", static_cast<double>(val));
        else
            std::snprintf(buf, sizeof(buf), "%7.2f", static_cast<double>(val));
        return buf;
    }

    void updateTelemetry() {
        for (int i = 0; i < TELEMETRY_ROWS; ++i) {
            std::string line = m_telemetryLabels[i] + "  " + formatValue(i, m_time);
            m_telemetryEntities[i]->setText(line);
        }
    }

    // ---- Alert update ----
    void updateAlert() {
        // Cycle through states based on time
        int newState;
        float cycle = std::fmod(m_time, 12.0f);
        if (cycle < 6.0f)
            newState = 0;  // nominal
        else if (cycle < 9.0f)
            newState = 1;  // caution
        else
            newState = 2;  // warning

        if (newState != m_alertState) {
            m_alertState = newState;
            switch (m_alertState) {
            case 0:
                m_alertBanner->setText("NOMINAL");
                m_alertBanner->setStyle(
                    {.color = Color::green(), .pixelScale = 3, .letterSpacing = 1});
                break;
            case 1:
                m_alertBanner->setText("CAUTION");
                m_alertBanner->setStyle(
                    {.color = Color::yellow(), .pixelScale = 3, .letterSpacing = 1});
                break;
            case 2:
                m_alertBanner->setText("WARNING");
                m_alertBanner->setStyle(
                    {.color = Color::red(), .pixelScale = 3, .letterSpacing = 1});
                break;
            }
        }
    }

    // ---- Log ----
    void appendLog(const std::string& msg) {
        m_logMessages.push_front(msg);
        if (static_cast<int>(m_logMessages.size()) > LOG_LINES)
            m_logMessages.pop_back();

        for (int i = 0; i < LOG_LINES; ++i) {
            if (i < static_cast<int>(m_logMessages.size())) {
                m_logEntities[i]->setText(m_logMessages[i]);
            } else {
                m_logEntities[i]->setText("---");
            }
        }
    }

    void generateLogEvent() {
        static const char* events[] = {
            "TELEMETRY SYNC OK",      "ATTITUDE CORRECTION +0.1", "SOLAR PANEL ROTATION OK",
            "GROUND STATION HANDOFF", "TEMPERATURE NOMINAL",      "FUEL PRESSURE STABLE",
            "ORBIT ADJUST COMPLETE",  "COMMS LINK VERIFIED",      "BATTERY CHARGE 94 PCT",
            "SENSOR CALIBRATION OK",  "GYROSCOPE RECALIBRATED",   "THERMAL SHIELD NOMINAL",
        };
        int idx = static_cast<int>(m_time * 7.3f) % 12;
        appendLog(events[idx]);
    }
};

// ============================================================================
// Game
// ============================================================================

class MissionGame : public vde::examples::BaseExampleGame<MissionInputHandler, MissionScene> {
  public:
    MissionGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    MissionGame game;
    return vde::examples::runExample(game, "VDE Mission Control Demo", 1280, 720, argc, argv);
}
