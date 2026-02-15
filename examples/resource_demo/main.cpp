/**
 * @file main.cpp
 * @brief Resource Management example demonstrating VDE Phase 5 features.
 *
 * This example demonstrates:
 * - ResourceManager with automatic caching
 * - Texture two-phase loading (CPU -> GPU)
 * - Resource deduplication (same path = same instance)
 * - Automatic memory management with weak_ptr
 * - Resource statistics and monitoring
 * - Sharing resources across multiple entities
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <iostream>
#include <memory>
#include <sstream>

#include "../ExampleBase.h"

/**
 * @brief Input handler for the resource demo.
 */
class ResourceInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Call base class first for ESC and F keys
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE)
            m_spacePressed = true;
        if (key == vde::KEY_R)
            m_reload = true;
        if (key == vde::KEY_C)
            m_clear = true;
    }

    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

    bool isReloadPressed() {
        bool val = m_reload;
        m_reload = false;
        return val;
    }

    bool isClearPressed() {
        bool val = m_clear;
        m_clear = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_reload = false;
    bool m_clear = false;
};

/**
 * @brief Text sprite that displays resource statistics.
 */
class StatDisplay : public vde::Entity {
  public:
    StatDisplay(vde::ResourceManager* manager) : m_resourceManager(manager) {}

    void update(float deltaTime) override {
        m_updateTimer += deltaTime;
        if (m_updateTimer >= 0.5f) {  // Update every 0.5 seconds
            updateStats();
            m_updateTimer = 0.0f;
        }
    }

    const std::string& getStatsText() const { return m_statsText; }

  private:
    void updateStats() {
        std::ostringstream oss;
        oss << "=== Resource Manager Stats ===" << std::endl;
        oss << "Cached Resources: " << m_resourceManager->getCachedCount() << std::endl;
        oss << "Memory Usage: " << formatBytes(m_resourceManager->getMemoryUsage()) << std::endl;
        m_statsText = oss.str();
    }

    std::string formatBytes(size_t bytes) const {
        if (bytes < 1024)
            return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024)
            return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }

    vde::ResourceManager* m_resourceManager;
    float m_updateTimer = 0.0f;
    std::string m_statsText;
};

/**
 * @brief Main scene demonstrating resource management.
 */
class ResourceDemoScene : public vde::examples::BaseExampleScene {
  public:
    ResourceDemoScene() : BaseExampleScene(20.0f) {}  // 20 second timeout

    void onEnter() override {
        // Print standard header
        printExampleHeader();

        // Get resource manager from game
        auto* game = getGame();
        if (!game) {
            std::cerr << "ERROR: No game instance!" << std::endl;
            return;
        }

        m_resourceManager = &game->getResourceManager();

        // Create procedural textures for demonstration
        // (In a real app, you'd load from files)
        createDemoTextures();

        // Create stat display
        m_statDisplay = std::make_shared<StatDisplay>(m_resourceManager);
        addEntity(m_statDisplay);

        // Create sprites demonstrating resource sharing
        createSpriteGrid();

        // Set up 2D camera
        auto camera = std::make_unique<vde::Camera2D>();
        camera->setPosition(0.0f, 0.0f);
        setCamera(std::move(camera));

        std::cout << "\n=== Resource Loading Demo ===" << std::endl;
        std::cout << "Loading 'red_texture' for the first time..." << std::endl;
        demonstrateResourceCaching();
    }

    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<ResourceInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Space: Toggle animation
        if (input->isSpacePressed()) {
            m_animating = !m_animating;
            std::cout << "Animation: " << (m_animating ? "ON" : "OFF") << std::endl;
        }

        // R: Reload demonstration
        if (input->isReloadPressed()) {
            std::cout << "\n=== Re-loading Resources ===" << std::endl;
            demonstrateResourceCaching();
        }

        // C: Clear cache and recreate
        if (input->isClearPressed()) {
            std::cout << "\n=== Clearing Cache ===" << std::endl;
            clearAndRecreate();
        }

        // Animate sprites if enabled
        if (m_animating) {
            animateSprites(deltaTime);
        }

        // Update stats display
        if (m_statDisplay) {
            m_statDisplay->update(deltaTime);
        }

