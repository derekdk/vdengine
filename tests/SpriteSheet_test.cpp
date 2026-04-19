/**
 * @file SpriteSheet_test.cpp
 * @brief Unit tests for SpriteSheet and SpriteEntity flip functionality.
 */

#include <vde/api/Entity.h>
#include <vde/api/Resource.h>
#include <vde/api/SpriteSheet.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// Helper: create a CPU-only Texture with known dimensions (no GPU needed).
static std::shared_ptr<Texture> makeTestTexture(uint32_t w, uint32_t h) {
    auto tex = std::make_shared<Texture>();
    std::vector<uint8_t> pixels(w * h * 4, 255);  // RGBA white
    tex->loadFromData(pixels.data(), w, h);
    return tex;
}

// ============================================================================
// SpriteSheet — Grid creation
// ============================================================================

class SpriteSheetGridTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tex = makeTestTexture(128, 64);
        // 4 columns, 2 rows → 8 sprites, each 32×32
        sheet = SpriteSheet::createGrid(tex, 4, 2);
    }
    std::shared_ptr<Texture> tex;
    SpriteSheet::Ref sheet;
};

TEST_F(SpriteSheetGridTest, SpriteCountMatchesGrid) {
    EXPECT_EQ(sheet->getSpriteCount(), 8);
}

TEST_F(SpriteSheetGridTest, TextureIsPreserved) {
    EXPECT_EQ(sheet->getTexture(), tex);
}

TEST_F(SpriteSheetGridTest, FirstFrameUV) {
    auto uv = sheet->getUVRect(0);
    EXPECT_FLOAT_EQ(uv.u, 0.0f);
    EXPECT_FLOAT_EQ(uv.v, 0.0f);
    EXPECT_FLOAT_EQ(uv.width, 32.0f / 128.0f);  // 0.25
    EXPECT_FLOAT_EQ(uv.height, 32.0f / 64.0f);  // 0.5
}

TEST_F(SpriteSheetGridTest, MiddleFrameUV) {
    // Frame index 5 → row 1, col 1 (second row, second column)
    auto uv = sheet->getUVRect(5);
    EXPECT_FLOAT_EQ(uv.u, 32.0f / 128.0f);  // col 1
    EXPECT_FLOAT_EQ(uv.v, 32.0f / 64.0f);   // row 1
    EXPECT_FLOAT_EQ(uv.width, 32.0f / 128.0f);
    EXPECT_FLOAT_EQ(uv.height, 32.0f / 64.0f);
}

TEST_F(SpriteSheetGridTest, LastFrameUV) {
    auto uv = sheet->getUVRect(7);
    EXPECT_FLOAT_EQ(uv.u, 96.0f / 128.0f);  // col 3
    EXPECT_FLOAT_EQ(uv.v, 32.0f / 64.0f);   // row 1
    EXPECT_FLOAT_EQ(uv.width, 32.0f / 128.0f);
    EXPECT_FLOAT_EQ(uv.height, 32.0f / 64.0f);
}

TEST_F(SpriteSheetGridTest, OutOfBoundsIndexThrows) {
    EXPECT_THROW(sheet->getUVRect(-1), std::out_of_range);
    EXPECT_THROW(sheet->getUVRect(8), std::out_of_range);
}

// ============================================================================
// SpriteSheet — Resource interface
// ============================================================================

TEST_F(SpriteSheetGridTest, IsAResource) {
    Resource* base = sheet.get();
    EXPECT_NE(base, nullptr);
}

TEST_F(SpriteSheetGridTest, TypeNameIsSpriteSheet) {
    EXPECT_STREQ(sheet->getTypeName(), "SpriteSheet");
}

TEST_F(SpriteSheetGridTest, GridIsLoadedAfterCreation) {
    EXPECT_TRUE(sheet->isLoaded());
}

TEST(SpriteSheetTest, ManualSheetIsLoadedAfterCreation) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_TRUE(sheet->isLoaded());
}

TEST(SpriteSheetTest, DefaultResourceIdIsInvalid) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_EQ(sheet->getId(), INVALID_RESOURCE_ID);
}

// ============================================================================
// SpriteSheet — Grid with spacing
// ============================================================================

