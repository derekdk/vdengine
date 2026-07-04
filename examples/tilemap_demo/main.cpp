#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "../ExampleBase.h"

namespace {

constexpr float kViewWidth = 22.0f;
constexpr float kViewHeight = 12.0f;
constexpr float kTileSize = 1.0f;
constexpr float kPlayerWidth = 0.78f;
constexpr float kPlayerHeight = 1.42f;
constexpr float kPlayerHalfWidth = kPlayerWidth * 0.5f;
constexpr float kPlayerHalfHeight = kPlayerHeight * 0.5f;
constexpr float kMoveSpeed = 7.5f;
constexpr float kJumpVelocity = 11.5f;
constexpr float kGravity = -28.0f;
constexpr float kMaxFallVelocity = -22.0f;
constexpr float kCollisionEpsilon = 0.001f;
constexpr glm::vec2 kDefaultSpawnPoint(4.5f, 7.0f);
constexpr const char* kImportedMapPath = "assets/tiled/tilemap_demo.tmj";

struct RGBA {
    constexpr RGBA() = default;
    constexpr RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct AABB {
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
};

void putPixel(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t x, uint32_t y, RGBA color) {
    const size_t offset = (static_cast<size_t>(y) * stride + x) * 4;
    buffer.at(offset + 0) = color.r;
    buffer.at(offset + 1) = color.g;
    buffer.at(offset + 2) = color.b;
    buffer.at(offset + 3) = color.a;
}

std::shared_ptr<vde::Texture> createBackdropTexture(uint32_t width, uint32_t height, RGBA base,
                                                    RGBA stripe, RGBA highlight, bool diagonal) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            RGBA color = base;
            if (((x / 8) + (diagonal ? (y / 6) : 0)) % 3 == 0) {
                color = stripe;
            }
            if ((y < (height / 4)) && ((x + y) % 17 < 5)) {
                color = highlight;
            }
            putPixel(pixels, width, x, y, color);
        }
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), width, height);
    return texture;
}
AABB makePlayerBounds(const glm::vec2& center) {
    return AABB{.left = center.x - kPlayerHalfWidth,
                .right = center.x + kPlayerHalfWidth,
                .bottom = center.y - kPlayerHalfHeight,
                .top = center.y + kPlayerHalfHeight};
}

AABB makeRectBounds(const vde::TileCollisionRect& rect) {
    return AABB{.left = rect.center.x - rect.halfExtents.x,
                .right = rect.center.x + rect.halfExtents.x,
                .bottom = rect.center.y - rect.halfExtents.y,
                .top = rect.center.y + rect.halfExtents.y};
}

bool intersects(const AABB& a, const AABB& b) {
    return a.left < b.right && a.right > b.left && a.bottom < b.top && a.top > b.bottom;
}

bool overlapsHorizontally(const AABB& a, const AABB& b) {
    return a.left < b.right && a.right > b.left;
}

glm::vec2 findSpawnPoint(const std::vector<vde::ImportedTileObject>& objects) {
    for (const auto& object : objects) {
        if (object.name == "spawn" || object.type == "spawn") {
            return object.position;
        }
    }

    return kDefaultSpawnPoint;
}

}  // namespace

class TileMapInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    TileMapInputHandler() {
        keys.bindHeld(vde::KEY_A, "left");
        keys.bindHeld(vde::KEY_LEFT, "left");
        keys.bindHeld(vde::KEY_D, "right");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindOneShot(vde::KEY_SPACE, "jump");
        keys.bindOneShot(vde::KEY_W, "jump");
        keys.bindOneShot(vde::KEY_UP, "jump");
        keys.bindOneShot(vde::KEY_R, "reset");
    }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

class TileMapScene : public vde::examples::BaseExampleScene {
  public:
    TileMapScene() : BaseExampleScene(18.0f) {}

