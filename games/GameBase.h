/**
 * @file GameBase.h
 * @brief Shared base classes and utilities for VDE games.
 *
 * This header provides common functionality for larger game applications:
 * - Standard ESC / F1 / F11 handling
 * - Optional ImGui debug UI integration
 * - Shared game header output
 * - Input-script compatible run helper for smoke tests
 */

#pragma once

#include <vde/Window.h>
#include <vde/api/GameAPI.h>

#include <cinttypes>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef VDE_GAME_USE_IMGUI
#include <vde/VulkanContext.h>
#include <vde/api/EmojiFont.h>

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

namespace vde {
namespace games {

inline void setWorkingDirectoryToExecutablePath() {
#ifdef _WIN32
    char exePathBuffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, exePathBuffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return;
    }

    std::error_code error;
    std::filesystem::path exeDir = std::filesystem::path(exePathBuffer).parent_path();
    if (!exeDir.empty()) {
        std::filesystem::current_path(exeDir, error);
    }
#endif
}

class BaseGameInputHandler : public vde::InputHandler {
  public:
    BaseGameInputHandler() = default;
    virtual ~BaseGameInputHandler() = default;

    void onKeyPress(int key) override {
        if (key == vde::KEY_ESCAPE) {
            m_escapePressed = true;
        }
        if (key == vde::KEY_F11) {
            m_fullscreenTogglePressed = true;
        }
        if (key == vde::KEY_F1) {
            m_debugUITogglePressed = true;
        }
    }

    bool isEscapePressed() {
        bool value = m_escapePressed;
        m_escapePressed = false;
        return value;
    }

    bool isFullscreenTogglePressed() {
        bool value = m_fullscreenTogglePressed;
        m_fullscreenTogglePressed = false;
        return value;
    }

    bool isDebugUITogglePressed() {
        bool value = m_debugUITogglePressed;
        m_debugUITogglePressed = false;
        return value;
    }

  protected:
    bool m_escapePressed = false;
    bool m_fullscreenTogglePressed = false;
    bool m_debugUITogglePressed = false;
};

class BaseGameScene : public vde::Scene {
  public:
    BaseGameScene() = default;
    virtual ~BaseGameScene() = default;

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        auto* input = dynamic_cast<BaseGameInputHandler*>(getInputHandler());
        if (!input) {
            return;
        }

        if (input->isFullscreenTogglePressed()) {
            if (getGame() && getGame()->getWindow()) {
                auto* window = getGame()->getWindow();
                window->setFullscreen(!window->isFullscreen());
            }
        }

        if (input->isDebugUITogglePressed()) {
            m_debugUIVisible = !m_debugUIVisible;
        }

        if (input->isEscapePressed() && getGame()) {
            getGame()->quit();
        }
    }

    bool isDebugUIVisible() const { return m_debugUIVisible; }
    void setDebugUIVisible(bool visible) { m_debugUIVisible = visible; }

    virtual void drawDebugUI() {
#ifdef VDE_GAME_USE_IMGUI
        auto* game = getGame();
        if (!game) {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(310, 150), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Game Debug")) {
            ImGui::Text("FPS: %.1f", game->getFPS());
            ImGui::Text("Frame: %" PRIu64, game->getFrameCount());
            ImGui::Text("Delta: %.3f ms", game->getDeltaTime() * 1000.0f);
            ImGui::Text("Entities: %zu", getEntities().size());
            ImGui::Text("DPI Scale: %.2f", game->getDPIScale());
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press F1 to toggle");
        }
        ImGui::End();
#endif
    }

    virtual void onBeforeImGuiShutdown() {}

    void printGameHeader() const {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  VDE Game: " << getGameName() << std::endl;
        std::cout << "========================================\n" << std::endl;

        std::cout << "Gameplay:" << std::endl;
        for (const auto& item : getGameplaySummary()) {
            std::cout << "  - " << item << std::endl;
        }

        std::cout << "\nGoals:" << std::endl;
        for (const auto& item : getGoals()) {
            std::cout << "  - " << item << std::endl;
        }

        std::cout << "\nControls:" << std::endl;
        for (const auto& item : getControls()) {
            std::cout << "  " << item << std::endl;
        }
        std::cout << "  F11   - Toggle fullscreen" << std::endl;
        std::cout << "  F1    - Toggle debug UI" << std::endl;
        std::cout << "  ESC   - Quit" << std::endl;
        std::cout << std::endl;
    }