TEST(SpriteSheetTest, GridWithSpacing) {
    auto tex = makeTestTexture(68, 34);
    // 2 cols, 2 rows, 4px spacing
    // cell width  = (68 - 4*1) / 2 = 32
    // cell height = (34 - 4*1) / 2 = 15
    auto sheet = SpriteSheet::createGrid(tex, 2, 2, 4);
    ASSERT_EQ(sheet->getSpriteCount(), 4);

    // Frame 0: (0, 0)
    auto uv0 = sheet->getUVRect(0);
    EXPECT_FLOAT_EQ(uv0.u, 0.0f);
    EXPECT_FLOAT_EQ(uv0.v, 0.0f);
    EXPECT_FLOAT_EQ(uv0.width, 32.0f / 68.0f);
    EXPECT_FLOAT_EQ(uv0.height, 15.0f / 34.0f);

    // Frame 1: col 1 → pixel x = 1*(32+4) = 36
    auto uv1 = sheet->getUVRect(1);
    EXPECT_FLOAT_EQ(uv1.u, 36.0f / 68.0f);
    EXPECT_FLOAT_EQ(uv1.v, 0.0f);

    // Frame 2: row 1 → pixel y = 1*(15+4) = 19
    auto uv2 = sheet->getUVRect(2);
    EXPECT_FLOAT_EQ(uv2.u, 0.0f);
    EXPECT_FLOAT_EQ(uv2.v, 19.0f / 34.0f);
}

// ============================================================================
// SpriteSheet — Manual named regions
// ============================================================================

TEST(SpriteSheetTest, ManualNamedSprites) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_EQ(sheet->getSpriteCount(), 0);

    sheet->addSprite("idle", 0, 0, 32, 32);
    sheet->addSprite("run", 32, 0, 32, 32);
    EXPECT_EQ(sheet->getSpriteCount(), 2);

    auto idle = sheet->getUVRect("idle");
    EXPECT_FLOAT_EQ(idle.u, 0.0f);
    EXPECT_FLOAT_EQ(idle.width, 0.5f);

    auto run = sheet->getUVRect("run");
    EXPECT_FLOAT_EQ(run.u, 0.5f);

    // Also accessible by index
    auto byIdx = sheet->getUVRect(0);
    EXPECT_FLOAT_EQ(byIdx.u, idle.u);
}

TEST(SpriteSheetTest, NameLookupThrowsOnMissing) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    sheet->addSprite("a", 0, 0, 32, 32);
    EXPECT_THROW(sheet->getUVRect("missing"), std::out_of_range);
}

TEST(SpriteSheetTest, DuplicateNameThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    sheet->addSprite("a", 0, 0, 32, 32);
    EXPECT_THROW(sheet->addSprite("a", 32, 0, 32, 32), std::invalid_argument);
}

TEST(SpriteSheetTest, EmptyNameThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("", 0, 0, 32, 32), std::invalid_argument);
}

// ============================================================================
// SpriteSheet — Null / invalid arguments
// ============================================================================

TEST(SpriteSheetTest, CreateGridNullTextureThrows) {
    EXPECT_THROW(SpriteSheet::createGrid(nullptr, 4, 2), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateGridZeroColumnsThrows) {
    auto tex = makeTestTexture(64, 64);
    EXPECT_THROW(SpriteSheet::createGrid(tex, 0, 2), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateManualNullTextureThrows) {
    EXPECT_THROW(SpriteSheet::create(nullptr), std::invalid_argument);
}

// ============================================================================
// SpriteSheet — addSprite() bounds & dimension validation
// ============================================================================

TEST(SpriteSheetTest, AddSpriteZeroWidthThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 0, 0, 0, 32), std::invalid_argument);
}

TEST(SpriteSheetTest, AddSpriteNegativeWidthThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 0, 0, -1, 32), std::invalid_argument);
}

TEST(SpriteSheetTest, AddSpriteZeroHeightThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 0, 0, 32, 0), std::invalid_argument);
}

TEST(SpriteSheetTest, AddSpriteNegativeXThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", -1, 0, 32, 32), std::invalid_argument);
}

TEST(SpriteSheetTest, AddSpriteNegativeYThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 0, -1, 32, 32), std::invalid_argument);
}