        // Print stats to console periodically
        m_statTimer += deltaTime;
        if (m_statTimer >= 2.0f) {
            printResourceStats();
            m_statTimer = 0.0f;
        }
    }

  protected:
    std::string getExampleName() const override { return "Resource Management"; }

    std::vector<std::string> getFeatures() const override {
        return {"ResourceManager with automatic caching", "Texture two-phase loading (CPU → GPU)",
                "Automatic resource deduplication",       "Weak pointer memory management",
                "Resource statistics and monitoring",     "Shared textures across entities"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"4x3 grid of colored squares (sprites)",
                "Different colors: Red, Green, Blue, Yellow",
                "Same colors share the same texture instance",
                "Resource stats printed to console every 2 seconds"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Toggle sprite animation", "R - Reload resources (demonstrates caching)",
                "C - Clear cache and recreate", "ESC - Exit early", "F - Report test failure"};
    }

  private:
    void createDemoTextures() {
        // Create simple colored textures in memory
        // These would normally be loaded from files

        // Red texture (16x16 pixels)
        m_redTexture = std::make_shared<vde::Texture>();
        std::vector<uint8_t> redPixels(16 * 16 * 4, 255);
        for (size_t i = 0; i < 16 * 16; ++i) {
            redPixels[i * 4 + 0] = 255;  // R
            redPixels[i * 4 + 1] = 0;    // G
            redPixels[i * 4 + 2] = 0;    // B
            redPixels[i * 4 + 3] = 255;  // A
        }
        m_redTexture->loadFromData(redPixels.data(), 16, 16);

        // Green texture
        m_greenTexture = std::make_shared<vde::Texture>();
        std::vector<uint8_t> greenPixels(16 * 16 * 4, 255);
        for (size_t i = 0; i < 16 * 16; ++i) {
            greenPixels[i * 4 + 0] = 0;    // R
            greenPixels[i * 4 + 1] = 255;  // G
            greenPixels[i * 4 + 2] = 0;    // B
            greenPixels[i * 4 + 3] = 255;  // A
        }
        m_greenTexture->loadFromData(greenPixels.data(), 16, 16);

        // Blue texture
        m_blueTexture = std::make_shared<vde::Texture>();
        std::vector<uint8_t> bluePixels(16 * 16 * 4, 255);
        for (size_t i = 0; i < 16 * 16; ++i) {
            bluePixels[i * 4 + 0] = 0;    // R
            bluePixels[i * 4 + 1] = 0;    // G
            bluePixels[i * 4 + 2] = 255;  // B
            bluePixels[i * 4 + 3] = 255;  // A
        }
        m_blueTexture->loadFromData(bluePixels.data(), 16, 16);

        // Yellow texture
        m_yellowTexture = std::make_shared<vde::Texture>();
        std::vector<uint8_t> yellowPixels(16 * 16 * 4, 255);
        for (size_t i = 0; i < 16 * 16; ++i) {
            yellowPixels[i * 4 + 0] = 255;  // R
            yellowPixels[i * 4 + 1] = 255;  // G
            yellowPixels[i * 4 + 2] = 0;    // B
            yellowPixels[i * 4 + 3] = 255;  // A
        }
        m_yellowTexture->loadFromData(yellowPixels.data(), 16, 16);

        // Add to resource manager
        m_resourceManager->add<vde::Texture>("red_texture", m_redTexture);
        m_resourceManager->add<vde::Texture>("green_texture", m_greenTexture);
        m_resourceManager->add<vde::Texture>("blue_texture", m_blueTexture);
        m_resourceManager->add<vde::Texture>("yellow_texture", m_yellowTexture);

        // Upload to GPU
        auto* game = getGame();
        auto* context = game->getVulkanContext();
        m_redTexture->uploadToGPU(context);
        m_greenTexture->uploadToGPU(context);
        m_blueTexture->uploadToGPU(context);
        m_yellowTexture->uploadToGPU(context);

        std::cout << "Created 4 demo textures (16x16 each)" << std::endl;
        std::cout << "Added to ResourceManager cache" << std::endl;
    }

    void createSpriteGrid() {
        // Create a 4x3 grid of sprites
        // Demonstrates that multiple sprites can share the same texture
        const float spacing = 2.5f;
        const float startX = -4.5f;
        const float startY = 3.0f;

        std::vector<vde::ResourcePtr<vde::Texture>> textures = {m_redTexture, m_greenTexture,
                                                                m_blueTexture, m_yellowTexture};

        int textureIndex = 0;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 4; ++col) {
                auto sprite = addEntity<vde::SpriteEntity>();
                float x = startX + col * spacing;
                float y = startY - row * spacing;
                sprite->setPosition(x, y, 0.0f);
                sprite->setScale(2.0f);

                // Use textures in a pattern to show sharing
                // Each texture is used by 3 different sprites
                sprite->setTexture(textures[textureIndex % 4]);
                textureIndex++;

                m_sprites.push_back(sprite.get());
            }
        }

        std::cout << "Created " << m_sprites.size() << " sprites in a 4x3 grid" << std::endl;
        std::cout << "Each texture is shared by 3 sprites" << std::endl;
    }

    void demonstrateResourceCaching() {
        // Demonstrate that loading the same resource returns the same instance
        auto red1 = m_resourceManager->get<vde::Texture>("red_texture");
        auto red2 = m_resourceManager->get<vde::Texture>("red_texture");

        bool same = (red1 == red2);
        std::cout << "First load:  " << red1.get() << std::endl;
        std::cout << "Second load: " << red2.get() << std::endl;
        std::cout << "Same instance? " << (same ? "YES ✓" : "NO ✗") << std::endl;

        if (!same) {
            std::cout << "ERROR: Resource deduplication failed!" << std::endl;
        }
    }

    void clearAndRecreate() {
        // Clear the resource manager cache
        size_t countBefore = m_resourceManager->getCachedCount();

        // Keep references so textures don't get destroyed
        std::vector<vde::ResourcePtr<vde::Texture>> tempRefs = {m_redTexture, m_greenTexture,
                                                                m_blueTexture, m_yellowTexture};

        m_resourceManager->clear();
        size_t countAfter = m_resourceManager->getCachedCount();

        std::cout << "Cached before clear: " << countBefore << std::endl;
        std::cout << "Cached after clear:  " << countAfter << std::endl;

        // Re-add textures
        m_resourceManager->add<vde::Texture>("red_texture", m_redTexture);
        m_resourceManager->add<vde::Texture>("green_texture", m_greenTexture);
        m_resourceManager->add<vde::Texture>("blue_texture", m_blueTexture);
        m_resourceManager->add<vde::Texture>("yellow_texture", m_yellowTexture);

        std::cout << "Re-added textures to cache" << std::endl;
        std::cout << "Cached now: " << m_resourceManager->getCachedCount() << std::endl;
    }

    void animateSprites(float deltaTime) {
        m_animTimer += deltaTime;

        for (size_t i = 0; i < m_sprites.size(); ++i) {
            auto* sprite = m_sprites[i];
            if (!sprite)
                continue;

            // Gentle rotation
            float phase = static_cast<float>(i) * 0.3f;
            auto rot = sprite->getRotation();
            rot.roll = std::sin(m_animTimer * 2.0f + phase) * 15.0f;
            sprite->setRotation(rot);

            // Gentle scale pulse
            float scale = 2.0f + std::sin(m_animTimer * 3.0f + phase) * 0.2f;
            sprite->setScale(scale);
        }
    }

    void printResourceStats() {
        if (!m_statDisplay)
            return;

        std::cout << "\n" << m_statDisplay->getStatsText();

        // Also print which textures are cached
        std::cout << "Textures in cache:" << std::endl;
        std::cout << "  red_texture:    " << (m_resourceManager->has("red_texture") ? "✓" : "✗")
                  << std::endl;
        std::cout << "  green_texture:  " << (m_resourceManager->has("green_texture") ? "✓" : "✗")
                  << std::endl;
        std::cout << "  blue_texture:   " << (m_resourceManager->has("blue_texture") ? "✓" : "✗")
                  << std::endl;
        std::cout << "  yellow_texture: " << (m_resourceManager->has("yellow_texture") ? "✓" : "✗")
                  << std::endl;
    }

    // Resource manager reference
    vde::ResourceManager* m_resourceManager = nullptr;

    // Textures (kept alive by shared_ptr)
    vde::ResourcePtr<vde::Texture> m_redTexture;
    vde::ResourcePtr<vde::Texture> m_greenTexture;
    vde::ResourcePtr<vde::Texture> m_blueTexture;
    vde::ResourcePtr<vde::Texture> m_yellowTexture;

    // Sprites
    std::vector<vde::SpriteEntity*> m_sprites;

    // Animation
    bool m_animating = false;
    float m_animTimer = 0.0f;

    // Stats display
    std::shared_ptr<StatDisplay> m_statDisplay;
    float m_statTimer = 0.0f;
};

/**
 * @brief Game class for the resource demo.
 */
class ResourceDemoGame
    : public vde::examples::BaseExampleGame<ResourceInputHandler, ResourceDemoScene> {
  public:
    ResourceDemoGame() = default;
};

/**
 * @brief Main entry point.
 */
int main(int argc, char** argv) {
    ResourceDemoGame demo;
    return vde::examples::runExample(demo, "VDE Resource Management Demo", 1280, 720, argc, argv);
}