  protected:
    virtual std::string getGameName() const = 0;
    virtual std::vector<std::string> getGameplaySummary() const = 0;
    virtual std::vector<std::string> getGoals() const = 0;
    virtual std::vector<std::string> getControls() const { return {}; }

  private:
    bool m_debugUIVisible = true;
};

template <typename TInputHandler, typename TScene>
class BaseGame : public vde::Game {
    static_assert(std::is_base_of<BaseGameInputHandler, TInputHandler>::value,
                  "TInputHandler must derive from BaseGameInputHandler");
    static_assert(std::is_base_of<BaseGameScene, TScene>::value,
                  "TScene must derive from BaseGameScene");

  public:
    BaseGame() = default;
    virtual ~BaseGame() {
#ifdef VDE_GAME_USE_IMGUI
        cleanupImGui();
#endif
    }

    void onStart() override {
        m_inputHandler = std::make_unique<TInputHandler>();
        setInputHandler(m_inputHandler.get());

        auto* scene = new TScene();
        m_scenePtr = scene;
        addScene("main", scene);
        setActiveScene("main");

#ifdef VDE_GAME_USE_IMGUI
        initImGui();
#endif
    }

    void onRender() override {
#ifdef VDE_GAME_USE_IMGUI
        renderImGui();
#endif
    }

    void onShutdown() override {
#ifdef VDE_GAME_USE_IMGUI
        if (m_scenePtr) {
            m_scenePtr->onBeforeImGuiShutdown();
        }
        if (getVulkanContext()) {
            vkDeviceWaitIdle(getVulkanContext()->getDevice());
        }
        cleanupImGui();
#endif
    }

  protected:
    TInputHandler* getGameInputHandler() { return m_inputHandler.get(); }
    TScene* getGameScene() { return m_scenePtr; }

  private:
    std::unique_ptr<TInputHandler> m_inputHandler;
    TScene* m_scenePtr = nullptr;

#ifdef VDE_GAME_USE_IMGUI
    VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;
    std::unique_ptr<vde::EmojiFont> m_imguiEmojiFont;

