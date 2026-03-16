/**
 * @file main.cpp
 * @brief Parallax scrolling demo for the VDE Game API.
 *
 * Demonstrates a layered 2D environment with independent looped scroll speeds,
 * keyboard playback controls, and an ImGui debug panel for per-layer tuning.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "../ExampleBase.h"

namespace {

constexpr float kViewWidth = 24.0f;
constexpr float kViewHeight = 13.5f;

enum class LayerId : size_t {
    Sky = 0,
    Clouds,
    Mountains,
    Lake,
    Foliage,
    Road,
    Count,
};

constexpr size_t kLayerCount = static_cast<size_t>(LayerId::Count);

struct MotionSpec {
    float frequency = 0.0f;
    float xAmplitude = 0.0f;
    float yAmplitude = 0.0f;
    float rollAmplitude = 0.0f;
    float widthScaleAmplitude = 0.0f;
    float heightScaleAmplitude = 0.0f;
    float colorPulse = 0.0f;
    float phase = 0.0f;
    vde::Color accentColor = vde::Color::white();
};

struct LayerState {
    const char* name = "";
    float speed = 0.0f;
    float defaultSpeed = 0.0f;
    float segmentWidth = kViewWidth;
    float offset = 0.0f;
};

enum class PieceMode {
    Wrapped,
    SunArc,
};

struct ParallaxPiece {
    vde::SpriteEntity* sprite = nullptr;
    LayerId layer = LayerId::Sky;
    PieceMode mode = PieceMode::Wrapped;
    int segment = 0;
    float localX = 0.0f;
    float localY = 0.0f;
    float depth = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
    float roll = 0.0f;
    float arcStartX = 0.0f;
    float arcEndX = 0.0f;
    float arcBaseY = 0.0f;
    float arcHeight = 0.0f;
    float arcDuration = 1.0f;
    float arcStartProgress = 0.0f;
    vde::Color color = vde::Color::white();
    MotionSpec motion;
};

size_t toIndex(LayerId layer) {
    return static_cast<size_t>(layer);
}

vde::Color blendColor(const vde::Color& a, const vde::Color& b, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return vde::Color(a.r + (b.r - a.r) * clamped, a.g + (b.g - a.g) * clamped,
                      a.b + (b.b - a.b) * clamped, a.a + (b.a - a.a) * clamped);
}

std::array<LayerState, kLayerCount> makeDefaultLayers() {
    return {{{"Sky & Sun", 0.05f, 0.05f, 32.0f, 0.0f},
             {"Clouds", 0.18f, 0.18f, 30.0f, 0.0f},
             {"Mountains", 0.42f, 0.42f, 28.0f, 0.0f},
             {"Lake", 0.78f, 0.78f, 26.0f, 0.0f},
             {"Foliage", 1.28f, 1.28f, 20.0f, 0.0f},
             {"Road", 2.25f, 2.25f, 16.0f, 0.0f}}};
}

class ParallaxInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE || key == vde::KEY_P) {
            m_togglePausePressed = true;
        }
        if (key == vde::KEY_DOWN || key == vde::KEY_MINUS || key == vde::KEY_LEFT_BRACKET) {
            m_slowDownPressed = true;
        }
        if (key == vde::KEY_UP || key == vde::KEY_EQUAL || key == vde::KEY_RIGHT_BRACKET) {
            m_speedUpPressed = true;
        }
        if (key == vde::KEY_R) {
            m_resetPressed = true;
        }
    }

    bool consumeTogglePause() {
        const bool pressed = m_togglePausePressed;
        m_togglePausePressed = false;
        return pressed;
    }

    bool consumeSlowDown() {
        const bool pressed = m_slowDownPressed;
        m_slowDownPressed = false;
        return pressed;
    }

    bool consumeSpeedUp() {
        const bool pressed = m_speedUpPressed;
        m_speedUpPressed = false;
        return pressed;
    }

    bool consumeReset() {
        const bool pressed = m_resetPressed;
        m_resetPressed = false;
        return pressed;
    }

  private:
    bool m_togglePausePressed = false;
    bool m_slowDownPressed = false;
    bool m_speedUpPressed = false;
    bool m_resetPressed = false;
};

class ParallaxScene : public vde::examples::BaseExampleScene {
  public:
    ParallaxScene() : BaseExampleScene(120.0f) {}

    void onEnter() override {
        printExampleHeader();
        setup2D(kViewWidth, kViewHeight, vde::Color::fromHex(0x8fd2ff));

        if (auto* camera = dynamic_cast<vde::Camera2D*>(getCamera())) {
            camera->setPosition(0.0f, 0.0f);
        }

        if (auto* game = getGame()) {
            m_dpiScale = std::max(game->getDPIScale(), 1.0f);
        }

        m_pieces.reserve(800);

        createSkyLayer();
        createCloudLayer();
        createMountainLayer();
        createLakeLayer();
        createFoliageLayer();
        createRoadLayer();
        applyAllPieces();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        handlePlaybackControls();

        if (!m_paused) {
            const float scaledDeltaTime = deltaTime * m_globalSpeed;
            m_playbackTime += scaledDeltaTime;

            for (auto& layer : m_layers) {
                advanceLayer(layer, scaledDeltaTime);
            }
        }

        applyAllPieces();
    }

    void drawDebugUI() override {
        const float scale = m_dpiScale;

        ImGui::SetNextWindowPos(ImVec2(12.0f * scale, 12.0f * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380.0f * scale, 470.0f * scale), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Parallax Controls")) {
            ImGui::End();
            return;
        }

        auto* game = getGame();
        ImGui::Text("FPS: %.1f", game ? game->getFPS() : 0.0f);
        ImGui::Text("Frame: %llu", game ? game->getFrameCount() : 0ULL);
        ImGui::Text("Playback Time: %.2fs", m_playbackTime);
        ImGui::Text("State: %s", m_paused ? "Paused" : "Running");
        ImGui::Separator();

        ImGui::Checkbox("Pause scrolling", &m_paused);
        if (ImGui::Button("Slower")) {
            m_globalSpeed = std::max(0.1f, m_globalSpeed - 0.2f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Faster")) {
            m_globalSpeed = std::min(3.5f, m_globalSpeed + 0.2f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            resetPlaybackSettings();
        }

        ImGui::SliderFloat("Global speed", &m_globalSpeed, 0.1f, 3.5f, "%.2fx");
        ImGui::Separator();
        ImGui::Text("Layer speeds");

        for (size_t i = 0; i < m_layers.size(); ++i) {
            auto& layer = m_layers[i];
            std::string label = std::string(layer.name) + "##layerSpeed" + std::to_string(i);
            ImGui::SliderFloat(label.c_str(), &layer.speed, 0.0f, 4.5f, "%.2f units/s");
            const float effectiveSpeed = m_paused ? 0.0f : layer.speed * m_globalSpeed;
            ImGui::TextDisabled("Effective: %.2f units/s", effectiveSpeed);
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Keys: SPACE or P pauses, UP speeds up, DOWN slows down, R resets defaults, "
            "and F1 toggles this panel.");

        ImGui::End();
    }

  protected:
    std::string getExampleName() const override { return "Parallax Scrolling"; }

    std::vector<std::string> getFeatures() const override {
        return {"Six looping parallax layers with a dedicated cloud band",
                "A single sun that drifts in a slow left-to-right sky arc",
                "Independent per-layer scroll tuning in the debug menu",
                "Runtime pause, slow down, and speed up controls",
                "Animated clouds, water shimmer, and swaying foreground foliage"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"A blue sky backdrop with a warm sun slowly arcing from left to right",
                "A separate cloud layer drifting in front of the sky",
                "Snow-capped mountains sliding slowly behind the scene",
                "A midground lake with bright shimmering highlights",
                "Foreground shrubbery, bushes, and trees swaying above a dirt road",
                "The whole landscape looping forever to imply a runner moving right"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE / P           - Pause or resume the scrolling",
                "UP / ] / =          - Speed up the playback",
                "DOWN / [ / -        - Slow down the playback",
                "R                   - Reset global and per-layer speeds"};
    }

  private:
    void handlePlaybackControls() {
        auto* input = dynamic_cast<ParallaxInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        if (input->consumeTogglePause()) {
            m_paused = !m_paused;
        }
        if (input->consumeSlowDown()) {
            m_globalSpeed = std::max(0.1f, m_globalSpeed - 0.2f);
        }
        if (input->consumeSpeedUp()) {
            m_globalSpeed = std::min(3.5f, m_globalSpeed + 0.2f);
        }
        if (input->consumeReset()) {
            resetPlaybackSettings();
        }
    }

    void resetPlaybackSettings() {
        m_paused = false;
        m_globalSpeed = 1.0f;
        for (auto& layer : m_layers) {
            layer.speed = layer.defaultSpeed;
        }
    }

    void advanceLayer(LayerState& layer, float scaledDeltaTime) {
        layer.offset -= layer.speed * scaledDeltaTime;

        while (layer.offset <= -layer.segmentWidth) {
            layer.offset += layer.segmentWidth;
        }
        while (layer.offset > 0.0f) {
            layer.offset -= layer.segmentWidth;
        }
    }

    void applyAllPieces() {
        for (auto& piece : m_pieces) {
            applyPiece(piece);
        }
    }

    int getSegmentRadius(LayerId layer) const {
        const float segmentWidth = m_layers[toIndex(layer)].segmentWidth;

        // Layers narrower than the view need additional wrapped copies so the
        // next repeat is already visible before the current one scrolls away.
        return std::max(1, static_cast<int>(std::ceil(kViewWidth / segmentWidth)) + 1);
    }

    void applyPiece(ParallaxPiece& piece) {
        const LayerState& layer = m_layers[toIndex(piece.layer)];
        const float angle = m_playbackTime * piece.motion.frequency + piece.motion.phase;
        const float wave = std::sin(angle);
        const float pulse = std::cos(angle);

        float x = piece.localX;
        float y = piece.localY;

        if (piece.mode == PieceMode::SunArc) {
            const float speedFactor =
                (layer.defaultSpeed > 0.0f) ? (layer.speed / layer.defaultSpeed) : 1.0f;
            float progress = piece.arcStartProgress;
            if (piece.arcDuration > 0.0f) {
                progress += (m_playbackTime * speedFactor) / piece.arcDuration;
            }
            progress -= std::floor(progress);

            x = piece.arcStartX + (piece.arcEndX - piece.arcStartX) * progress;
            y = piece.arcBaseY + std::sin(progress * std::numbers::pi_v<float>) * piece.arcHeight;
        } else {
            x += static_cast<float>(piece.segment) * layer.segmentWidth + layer.offset;
        }

        x += piece.motion.xAmplitude * wave;
        y += piece.motion.yAmplitude * pulse;
        const float width =
            std::max(0.02f, piece.width * (1.0f + piece.motion.widthScaleAmplitude * wave));
        const float height =
            std::max(0.02f, piece.height * (1.0f + piece.motion.heightScaleAmplitude * pulse));
        const float roll = piece.roll + piece.motion.rollAmplitude * wave;

        vde::Color color = piece.color;
        if (piece.motion.colorPulse > 0.0f) {
            const float mix = piece.motion.colorPulse * (0.5f + 0.5f * wave);
            color = blendColor(piece.color, piece.motion.accentColor, mix);
        }

        piece.sprite->setPosition(x, y, piece.depth);
        piece.sprite->setScale(width, height, 1.0f);
        piece.sprite->setRotation(0.0f, 0.0f, roll);
        piece.sprite->setColor(color);
    }

    void addPiece(LayerId layer, int segment, float localX, float localY, float depth, float width,
                  float height, const vde::Color& color, float roll = 0.0f,
                  const MotionSpec& motion = {}) {
        auto sprite = addEntity<vde::SpriteEntity>();
        sprite->setColor(color);
        sprite->setScale(width, height, 1.0f);
        sprite->setRotation(0.0f, 0.0f, roll);

        ParallaxPiece piece;
        piece.sprite = sprite.get();
        piece.layer = layer;
        piece.segment = segment;
        piece.localX = localX;
        piece.localY = localY;
        piece.depth = depth;
        piece.width = width;
        piece.height = height;
        piece.roll = roll;
        piece.color = color;
        piece.motion = motion;

        m_pieces.push_back(piece);
    }

    void addSunArcPiece(float depth, float width, float height, const vde::Color& color, float roll,
                        const MotionSpec& motion, float arcStartX, float arcEndX, float arcBaseY,
                        float arcHeight, float arcDuration, float arcStartProgress) {
        auto sprite = addEntity<vde::SpriteEntity>();
        sprite->setColor(color);
        sprite->setScale(width, height, 1.0f);
        sprite->setRotation(0.0f, 0.0f, roll);

        ParallaxPiece piece;
        piece.sprite = sprite.get();
        piece.layer = LayerId::Sky;
        piece.mode = PieceMode::SunArc;
        piece.depth = depth;
        piece.width = width;
        piece.height = height;
        piece.roll = roll;
        piece.arcStartX = arcStartX;
        piece.arcEndX = arcEndX;
        piece.arcBaseY = arcBaseY;
        piece.arcHeight = arcHeight;
        piece.arcDuration = arcDuration;
        piece.arcStartProgress = arcStartProgress;
        piece.color = color;
        piece.motion = motion;

        m_pieces.push_back(piece);
    }

    void addCloudCluster(int segment, float centerX, float centerY, float scale, float phase) {
        MotionSpec mainDrift;
        mainDrift.frequency = 0.30f + scale * 0.08f;
        mainDrift.yAmplitude = 0.05f * scale;
        mainDrift.widthScaleAmplitude = 0.03f;
        mainDrift.heightScaleAmplitude = 0.02f;
        mainDrift.phase = phase;
        mainDrift.colorPulse = 0.10f;
        mainDrift.accentColor = vde::Color(1.0f, 1.0f, 1.0f, 0.95f);

        MotionSpec secondaryDrift = mainDrift;
        secondaryDrift.phase += 0.65f;

        addPiece(LayerId::Clouds, segment, centerX - 0.95f * scale, centerY, -0.89f, 1.80f * scale,
                 0.72f * scale, vde::Color(0.94f, 0.97f, 1.0f, 0.76f), 0.0f, secondaryDrift);
        addPiece(LayerId::Clouds, segment, centerX, centerY + 0.12f * scale, -0.88f, 2.35f * scale,
                 0.96f * scale, vde::Color(1.0f, 1.0f, 1.0f, 0.82f), 0.0f, mainDrift);
        addPiece(LayerId::Clouds, segment, centerX + 1.00f * scale, centerY - 0.04f * scale, -0.89f,
                 1.65f * scale, 0.68f * scale, vde::Color(0.96f, 0.99f, 1.0f, 0.75f), 0.0f,
                 secondaryDrift);
        addPiece(LayerId::Clouds, segment, centerX + 0.25f * scale, centerY + 0.34f * scale, -0.87f,
                 1.30f * scale, 0.54f * scale, vde::Color(1.0f, 1.0f, 1.0f, 0.70f), 0.0f,
                 mainDrift);
    }

    void addMountain(int segment, float centerX, float baseY, float width, float height,
                     const vde::Color& bodyColor, const vde::Color& snowColor, float depth) {
        const vde::Color baseColor = blendColor(bodyColor, vde::Color::fromHex(0x2b3f62), 0.25f);

        addPiece(LayerId::Mountains, segment, centerX, baseY + height * 0.16f, depth - 0.02f,
                 width * 0.95f, height * 0.56f, baseColor);
        addPiece(LayerId::Mountains, segment, centerX - width * 0.19f, baseY + height * 0.42f,
                 depth, width * 0.76f, height * 0.21f, bodyColor, 33.0f);
        addPiece(LayerId::Mountains, segment, centerX + width * 0.19f, baseY + height * 0.42f,
                 depth, width * 0.76f, height * 0.21f, bodyColor, -33.0f);

        addPiece(LayerId::Mountains, segment, centerX, baseY + height * 0.62f, depth + 0.02f,
                 width * 0.18f, height * 0.18f, snowColor);
        addPiece(LayerId::Mountains, segment, centerX - width * 0.09f, baseY + height * 0.60f,
                 depth + 0.03f, width * 0.26f, height * 0.09f, snowColor, 18.0f);
        addPiece(LayerId::Mountains, segment, centerX + width * 0.09f, baseY + height * 0.60f,
                 depth + 0.03f, width * 0.26f, height * 0.09f, snowColor, -18.0f);
    }

    void addTree(int segment, float centerX, float groundY, float trunkHeight, float canopyScale,
                 float phase, const vde::Color& canopyColor, const vde::Color& highlightColor) {
        MotionSpec trunkMotion;
        trunkMotion.frequency = 1.0f;
        trunkMotion.xAmplitude = 0.02f;
        trunkMotion.rollAmplitude = 0.8f;
        trunkMotion.phase = phase;

        MotionSpec canopyMotion;
        canopyMotion.frequency = 1.0f;
        canopyMotion.xAmplitude = 0.07f;
        canopyMotion.yAmplitude = 0.03f;
        canopyMotion.rollAmplitude = 2.2f;
        canopyMotion.widthScaleAmplitude = 0.04f;
        canopyMotion.heightScaleAmplitude = 0.05f;
        canopyMotion.phase = phase;
        canopyMotion.colorPulse = 0.16f;
        canopyMotion.accentColor = highlightColor;

        addPiece(LayerId::Foliage, segment, centerX, groundY + trunkHeight * 0.50f, 0.02f,
                 0.18f * canopyScale, trunkHeight, vde::Color::fromHex(0x6d4a2c), 0.0f,
                 trunkMotion);

        addPiece(LayerId::Foliage, segment, centerX - 0.30f * canopyScale,
                 groundY + trunkHeight + 0.18f * canopyScale, 0.06f, 0.82f * canopyScale,
                 0.72f * canopyScale, canopyColor, -10.0f, canopyMotion);

        MotionSpec centerCanopyMotion = canopyMotion;
        centerCanopyMotion.phase += 0.35f;
        addPiece(LayerId::Foliage, segment, centerX + 0.05f * canopyScale,
                 groundY + trunkHeight + 0.38f * canopyScale, 0.07f, 0.96f * canopyScale,
                 0.88f * canopyScale, canopyColor, 4.0f, centerCanopyMotion);

        MotionSpec rightCanopyMotion = canopyMotion;
        rightCanopyMotion.phase += 0.70f;
        addPiece(LayerId::Foliage, segment, centerX + 0.32f * canopyScale,
                 groundY + trunkHeight + 0.14f * canopyScale, 0.06f, 0.74f * canopyScale,
                 0.66f * canopyScale, canopyColor, 12.0f, rightCanopyMotion);
    }

    void addBushCluster(int segment, float centerX, float baseY, float scale, float phase,
                        const vde::Color& bodyColor, const vde::Color& accentColor) {
        MotionSpec sway;
        sway.frequency = 1.35f;
        sway.xAmplitude = 0.05f * scale;
        sway.yAmplitude = 0.02f * scale;
        sway.widthScaleAmplitude = 0.03f;
        sway.heightScaleAmplitude = 0.05f;
        sway.phase = phase;
        sway.colorPulse = 0.14f;
        sway.accentColor = accentColor;

        addPiece(LayerId::Foliage, segment, centerX - 0.42f * scale, baseY + 0.22f * scale, 0.04f,
                 0.92f * scale, 0.58f * scale, bodyColor, 0.0f, sway);

        MotionSpec midSway = sway;
        midSway.phase += 0.45f;
        addPiece(LayerId::Foliage, segment, centerX + 0.10f * scale, baseY + 0.28f * scale, 0.05f,
                 1.08f * scale, 0.68f * scale, blendColor(bodyColor, accentColor, 0.18f), 0.0f,
                 midSway);

        MotionSpec rightSway = sway;
        rightSway.phase += 0.90f;
        addPiece(LayerId::Foliage, segment, centerX + 0.56f * scale, baseY + 0.18f * scale, 0.04f,
                 0.86f * scale, 0.54f * scale, bodyColor, 0.0f, rightSway);
    }

    void createSkyLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Sky);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            const float skyWidth = m_layers[toIndex(LayerId::Sky)].segmentWidth;

            addPiece(LayerId::Sky, segment, 0.0f, 0.70f, -0.99f, skyWidth + 1.0f, 13.0f,
                     vde::Color::fromHex(0x95d5ff));
            addPiece(LayerId::Sky, segment, 0.0f, 3.95f, -0.98f, skyWidth + 1.0f, 6.0f,
                     vde::Color(0.72f, 0.86f, 1.0f, 0.22f));
            addPiece(LayerId::Sky, segment, 0.0f, -2.55f, -0.97f, skyWidth + 1.0f, 4.8f,
                     vde::Color(0.88f, 0.95f, 1.0f, 0.16f));
        }

        MotionSpec sunGlow;
        sunGlow.frequency = 0.22f;
        sunGlow.widthScaleAmplitude = 0.06f;
        sunGlow.heightScaleAmplitude = 0.06f;
        sunGlow.phase = 0.25f;
        sunGlow.colorPulse = 0.10f;
        sunGlow.accentColor = vde::Color(1.0f, 0.97f, 0.85f, 0.28f);

        MotionSpec sunCore = sunGlow;
        sunCore.frequency = 0.35f;
        sunCore.widthScaleAmplitude = 0.03f;
        sunCore.heightScaleAmplitude = 0.03f;
        sunCore.colorPulse = 0.22f;
        sunCore.accentColor = vde::Color::fromHex(0xfff1b0);

        addSunArcPiece(-0.95f, 2.20f, 2.20f, vde::Color(1.0f, 0.91f, 0.60f, 0.18f), 45.0f, sunGlow,
                       -16.0f, 16.0f, 3.35f, 2.40f, 140.0f, 0.10f);
        addSunArcPiece(-0.94f, 1.12f, 1.12f, vde::Color::fromHex(0xffdd6b), 45.0f, sunCore, -16.0f,
                       16.0f, 3.35f, 2.40f, 140.0f, 0.10f);
    }

    void createCloudLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Clouds);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            addCloudCluster(segment, -10.00f, 4.05f, 1.05f, 0.25f);
            addCloudCluster(segment, -1.20f, 3.55f, 0.92f, 1.35f);
            addCloudCluster(segment, 6.80f, 4.45f, 1.18f, 2.15f);
            addCloudCluster(segment, 12.80f, 3.90f, 0.84f, 0.90f);
            addCloudCluster(segment, 17.40f, 4.20f, 1.10f, 1.80f);
        }
    }

    void createMountainLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Mountains);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            const float mountainWidth = m_layers[toIndex(LayerId::Mountains)].segmentWidth;
            addPiece(LayerId::Mountains, segment, 0.0f, 0.80f, -0.82f, mountainWidth + 0.8f, 1.30f,
                     vde::Color(0.70f, 0.82f, 0.90f, 0.20f));
            addPiece(LayerId::Mountains, segment, 0.0f, -0.05f, -0.78f, mountainWidth + 1.2f, 1.60f,
                     vde::Color::fromHex(0x7389a8));

            addMountain(segment, -10.60f, 0.45f, 6.20f, 4.30f, vde::Color::fromHex(0x7087a6),
                        vde::Color::fromHex(0xf5f8ff), -0.73f);
            addMountain(segment, -4.30f, 0.10f, 5.60f, 3.80f, vde::Color::fromHex(0x637998),
                        vde::Color::fromHex(0xf0f6ff), -0.72f);
            addMountain(segment, 3.00f, 0.05f, 7.10f, 4.80f, vde::Color::fromHex(0x5f7694),
                        vde::Color::fromHex(0xf5f9ff), -0.71f);
            addMountain(segment, 10.70f, 0.35f, 5.80f, 3.90f, vde::Color::fromHex(0x6880a0),
                        vde::Color::fromHex(0xf4f8ff), -0.72f);
        }
    }

    void createLakeLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Lake);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            const float lakeWidth = m_layers[toIndex(LayerId::Lake)].segmentWidth;

            addPiece(LayerId::Lake, segment, 0.0f, -1.55f, -0.40f, lakeWidth + 0.8f, 2.85f,
                     vde::Color::fromHex(0x2e78b7));
            addPiece(LayerId::Lake, segment, 0.0f, -0.40f, -0.39f, lakeWidth + 0.8f, 0.40f,
                     vde::Color(0.72f, 0.88f, 0.96f, 0.24f));
            addPiece(LayerId::Lake, segment, 0.0f, -0.05f, -0.38f, lakeWidth + 0.8f, 0.18f,
                     vde::Color::fromHex(0xcbd9e5));

            for (int i = 0; i < 10; ++i) {
                MotionSpec shimmer;
                shimmer.frequency = 1.15f + static_cast<float>(i) * 0.07f;
                shimmer.xAmplitude = 0.06f;
                shimmer.yAmplitude = 0.05f;
                shimmer.heightScaleAmplitude = 0.32f;
                shimmer.phase = 0.45f * static_cast<float>(i);
                shimmer.colorPulse = 0.60f;
                shimmer.accentColor = vde::Color(0.78f, 0.92f, 1.0f, 0.95f);

                const float localX =
                    -11.20f + static_cast<float>(i) * 2.45f + static_cast<float>(i % 2) * 0.35f;
                const float localY = -0.90f - static_cast<float>(i % 3) * 0.45f;
                const float width = 1.45f + static_cast<float>(i % 3) * 0.40f;
                const float height = 0.11f + static_cast<float>(i % 2) * 0.04f;

                addPiece(LayerId::Lake, segment, localX, localY, -0.34f, width, height,
                         vde::Color(0.40f, 0.73f, 0.92f, 0.78f), 0.0f, shimmer);
            }
        }
    }

    void createFoliageLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Foliage);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            const float foliageWidth = m_layers[toIndex(LayerId::Foliage)].segmentWidth;

            addPiece(LayerId::Foliage, segment, 0.0f, -3.15f, -0.10f, foliageWidth + 0.8f, 1.20f,
                     vde::Color::fromHex(0x48652e));
            addPiece(LayerId::Foliage, segment, 0.0f, -2.65f, -0.12f, foliageWidth + 0.6f, 0.58f,
                     vde::Color::fromHex(0x395724));

            addBushCluster(segment, -8.30f, -3.10f, 0.95f, 0.25f, vde::Color::fromHex(0x3d6a2b),
                           vde::Color::fromHex(0x7fba53));
            addBushCluster(segment, -4.90f, -3.00f, 1.05f, 1.15f, vde::Color::fromHex(0x355f26),
                           vde::Color::fromHex(0x6cab47));
            addBushCluster(segment, 1.60f, -3.08f, 1.15f, 2.10f, vde::Color::fromHex(0x436f2f),
                           vde::Color::fromHex(0x86be58));
            addBushCluster(segment, 7.70f, -3.02f, 0.90f, 2.80f, vde::Color::fromHex(0x345d25),
                           vde::Color::fromHex(0x6ea84c));

            addTree(segment, -7.00f, -3.15f, 1.55f, 1.15f, 0.40f, vde::Color::fromHex(0x355f28),
                    vde::Color::fromHex(0x7cb85d));
            addTree(segment, -1.20f, -3.15f, 1.90f, 1.30f, 1.30f, vde::Color::fromHex(0x2f5a24),
                    vde::Color::fromHex(0x73b550));
            addTree(segment, 5.80f, -3.15f, 1.65f, 1.18f, 2.05f, vde::Color::fromHex(0x3a642c),
                    vde::Color::fromHex(0x86bf62));
        }
    }

    void createRoadLayer() {
        const int segmentRadius = getSegmentRadius(LayerId::Road);
        for (int segment = -segmentRadius; segment <= segmentRadius; ++segment) {
            const float roadWidth = m_layers[toIndex(LayerId::Road)].segmentWidth;

            addPiece(LayerId::Road, segment, 0.0f, -5.45f, 0.34f, roadWidth + 0.8f, 2.20f,
                     vde::Color::fromHex(0x8c6239));
            addPiece(LayerId::Road, segment, 0.0f, -6.30f, 0.33f, roadWidth + 0.8f, 0.60f,
                     vde::Color::fromHex(0x5e4027));
            addPiece(LayerId::Road, segment, 0.0f, -4.52f, 0.36f, roadWidth + 0.8f, 0.34f,
                     vde::Color::fromHex(0xc7a16a));

            addPiece(LayerId::Road, segment, 0.0f, -5.80f, 0.37f, roadWidth + 0.3f, 0.10f,
                     vde::Color::fromHex(0x6d4b2c));
            addPiece(LayerId::Road, segment, 0.0f, -5.28f, 0.37f, roadWidth + 0.3f, 0.10f,
                     vde::Color::fromHex(0x7a5332));

            for (int i = 0; i < 6; ++i) {
                MotionSpec dustPulse;
                dustPulse.frequency = 1.60f + static_cast<float>(i) * 0.08f;
                dustPulse.heightScaleAmplitude = 0.18f;
                dustPulse.phase = 0.70f * static_cast<float>(i);
                dustPulse.colorPulse = 0.18f;
                dustPulse.accentColor = vde::Color::fromHex(0xd6b57b);

                addPiece(LayerId::Road, segment, -6.80f + static_cast<float>(i) * 2.70f,
                         -4.95f - static_cast<float>(i % 2) * 0.22f, 0.38f,
                         0.95f + static_cast<float>(i % 3) * 0.18f,
                         0.14f + static_cast<float>(i % 2) * 0.03f, vde::Color::fromHex(0xb88d5b),
                         0.0f, dustPulse);
            }

            for (int i = 0; i < 8; ++i) {
                const float localX = -7.00f + static_cast<float>(i) * 2.00f;
                const float localY = -5.15f - static_cast<float>(i % 3) * 0.32f;
                const float width = 0.18f + static_cast<float>(i % 3) * 0.08f;
                const float height = 0.11f + static_cast<float>(i % 2) * 0.05f;
                const float roll = -14.0f + static_cast<float>((i * 9) % 28);
                const vde::Color pebbleColor =
                    (i % 2 == 0) ? vde::Color::fromHex(0x6d5137) : vde::Color::fromHex(0x7d5e41);

                addPiece(LayerId::Road, segment, localX, localY, 0.39f, width, height, pebbleColor,
                         roll);
            }
        }
    }

    std::array<LayerState, kLayerCount> m_layers = makeDefaultLayers();
    std::vector<ParallaxPiece> m_pieces;
    float m_playbackTime = 0.0f;
    float m_globalSpeed = 1.0f;
    float m_dpiScale = 1.0f;
    bool m_paused = false;
};

class ParallaxDemoGame
    : public vde::examples::BaseExampleGame<ParallaxInputHandler, ParallaxScene> {
  public:
    ParallaxDemoGame() = default;
};

}  // namespace

int main(int argc, char** argv) {
    ParallaxDemoGame game;
    return vde::examples::runExample(game, "VDE Parallax Demo", 1280, 720, argc, argv);
}