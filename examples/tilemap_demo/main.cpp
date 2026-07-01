#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "../ExampleBase.h"

namespace {

constexpr float kViewWidth = 22.0f;
constexpr float kViewHeight = 12.0f;
constexpr int kMapColumns = 128;
constexpr int kMapRows = 24;
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
constexpr glm::vec2 kSpawnPoint(4.5f, 7.0f);
constexpr int kGrassTileId = 0;
constexpr int kDirtTileId = 1;
constexpr int kStoneTileId = 2;
constexpr int kOneWayTileId = 3;
constexpr int kAccentTileId = 4;

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

void fillRect(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t x0, uint32_t y0,
              uint32_t width, uint32_t height, RGBA color) {
    for (uint32_t y = y0; y < y0 + height; ++y) {
        for (uint32_t x = x0; x < x0 + width; ++x) {
            putPixel(buffer, stride, x, y, color);
        }
    }
}

void drawTileBorder(std::vector<uint8_t>& buffer, uint32_t stride, uint32_t originX,
                    uint32_t originY, RGBA color) {
    fillRect(buffer, stride, originX, originY, 16, 1, color);
    fillRect(buffer, stride, originX, originY + 15, 16, 1, color);
    fillRect(buffer, stride, originX, originY, 1, 16, color);
    fillRect(buffer, stride, originX + 15, originY, 1, 16, color);
}

std::shared_ptr<vde::SpriteSheet> createTileSet(vde::VulkanContext* context) {
    constexpr uint32_t kTilePixels = 16;
    constexpr uint32_t kColumns = 5;
    constexpr uint32_t kRows = 1;
    constexpr uint32_t kTextureWidth = kTilePixels * kColumns;
    constexpr uint32_t kTextureHeight = kTilePixels * kRows;

    constexpr RGBA kGrassTop{92, 196, 92, 255};
    constexpr RGBA kGrassBody{57, 138, 57, 255};
    constexpr RGBA kDirtBase{123, 90, 56, 255};
    constexpr RGBA kDirtDark{92, 62, 36, 255};
    constexpr RGBA kStoneBase{106, 117, 132, 255};
    constexpr RGBA kStoneDark{74, 84, 98, 255};
    constexpr RGBA kPlatformWood{204, 152, 72, 255};
    constexpr RGBA kPlatformShadow{126, 86, 38, 255};
    constexpr RGBA kAccentBase{246, 220, 108, 255};
    constexpr RGBA kAccentDark{194, 134, 44, 255};
    constexpr RGBA kOutline{38, 48, 61, 255};

    std::vector<uint8_t> pixels(static_cast<size_t>(kTextureWidth) * kTextureHeight * 4, 0);

    for (uint32_t tile = 0; tile < kColumns; ++tile) {
        const uint32_t originX = tile * kTilePixels;
        const uint32_t originY = 0;

        switch (tile) {
        case kGrassTileId:
            fillRect(pixels, kTextureWidth, originX, originY, 16, 16, kGrassBody);
            fillRect(pixels, kTextureWidth, originX, originY, 16, 4, kGrassTop);
            fillRect(pixels, kTextureWidth, originX + 2, originY + 4, 2, 3, kGrassTop);
            fillRect(pixels, kTextureWidth, originX + 7, originY + 5, 2, 2, kGrassTop);
            fillRect(pixels, kTextureWidth, originX + 12, originY + 4, 2, 3, kGrassTop);
            break;
        case kDirtTileId:
            fillRect(pixels, kTextureWidth, originX, originY, 16, 16, kDirtBase);
            fillRect(pixels, kTextureWidth, originX + 2, originY + 3, 3, 3, kDirtDark);
            fillRect(pixels, kTextureWidth, originX + 10, originY + 4, 2, 2, kDirtDark);
            fillRect(pixels, kTextureWidth, originX + 6, originY + 10, 4, 2, kDirtDark);
            break;
        case kStoneTileId:
            fillRect(pixels, kTextureWidth, originX, originY, 16, 16, kStoneBase);
            fillRect(pixels, kTextureWidth, originX + 2, originY + 2, 5, 4, kStoneDark);
            fillRect(pixels, kTextureWidth, originX + 9, originY + 4, 4, 3, kStoneDark);
            fillRect(pixels, kTextureWidth, originX + 5, originY + 9, 6, 4, kStoneDark);
            break;
        case kOneWayTileId:
            fillRect(pixels, kTextureWidth, originX, originY, 16, 16, kPlatformShadow);
            fillRect(pixels, kTextureWidth, originX, originY, 16, 4, kPlatformWood);
            fillRect(pixels, kTextureWidth, originX + 2, originY + 5, 2, 6, kPlatformWood);
            fillRect(pixels, kTextureWidth, originX + 12, originY + 5, 2, 6, kPlatformWood);
            break;
        case kAccentTileId:
            fillRect(pixels, kTextureWidth, originX, originY, 16, 16, kAccentDark);
            fillRect(pixels, kTextureWidth, originX + 3, originY + 3, 10, 10, kAccentBase);
            fillRect(pixels, kTextureWidth, originX + 6, originY + 0, 4, 16, kAccentDark);
            fillRect(pixels, kTextureWidth, originX + 0, originY + 6, 16, 4, kAccentDark);
            break;
        default:
            break;
        }

        drawTileBorder(pixels, kTextureWidth, originX, originY, kOutline);
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), kTextureWidth, kTextureHeight);
    if (context != nullptr) {
        texture->uploadToGPU(context);
    }