    static VkDescriptorPool createImGuiDescriptorPool(VkDevice device) {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ImGui descriptor pool!");
        }
        return pool;
    }

    void initImGui() {
        auto* ctx = getVulkanContext();
        auto* win = getWindow();
        if (!ctx || !win) {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        float dpiScale = getDPIScale();
        if (dpiScale > 0.0f) {
            io.FontGlobalScale = dpiScale;
        }

        ImGui_ImplGlfw_InitForVulkan(win->getHandle(), true);

        m_imguiPool = createImGuiDescriptorPool(ctx->getDevice());

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = ctx->getInstance();
        initInfo.PhysicalDevice = ctx->getPhysicalDevice();
        initInfo.Device = ctx->getDevice();
        initInfo.QueueFamily = ctx->getGraphicsQueueFamily();
        initInfo.Queue = ctx->getGraphicsQueue();
        initInfo.DescriptorPool = m_imguiPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = 2;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.RenderPass = ctx->getRenderPass();
        initInfo.Subpass = 0;

        ImGui_ImplVulkan_Init(&initInfo);
        injectImGuiEmoji(io);
        ImGui_ImplVulkan_CreateFontsTexture();

        m_imguiInitialized = true;
    }

    void injectImGuiEmoji(ImGuiIO& io) {
        std::string emojiPath = vde::EmojiFont::findSystemEmojiFont();
        if (emojiPath.empty()) {
            return;
        }

        m_imguiEmojiFont = std::make_unique<vde::EmojiFont>();
        if (!m_imguiEmojiFont->loadFromFile(nullptr, emojiPath, 16)) {
            m_imguiEmojiFont.reset();
            return;
        }

        if (io.Fonts->Fonts.empty()) {
            io.Fonts->AddFontDefault();
        }
        ImFont* defaultFont = io.Fonts->Fonts[0];

        const auto& codepoints = m_imguiEmojiFont->getAvailableCodepoints();
        struct EmojiRect {
            char32_t cp;
            int rectIdx;
        };

        constexpr std::size_t kMaxEmojiGlyphs = 4096;
        std::vector<EmojiRect> emojiRects;
        emojiRects.reserve(std::min<std::size_t>(codepoints.size(), kMaxEmojiGlyphs));

        for (char32_t cp : codepoints) {
            if (emojiRects.size() >= kMaxEmojiGlyphs) {
                break;
            }

            const auto* glyph = m_imguiEmojiFont->getGlyph(cp);
            if (!glyph) {
                continue;
            }

            int rectIdx = io.Fonts->AddCustomRectFontGlyph(defaultFont, static_cast<ImWchar>(cp),
                                                           glyph->width, glyph->height,
                                                           glyph->advanceX, ImVec2(0, 0));
            if (rectIdx >= 0) {
                emojiRects.push_back({cp, rectIdx});
            }
        }

        if (emojiRects.empty()) {
            m_imguiEmojiFont.reset();
            return;
        }

        io.Fonts->Build();

        unsigned char* texPixels = nullptr;
        int texW = 0;
        int texH = 0;
        io.Fonts->GetTexDataAsRGBA32(&texPixels, &texW, &texH);
        if (!texPixels) {
            m_imguiEmojiFont.reset();
            return;
        }

        std::vector<uint8_t> glyphPixels;
        for (const auto& rectInfo : emojiRects) {
            const ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(rectInfo.rectIdx);
            if (!rect || !rect->IsPacked()) {
                continue;
            }

            const auto* glyph = m_imguiEmojiFont->getGlyph(rectInfo.cp);
            if (!glyph) {
                continue;
            }

            glyphPixels.resize(static_cast<size_t>(glyph->width) * glyph->height * 4);
            if (!m_imguiEmojiFont->copyGlyphPixels(rectInfo.cp, glyphPixels.data())) {
                continue;
            }

            for (int y = 0; y < glyph->height && (rect->Y + y) < texH; ++y) {
                unsigned char* dst =
                    texPixels + (static_cast<size_t>(rect->Y + y) * texW + rect->X) * 4;
                const uint8_t* src = glyphPixels.data() + static_cast<size_t>(y) * glyph->width * 4;
                int copyW = std::min(glyph->width, texW - static_cast<int>(rect->X));
                std::memcpy(dst, src, static_cast<size_t>(copyW) * 4);
            }
        }
    }

    void renderImGui() {
        if (!m_imguiInitialized || !m_scenePtr || !m_scenePtr->isDebugUIVisible()) {
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        m_scenePtr->drawDebugUI();

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        auto* ctx = getVulkanContext();
        if (ctx && drawData) {
            VkCommandBuffer cmd = ctx->getCurrentCommandBuffer();
            if (cmd != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
            }
        }
    }

    void cleanupImGui() {
        if (!m_imguiInitialized) {
            return;
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_imguiPool != VK_NULL_HANDLE) {
            auto* ctx = getVulkanContext();
            if (ctx && ctx->getDevice()) {
                vkDestroyDescriptorPool(ctx->getDevice(), m_imguiPool, nullptr);
            }
            m_imguiPool = VK_NULL_HANDLE;
        }

        m_imguiInitialized = false;
    }
#endif
};

template <typename TGame>
int runGame(TGame& game, const std::string& gameName, uint32_t width = 1280, uint32_t height = 720,
            int argc = 0, char** argv = nullptr) {
    if (argc > 0 && argv != nullptr) {
        vde::configureInputScriptFromArgs(game, argc, argv);
    }

    setWorkingDirectoryToExecutablePath();

    vde::GameSettings settings;
    settings.gameName = gameName;
    settings.display.windowWidth = width;
    settings.display.windowHeight = height;
    settings.display.fullscreen = false;

    try {
        if (!game.initialize(settings)) {
            std::cerr << "Failed to initialize " << gameName << "!" << std::endl;
            return 1;
        }

        game.run();
        return game.getExitCode();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

}  // namespace games
}  // namespace vde