TEST(SpriteSheetTest, AddSpriteExceedsTextureWidthThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 48, 0, 32, 32), std::out_of_range);
}

TEST(SpriteSheetTest, AddSpriteExceedsTextureHeightThrows) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_THROW(sheet->addSprite("a", 0, 48, 32, 32), std::out_of_range);
}

TEST(SpriteSheetTest, AddSpriteExactBoundsOk) {
    auto tex = makeTestTexture(64, 64);
    auto sheet = SpriteSheet::create(tex);
    EXPECT_NO_THROW(sheet->addSprite("full", 0, 0, 64, 64));
}

// ============================================================================
// SpriteSheet — createGrid() spacing validation
// ============================================================================

TEST(SpriteSheetTest, CreateGridNegativeSpacingThrows) {
    auto tex = makeTestTexture(64, 64);
    EXPECT_THROW(SpriteSheet::createGrid(tex, 2, 2, -1), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateGridExcessiveSpacingThrows) {
    auto tex = makeTestTexture(64, 64);
    // 2 cols, spacing of 100 → totalSpacingX = 100, (64 - 100)/2 = negative
    EXPECT_THROW(SpriteSheet::createGrid(tex, 2, 2, 100), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateGridUnevenDimensionsThrows) {
    // 65 wide / 3 cols = 21 remainder 2 → remainder pixels would be silently dropped
    auto tex = makeTestTexture(65, 64);
    EXPECT_THROW(SpriteSheet::createGrid(tex, 3, 2), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateGridUnevenHeightThrows) {
    auto tex = makeTestTexture(64, 65);
    EXPECT_THROW(SpriteSheet::createGrid(tex, 2, 3), std::invalid_argument);
}

TEST(SpriteSheetTest, CreateGridUnevenWithSpacingThrows) {
    // 70 wide, 2 cols, 5px spacing → usable = 70 - 5 = 65, 65 / 2 = 32 remainder 1
    auto tex = makeTestTexture(70, 64);
    EXPECT_THROW(SpriteSheet::createGrid(tex, 2, 2, 5), std::invalid_argument);
}

// ============================================================================
// SpriteEntity — Flip flags
// ============================================================================

class SpriteFlipTest : public ::testing::Test {
  protected:
    void SetUp() override { sprite = std::make_shared<SpriteEntity>(); }
    std::shared_ptr<SpriteEntity> sprite;
};

TEST_F(SpriteFlipTest, DefaultNotFlipped) {
    EXPECT_FALSE(sprite->isFlippedX());
    EXPECT_FALSE(sprite->isFlippedY());
}

TEST_F(SpriteFlipTest, SetFlipX) {
    sprite->setFlipX(true);
    EXPECT_TRUE(sprite->isFlippedX());
    EXPECT_FALSE(sprite->isFlippedY());

    sprite->setFlipX(false);
    EXPECT_FALSE(sprite->isFlippedX());
}

TEST_F(SpriteFlipTest, SetFlipY) {
    sprite->setFlipY(true);
    EXPECT_TRUE(sprite->isFlippedY());
    EXPECT_FALSE(sprite->isFlippedX());
}

TEST_F(SpriteFlipTest, FlipDoesNotAffectPosition) {
    sprite->setPosition(1.0f, 2.0f, 3.0f);
    sprite->setFlipX(true);
    sprite->setFlipY(true);

    auto pos = sprite->getPosition();
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

TEST_F(SpriteFlipTest, FlipDoesNotAffectAnchor) {
    sprite->setAnchor(0.3f, 0.7f);
    sprite->setFlipX(true);
    EXPECT_FLOAT_EQ(sprite->getAnchorX(), 0.3f);
    EXPECT_FLOAT_EQ(sprite->getAnchorY(), 0.7f);
}

TEST_F(SpriteFlipTest, FlipDoesNotAffectScale) {
    sprite->setScale(2.0f, 3.0f, 1.0f);
    sprite->setFlipX(true);
    sprite->setFlipY(true);
    auto scale = sprite->getScale();
    EXPECT_FLOAT_EQ(scale.x, 2.0f);
    EXPECT_FLOAT_EQ(scale.y, 3.0f);
}

}  // namespace test
}  // namespace vde