    return vde::SpriteSheet::createGrid(texture, static_cast<int>(kColumns),
                                        static_cast<int>(kRows));
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

int surfaceRowForColumn(int column) {
    static constexpr std::array<int, 8> kSegments = {3, 4, 5, 3, 6, 4, 7, 5};
    const int base = kSegments.at(static_cast<size_t>(column / 16));
    const int wobble = ((column % 12) >= 8) ? 1 : 0;
    return std::clamp(base + wobble, 3, 8);
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

        std::cout << "TileMap demo: " << kMapColumns << 'x' << kMapRows << " tiles across "
                  << m_tileMap->getLayerCount() << " layers, extracted " << m_solidRects.size()
                  << " solid regions and " << m_oneWayRects.size() << " one-way regions\n";
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

        const float maxX = static_cast<float>(kMapColumns) - kPlayerHalfWidth;
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
    [[nodiscard]] std::string getExampleName() const override { return "TileMap Runtime Demo"; }

    [[nodiscard]] std::vector<std::string> getFeatures() const override {
        return {
            "Mesh-backed TileMap rendering with layered SpriteSheet tiles and camera culling",
            "Collision extraction for merged solid terrain and one-way platform spans",
            "RepeatingBackground parallax layers behind a medium-size scrolling level",
        };
    }

    [[nodiscard]] std::vector<std::string> getExpectedVisuals() const override {
        return {
            "A long side-view level with grass, dirt, stone, and amber one-way platforms",
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
        m_tileMap = addEntity<vde::TileMap>(kTileSize, kTileSize, kMapColumns, kMapRows);
        m_tileMap->setPosition(0.0f, 0.0f, -0.4f);
        m_tileMap->setTileSet(createTileSet(getGame() ? getGame()->getVulkanContext() : nullptr));
        m_tileMap->setCollisionKind(kGrassTileId, vde::TileCollisionKind::Solid);
        m_tileMap->setCollisionKind(kDirtTileId, vde::TileCollisionKind::Solid);
        m_tileMap->setCollisionKind(kStoneTileId, vde::TileCollisionKind::Solid);
        m_tileMap->setCollisionKind(kOneWayTileId, vde::TileCollisionKind::OneWay);

        const int decorLayer = m_tileMap->addLayer("accents");
        m_tileMap->setLayerDepth(decorLayer, 0.06f);

        for (int column = 0; column < kMapColumns; ++column) {
            const int surfaceRow = surfaceRowForColumn(column);
            for (int row = 0; row < surfaceRow - 1; ++row) {
                const int tile = (row < 2 || (column % 11) < 4) ? kDirtTileId : kStoneTileId;
                m_tileMap->setTile(column, row, tile);
            }
            m_tileMap->setTile(column, surfaceRow - 1, kGrassTileId);

            if ((column % 9) == 2 && surfaceRow < (kMapRows - 1)) {
                m_tileMap->setTile(decorLayer, column, surfaceRow, kAccentTileId);
            }
        }

        m_tileMap->fillRegion(14, 7, 20, 7, kOneWayTileId);
        m_tileMap->fillRegion(28, 9, 35, 9, kOneWayTileId);
        m_tileMap->fillRegion(49, 11, 57, 11, kOneWayTileId);
        m_tileMap->fillRegion(74, 10, 84, 10, kOneWayTileId);
        m_tileMap->fillRegion(98, 13, 108, 13, kOneWayTileId);

        m_tileMap->fillRegion(24, 4, 27, 6, kStoneTileId);
        m_tileMap->fillRegion(60, 5, 63, 8, kStoneTileId);
        m_tileMap->fillRegion(88, 6, 93, 8, kStoneTileId);
        m_tileMap->fillRegion(112, 4, 117, 7, kStoneTileId);
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
        m_playerPosition = kSpawnPoint;
        m_playerVelocity = glm::vec2(0.0f);
        m_onGround = false;

        if (m_camera2D != nullptr) {
            m_camera2D->setPosition(kSpawnPoint.x + 3.0f, kSpawnPoint.y + 1.5f);
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