    void onEnter() override {
        printExampleHeader();

        setup2D(kViewWidth, kViewHeight, vde::Color::fromHex(0x08121f));
        m_camera2D = dynamic_cast<vde::Camera2D*>(getCamera());
        if (m_camera2D != nullptr) {
            m_camera2D->setDeadzone(4.0f, 2.6f);
            m_camera2D->setLookAhead(2.1f, 0.18f, 7.5f);
            m_camera2D->setZoom(1.08f);
        }

        createBackgrounds();
        createTileMap();
        rebuildCollisionCache();
        createPlayer();
        resetDemo();

        std::cout << "Tiled import demo: " << m_tileMap->getColumnCount() << 'x'
                  << m_tileMap->getRowCount() << " imported tiles across "
                  << m_tileMap->getLayerCount() << " layers, " << m_importedObjectCount
                  << " imported objects, extracted " << m_solidRects.size() << " solid regions and "
                  << m_oneWayRects.size() << " one-way regions\n";
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<TileMapInputHandler*>(getInputHandler());
        if (input == nullptr || m_camera2D == nullptr || m_player == nullptr) {
            return;
        }

        if (input->keys.consume("reset")) {
            resetDemo();
            return;
        }

        float moveAxis = 0.0f;
        if (input->keys.isHeld("left")) {
            moveAxis -= 1.0f;
        }
        if (input->keys.isHeld("right")) {
            moveAxis += 1.0f;
        }

        if (input->keys.consume("jump") && m_onGround) {
            m_playerVelocity.y = kJumpVelocity;
            m_onGround = false;
        }

        m_playerVelocity.x = moveAxis * kMoveSpeed;
        m_playerVelocity.y =
            std::max(m_playerVelocity.y + (kGravity * deltaTime), kMaxFallVelocity);

        m_playerPosition.x += m_playerVelocity.x * deltaTime;
        resolveHorizontalCollisions();

        const float previousBottom = m_playerPosition.y - kPlayerHalfHeight;
        m_playerPosition.y += m_playerVelocity.y * deltaTime;
        m_onGround = false;
        resolveVerticalCollisions(previousBottom);

        const float maxX = static_cast<float>(m_tileMap->getColumnCount()) - kPlayerHalfWidth;
        m_playerPosition.x = std::clamp(m_playerPosition.x, kPlayerHalfWidth, maxX);

        if (m_playerPosition.y < -5.0f) {
            resetDemo();
            return;
        }

        if (moveAxis < 0.0f) {
            m_player->setFlipX(true);
        } else if (moveAxis > 0.0f) {
            m_player->setFlipX(false);
        }

        m_camera2D->followTarget(m_playerPosition + glm::vec2(0.0f, 1.1f), 7.5f);
        syncVisuals();
    }

  protected:
    [[nodiscard]] std::string getExampleName() const override { return "Tiled Import Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "Checked-in Tiled JSON sample imported into TileMap layers and SpriteSheet tiles",
            "Collision extraction from imported solid terrain and one-way platform spans",
            "Object-layer spawn metadata plus repeating parallax backgrounds in a playable scene",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "A long imported side-view level with grass, dirt, stone, and amber one-way platforms",
            "Two repeating background layers scrolling behind the map at different parallax speeds",
            "A bright player marker that can run, jump through one-way platforms from below, and "
            "land on them from above",
        };
    }

    [[nodiscard]] std::vector<std::string> getControls() const override {
        return {
            "A / D or Left / Right - Move across the tilemap",
            "Space / W / Up - Jump",
            "R - Reset to the spawn point",
        };
    }

  private:
    void createBackgrounds() {
        auto skyTexture = createBackdropTexture(
            64, 64, RGBA{10, 22, 38, 255}, RGBA{18, 38, 62, 255}, RGBA{44, 74, 104, 255}, false);
        auto ridgeTexture = createBackdropTexture(
            64, 48, RGBA{18, 31, 46, 255}, RGBA{28, 52, 72, 255}, RGBA{54, 92, 118, 255}, true);

        auto sky = addEntity<vde::RepeatingBackground>(skyTexture, 8.0f, 8.0f, 8, 3);
        sky->setPosition(0.0f, 0.0f, -3.0f);
        sky->setParallaxFactor(0.18f, 0.06f);
        sky->setScrollVelocity(0.22f, 0.0f);

        auto ridges = addEntity<vde::RepeatingBackground>(ridgeTexture, 6.0f, 4.0f, 10, 4);
        ridges->setPosition(0.0f, -1.5f, -2.2f);
        ridges->setParallaxFactor(0.42f, 0.12f);
        ridges->setScrollVelocity(0.48f, 0.0f);
    }

    void createTileMap() {
        vde::TileMapImportOptions options;
        options.tileWidth = kTileSize;
        options.tileHeight = kTileSize;
        options.layerDepthStep = 0.06f;

        auto imported = vde::TileMapImport::importTiledJsonFile(
            getGame() ? getGame()->getVulkanContext() : nullptr, kImportedMapPath, options);
        m_tileMap = imported.tileMap;
        m_importedObjectCount = imported.objects.size();
        m_spawnPoint = findSpawnPoint(imported.objects);

        addEntity(std::static_pointer_cast<vde::Entity>(m_tileMap));
        m_tileMap->setPosition(0.0f, 0.0f, -0.4f);
    }

    void rebuildCollisionCache() {
        const std::vector<vde::TileCollisionRect> collisions = m_tileMap->extractCollisionRects();
        m_solidRects.clear();
        m_oneWayRects.clear();
        for (const auto& rect : collisions) {
            if (rect.kind == vde::TileCollisionKind::Solid) {
                m_solidRects.push_back(rect);
            } else if (rect.kind == vde::TileCollisionKind::OneWay) {
                m_oneWayRects.push_back(rect);
            }
        }
    }

    void createPlayer() {
        m_player = addEntity<vde::SpriteEntity>();
        m_player->setScale(kPlayerWidth, kPlayerHeight, 1.0f);
        m_player->setAnchor(0.5f, 0.5f);
        m_player->setColor(vde::Color::fromHex(0xfff1a8));

        m_playerShadow = addEntity<vde::SpriteEntity>();
        m_playerShadow->setScale(0.95f, 0.22f, 1.0f);
        m_playerShadow->setColor(vde::Color(0.05f, 0.08f, 0.12f, 0.45f));
    }

    void resetDemo() {
        m_playerPosition = m_spawnPoint;
        m_playerVelocity = glm::vec2(0.0f);
        m_onGround = false;

        if (m_camera2D != nullptr) {
            m_camera2D->setPosition(m_spawnPoint.x + 3.0f, m_spawnPoint.y + 1.5f);
        }

        syncVisuals();
    }

    void resolveHorizontalCollisions() {
        if (std::abs(m_playerVelocity.x) < 0.0001f) {
            return;
        }

        AABB playerBounds = makePlayerBounds(m_playerPosition);
        for (const auto& rect : m_solidRects) {
            const AABB solidBounds = makeRectBounds(rect);
            if (!intersects(playerBounds, solidBounds)) {
                continue;
            }

            if (m_playerVelocity.x > 0.0f) {
                m_playerPosition.x = solidBounds.left - kPlayerHalfWidth - kCollisionEpsilon;
            } else {
                m_playerPosition.x = solidBounds.right + kPlayerHalfWidth + kCollisionEpsilon;
            }

            m_playerVelocity.x = 0.0f;
            playerBounds = makePlayerBounds(m_playerPosition);
        }
    }

    void resolveVerticalCollisions(float previousBottom) {
        AABB playerBounds = makePlayerBounds(m_playerPosition);

        for (const auto& rect : m_solidRects) {
            const AABB solidBounds = makeRectBounds(rect);
            if (!intersects(playerBounds, solidBounds)) {
                continue;
            }

            if (m_playerVelocity.y > 0.0f) {
                m_playerPosition.y = solidBounds.bottom - kPlayerHalfHeight - kCollisionEpsilon;
            } else {
                m_playerPosition.y = solidBounds.top + kPlayerHalfHeight + kCollisionEpsilon;
                m_onGround = true;
            }

            m_playerVelocity.y = 0.0f;
            playerBounds = makePlayerBounds(m_playerPosition);
        }

        if (m_playerVelocity.y > 0.0f) {
            return;
        }

        playerBounds = makePlayerBounds(m_playerPosition);
        for (const auto& rect : m_oneWayRects) {
            const AABB platformBounds = makeRectBounds(rect);
            if (!overlapsHorizontally(playerBounds, platformBounds)) {
                continue;
            }

            if (previousBottom >= (platformBounds.top - 0.02f) &&
                playerBounds.bottom <= platformBounds.top &&
                playerBounds.top > platformBounds.top) {
                m_playerPosition.y = platformBounds.top + kPlayerHalfHeight + kCollisionEpsilon;
                m_playerVelocity.y = 0.0f;
                m_onGround = true;
                break;
            }
        }
    }

    void syncVisuals() {
        if (m_player == nullptr || m_playerShadow == nullptr) {
            return;
        }

        const float speedRatio =
            vde::math2d::saturate(std::abs(m_playerVelocity.x) / (kMoveSpeed + 0.001f));
        m_player->setPosition(m_playerPosition.x, m_playerPosition.y, 0.75f);
        m_player->setScale(kPlayerWidth + (speedRatio * 0.08f),
                           kPlayerHeight - (speedRatio * 0.06f), 1.0f);
        m_player->setColor(m_onGround ? vde::Color::fromHex(0xfff1a8)
                                      : vde::Color::fromHex(0xffc36b));

        const float shadowY = std::max(0.12f, m_playerPosition.y - kPlayerHalfHeight);
        m_playerShadow->setPosition(m_playerPosition.x, shadowY, 0.68f);
        m_playerShadow->setScale(0.84f - (speedRatio * 0.08f), 0.18f, 1.0f);
    }

    vde::Camera2D* m_camera2D = nullptr;
    std::shared_ptr<vde::TileMap> m_tileMap;
    std::shared_ptr<vde::SpriteEntity> m_player;
    std::shared_ptr<vde::SpriteEntity> m_playerShadow;
    std::vector<vde::TileCollisionRect> m_solidRects;
    std::vector<vde::TileCollisionRect> m_oneWayRects;
    size_t m_importedObjectCount = 0;
    glm::vec2 m_spawnPoint{kDefaultSpawnPoint};
    glm::vec2 m_playerPosition{0.0f};
    glm::vec2 m_playerVelocity{0.0f};
    bool m_onGround = false;
};

class TileMapDemoGame : public vde::examples::BaseExampleGame<TileMapInputHandler, TileMapScene> {};

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    TileMapDemoGame game;
    return vde::examples::runExample(game, "VDE TileMap Demo", 1280, 720, argc, argv);
}