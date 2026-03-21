/**
 * @file SpriteDocument_test.cpp
 * @brief Unit tests for SpriteDocument — sprite regions, animations, and
 *        TOML serialization.
 */

#include <cmath>

#include "../../tools/sprite_editor/SpriteDocument.h"
#include <gtest/gtest.h>

namespace vde::test {

// ═══════════════════════════════════════════════════════════════
//  SpriteRegion Tests
// ═══════════════════════════════════════════════════════════════

class SpriteDocumentTest : public ::testing::Test {
  protected:
    vde::tools::SpriteDocument doc;

    void SetUp() override {
        // Set up a 256x128 virtual image for grid tests.
        doc.setSourceImage("test.png", 256, 128);
    }
};

TEST_F(SpriteDocumentTest, SetSourceImage) {
    EXPECT_EQ(doc.getSourceImagePath(), "test.png");
    EXPECT_EQ(doc.getImageWidth(), 256);
    EXPECT_EQ(doc.getImageHeight(), 128);
    EXPECT_TRUE(doc.hasImage());
}

TEST_F(SpriteDocumentTest, HasImageReturnsFalseWhenEmpty) {
    vde::tools::SpriteDocument empty;
    EXPECT_FALSE(empty.hasImage());
}

TEST_F(SpriteDocumentTest, AddSprite) {
    vde::tools::SpriteRegion r{"idle_0", 0, 0, 32, 32};
    EXPECT_TRUE(doc.addSprite(r));
    EXPECT_EQ(doc.getSpriteCount(), 1);

    const auto* found = doc.findSprite("idle_0");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->x, 0);
    EXPECT_EQ(found->w, 32);
}

TEST_F(SpriteDocumentTest, AddDuplicateSpriteFails) {
    vde::tools::SpriteRegion r{"idle_0", 0, 0, 32, 32};
    EXPECT_TRUE(doc.addSprite(r));
    EXPECT_FALSE(doc.addSprite(r));
    EXPECT_EQ(doc.getSpriteCount(), 1);
}

TEST_F(SpriteDocumentTest, RemoveSprite) {
    vde::tools::SpriteRegion r{"idle_0", 0, 0, 32, 32};
    doc.addSprite(r);
    EXPECT_TRUE(doc.removeSprite("idle_0"));
    EXPECT_EQ(doc.getSpriteCount(), 0);
    EXPECT_EQ(doc.findSprite("idle_0"), nullptr);
}

TEST_F(SpriteDocumentTest, RemoveNonexistentSpriteFails) {
    EXPECT_FALSE(doc.removeSprite("nope"));
}

TEST_F(SpriteDocumentTest, RenameSprite) {
    doc.addSprite({"old_name", 0, 0, 32, 32});
    EXPECT_TRUE(doc.renameSprite("old_name", "new_name"));
    EXPECT_EQ(doc.findSprite("old_name"), nullptr);
    EXPECT_NE(doc.findSprite("new_name"), nullptr);
}

TEST_F(SpriteDocumentTest, RenameToExistingNameFails) {
    doc.addSprite({"a", 0, 0, 32, 32});
    doc.addSprite({"b", 32, 0, 32, 32});
    EXPECT_FALSE(doc.renameSprite("a", "b"));
}

TEST_F(SpriteDocumentTest, SetAnchor) {
    doc.addSprite({"s", 0, 0, 32, 32});
    EXPECT_TRUE(doc.setAnchor("s", 0.0f, 1.0f));

    const auto* s = doc.findSprite("s");
    ASSERT_NE(s, nullptr);
    EXPECT_FLOAT_EQ(s->anchorX, 0.0f);
    EXPECT_FLOAT_EQ(s->anchorY, 1.0f);
}

TEST_F(SpriteDocumentTest, SetAnchorMissingSpriteFails) {
    EXPECT_FALSE(doc.setAnchor("nope", 0.0f, 0.0f));
}

TEST_F(SpriteDocumentTest, ClearSprites) {
    doc.addSprite({"a", 0, 0, 32, 32});
    doc.addSprite({"b", 32, 0, 32, 32});
    doc.clearSprites();
    EXPECT_EQ(doc.getSpriteCount(), 0);
}

// ── Grid slice ──────────────────────────────────────────────────

TEST_F(SpriteDocumentTest, GridSliceBasic) {
    // 256x128 image with 64x64 cells → 4 cols x 2 rows = 8 sprites
    int count = doc.gridSlice(64, 64);
    EXPECT_EQ(count, 8);
    EXPECT_EQ(doc.getSpriteCount(), 8);

    // First sprite
    const auto* first = doc.findSprite("sprite_0");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->x, 0);
    EXPECT_EQ(first->y, 0);
    EXPECT_EQ(first->w, 64);
    EXPECT_EQ(first->h, 64);

    // Last sprite (sprite_7)
    const auto* last = doc.findSprite("sprite_7");
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->x, 192);
    EXPECT_EQ(last->y, 64);
}

TEST_F(SpriteDocumentTest, GridSliceWithSpacing) {
    // 256x128 with 60x60 cells and 4px spacing
    // Cols: 0, 64, 128, 192 → 4 cols (60+4=64 stride, 192+60=252 ≤ 256)
    // Rows: 0, 64 → 2 rows (60+4=64 stride, 64+60=124 ≤ 128)
    int count = doc.gridSlice(60, 60, 4, 4);
    EXPECT_EQ(count, 8);
}

TEST_F(SpriteDocumentTest, GridSliceWithOffset) {
    // 256x128 with 64x64 cells, offset (10, 10)
    // Cols: 10, 74, 138, 202 → 3 cols (202+64=266 > 256, only 3)
    // Wait: 10+64=74, 74+64=138, 138+64=202, 202+64=266 > 256 → 3 cols
    // Rows: 10+64=74, 74+64=138 > 128 → 1 row
    int count = doc.gridSlice(64, 64, 0, 0, 10, 10);
    EXPECT_EQ(count, 3);
}

TEST_F(SpriteDocumentTest, GridSliceReplacesExisting) {
    doc.addSprite({"manual", 0, 0, 10, 10});
    EXPECT_EQ(doc.getSpriteCount(), 1);

    doc.gridSlice(64, 64);
    EXPECT_EQ(doc.getSpriteCount(), 8);
    EXPECT_EQ(doc.findSprite("manual"), nullptr);
}

TEST_F(SpriteDocumentTest, GridSliceNoImageReturnZero) {
    vde::tools::SpriteDocument empty;
    EXPECT_EQ(empty.gridSlice(64, 64), 0);
}

TEST_F(SpriteDocumentTest, GridSliceInvalidCellSizeReturnZero) {
    EXPECT_EQ(doc.gridSlice(0, 64), 0);
    EXPECT_EQ(doc.gridSlice(64, -1), 0);
}

// ═══════════════════════════════════════════════════════════════
//  Animation Tests
// ═══════════════════════════════════════════════════════════════

class AnimationTest : public ::testing::Test {
  protected:
    vde::tools::SpriteDocument doc;

    void SetUp() override {
        doc.setSourceImage("test.png", 256, 128);
        doc.addSprite({"idle_0", 0, 0, 64, 64});
        doc.addSprite({"idle_1", 64, 0, 64, 64});
        doc.addSprite({"run_0", 128, 0, 64, 64});
    }
};

TEST_F(AnimationTest, CreateAnimation) {
    EXPECT_TRUE(doc.createAnimation("idle"));
    EXPECT_EQ(doc.getAnimationCount(), 1);

    const auto* anim = doc.findAnimation("idle");
    ASSERT_NE(anim, nullptr);
    EXPECT_EQ(anim->name, "idle");
    EXPECT_TRUE(anim->looping);
    EXPECT_TRUE(anim->frames.empty());
}

TEST_F(AnimationTest, CreateNonLooping) {
    EXPECT_TRUE(doc.createAnimation("death", false));
    const auto* anim = doc.findAnimation("death");
    ASSERT_NE(anim, nullptr);
    EXPECT_FALSE(anim->looping);
}

TEST_F(AnimationTest, CreateDuplicateFails) {
    doc.createAnimation("idle");
    EXPECT_FALSE(doc.createAnimation("idle"));
}

TEST_F(AnimationTest, DeleteAnimation) {
    doc.createAnimation("idle");
    EXPECT_TRUE(doc.deleteAnimation("idle"));
    EXPECT_EQ(doc.getAnimationCount(), 0);
}

TEST_F(AnimationTest, DeleteMissingAnimationFails) {
    EXPECT_FALSE(doc.deleteAnimation("nope"));
}

TEST_F(AnimationTest, AddFrame) {
    doc.createAnimation("idle");
    EXPECT_TRUE(doc.addFrame("idle", "idle_0", 0.15f));
    EXPECT_TRUE(doc.addFrame("idle", "idle_1", 0.15f));

    const auto* anim = doc.findAnimation("idle");
    ASSERT_NE(anim, nullptr);
    ASSERT_EQ(anim->frames.size(), 2u);
    EXPECT_EQ(anim->frames[0].spriteName, "idle_0");
    EXPECT_FLOAT_EQ(anim->frames[0].duration, 0.15f);
}

TEST_F(AnimationTest, AddFrameMissingAnimationFails) {
    EXPECT_FALSE(doc.addFrame("nope", "idle_0"));
}

TEST_F(AnimationTest, AddFrameMissingSpriteFails) {
    doc.createAnimation("idle");
    EXPECT_FALSE(doc.addFrame("idle", "nonexistent"));
}

TEST_F(AnimationTest, RemoveFrame) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0");
    doc.addFrame("idle", "idle_1");

    EXPECT_TRUE(doc.removeFrame("idle", 0));
    const auto* anim = doc.findAnimation("idle");
    ASSERT_EQ(anim->frames.size(), 1u);
    EXPECT_EQ(anim->frames[0].spriteName, "idle_1");
}

TEST_F(AnimationTest, RemoveFrameOutOfBoundsFails) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0");
    EXPECT_FALSE(doc.removeFrame("idle", 5));
    EXPECT_FALSE(doc.removeFrame("idle", -1));
}

TEST_F(AnimationTest, SetFrameDuration) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0");
    EXPECT_TRUE(doc.setFrameDuration("idle", 0, 0.25f));

    const auto* anim = doc.findAnimation("idle");
    EXPECT_FLOAT_EQ(anim->frames[0].duration, 0.25f);
}

TEST_F(AnimationTest, ClearAnimations) {
    doc.createAnimation("idle");
    doc.createAnimation("run");
    doc.clearAnimations();
    EXPECT_EQ(doc.getAnimationCount(), 0);
}

// ── AnimationSequence timing ────────────────────────────────────

TEST_F(AnimationTest, GetTotalDuration) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0", 0.15f);
    doc.addFrame("idle", "idle_1", 0.25f);

    const auto* anim = doc.findAnimation("idle");
    EXPECT_FLOAT_EQ(anim->getTotalDuration(), 0.40f);
}

TEST_F(AnimationTest, GetFrameAtBasic) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0", 0.2f);
    doc.addFrame("idle", "idle_1", 0.3f);

    const auto* anim = doc.findAnimation("idle");

    // t=0 → first frame
    EXPECT_EQ(anim->getFrameAt(0.0f), "idle_0");
    // t=0.1 → still first frame
    EXPECT_EQ(anim->getFrameAt(0.1f), "idle_0");
    // t=0.2 → second frame
    EXPECT_EQ(anim->getFrameAt(0.2f), "idle_1");
    // t=0.4 → past end, so wraps to first frame (looping)
    EXPECT_EQ(anim->getFrameAt(0.5f), "idle_0");
}

TEST_F(AnimationTest, GetFrameAtNonLooping) {
    doc.createAnimation("death", false);
    doc.addFrame("death", "idle_0", 0.2f);
    doc.addFrame("death", "idle_1", 0.3f);

    const auto* anim = doc.findAnimation("death");

    // Past end → last frame (non-looping)
    EXPECT_EQ(anim->getFrameAt(1.0f), "idle_1");
}

TEST_F(AnimationTest, GetFrameAtEmptyReturnsEmpty) {
    doc.createAnimation("empty");
    const auto* anim = doc.findAnimation("empty");
    EXPECT_TRUE(anim->getFrameAt(0.0f).empty());
}

// ═══════════════════════════════════════════════════════════════
//  Serialization Tests
// ═══════════════════════════════════════════════════════════════

class SerializationTest : public ::testing::Test {
  protected:
    vde::tools::SpriteDocument doc;

    void SetUp() override {
        doc.setSourceImage("characters.png", 512, 256);
        doc.setAuthor("tester");
        doc.setNotes("test sheet");

        doc.addSprite({"idle_0", 0, 0, 64, 64});
        doc.addSprite({"idle_1", 64, 0, 64, 64});

        doc.createAnimation("idle");
        doc.addFrame("idle", "idle_0", 0.15f);
        doc.addFrame("idle", "idle_1", 0.15f);
    }
};

TEST_F(SerializationTest, RoundTrip) {
    std::string toml = doc.serializeToString();
    EXPECT_FALSE(toml.empty());

    vde::tools::SpriteDocument loaded;
    EXPECT_TRUE(loaded.deserializeFromString(toml));

    EXPECT_EQ(loaded.getSourceImagePath(), "characters.png");
    EXPECT_EQ(loaded.getImageWidth(), 512);
    EXPECT_EQ(loaded.getImageHeight(), 256);
    EXPECT_EQ(loaded.getAuthor(), "tester");
    EXPECT_EQ(loaded.getNotes(), "test sheet");

    EXPECT_EQ(loaded.getSpriteCount(), 2);
    const auto* s0 = loaded.findSprite("idle_0");
    ASSERT_NE(s0, nullptr);
    EXPECT_EQ(s0->x, 0);
    EXPECT_EQ(s0->w, 64);

    EXPECT_EQ(loaded.getAnimationCount(), 1);
    const auto* anim = loaded.findAnimation("idle");
    ASSERT_NE(anim, nullptr);
    EXPECT_TRUE(anim->looping);
    ASSERT_EQ(anim->frames.size(), 2u);
    EXPECT_EQ(anim->frames[0].spriteName, "idle_0");
    EXPECT_FLOAT_EQ(anim->frames[0].duration, 0.15f);
}

TEST_F(SerializationTest, SerializationPreservesAnchor) {
    doc.setAnchor("idle_0", 0.0f, 1.0f);
    std::string toml = doc.serializeToString();

    vde::tools::SpriteDocument loaded;
    loaded.deserializeFromString(toml);

    const auto* s0 = loaded.findSprite("idle_0");
    ASSERT_NE(s0, nullptr);
    EXPECT_FLOAT_EQ(s0->anchorX, 0.0f);
    EXPECT_FLOAT_EQ(s0->anchorY, 1.0f);
}

TEST_F(SerializationTest, DeserializeInvalidTomlFails) {
    vde::tools::SpriteDocument d;
    EXPECT_FALSE(d.deserializeFromString("{{{{invalid toml!!!!"));
}

TEST_F(SerializationTest, DefaultAnchorNotSerialized) {
    // Default anchor (0.5, 0.5) should not appear in TOML output
    std::string toml = doc.serializeToString();
    EXPECT_EQ(toml.find("anchor_x"), std::string::npos);
    EXPECT_EQ(toml.find("anchor_y"), std::string::npos);
}

TEST_F(SerializationTest, NonDefaultAnchorSerialized) {
    doc.setAnchor("idle_0", 0.0f, 1.0f);
    std::string toml = doc.serializeToString();
    EXPECT_NE(toml.find("anchor_x"), std::string::npos);
    EXPECT_NE(toml.find("anchor_y"), std::string::npos);
}

TEST_F(SerializationTest, FileRoundTrip) {
    const std::string testPath = "test_output.vdesheet";

    EXPECT_TRUE(doc.saveToFile(testPath));

    vde::tools::SpriteDocument loaded;
    EXPECT_TRUE(loaded.loadFromFile(testPath));

    EXPECT_EQ(loaded.getSourceImagePath(), "characters.png");
    EXPECT_EQ(loaded.getSpriteCount(), 2);
    EXPECT_EQ(loaded.getAnimationCount(), 1);

    // Clean up
    std::remove(testPath.c_str());
}

TEST_F(SerializationTest, LoadFromInvalidPathFails) {
    vde::tools::SpriteDocument d;
    EXPECT_FALSE(d.loadFromFile("nonexistent_file.vdesheet"));
}

// ═══════════════════════════════════════════════════════════════
//  Validation Tests
// ═══════════════════════════════════════════════════════════════

TEST_F(SpriteDocumentTest, AddSpriteOutOfBoundsFails) {
    // Sprite extends beyond image width (256x128 image)
    EXPECT_FALSE(doc.addSprite({"oob", 200, 0, 100, 32}));  // 200+100=300 > 256
    EXPECT_FALSE(doc.addSprite({"oob", 0, 100, 32, 100}));  // 100+100=200 > 128
    EXPECT_FALSE(doc.addSprite({"oob", -1, 0, 32, 32}));    // negative x
    EXPECT_FALSE(doc.addSprite({"oob", 0, -1, 32, 32}));    // negative y
    EXPECT_EQ(doc.getSpriteCount(), 0);
}

TEST_F(SpriteDocumentTest, AddSpriteZeroSizeFails) {
    EXPECT_FALSE(doc.addSprite({"bad", 0, 0, 0, 32}));
    EXPECT_FALSE(doc.addSprite({"bad", 0, 0, 32, 0}));
    EXPECT_FALSE(doc.addSprite({"bad", 0, 0, -1, 32}));
}

TEST_F(SpriteDocumentTest, AddSpriteExactBoundsSucceeds) {
    // Exactly fills the image
    EXPECT_TRUE(doc.addSprite({"full", 0, 0, 256, 128}));
    EXPECT_EQ(doc.getSpriteCount(), 1);
}

TEST_F(AnimationTest, AddFrameNegativeDurationFails) {
    doc.createAnimation("idle");
    EXPECT_FALSE(doc.addFrame("idle", "idle_0", -0.1f));
    EXPECT_FALSE(doc.addFrame("idle", "idle_0", 0.0f));
    // Positive duration should work
    EXPECT_TRUE(doc.addFrame("idle", "idle_0", 0.01f));
}

TEST_F(AnimationTest, SetFrameDurationNegativeFails) {
    doc.createAnimation("idle");
    doc.addFrame("idle", "idle_0", 0.1f);
    EXPECT_FALSE(doc.setFrameDuration("idle", 0, 0.0f));
    EXPECT_FALSE(doc.setFrameDuration("idle", 0, -1.0f));
    // Positive still works
    EXPECT_TRUE(doc.setFrameDuration("idle", 0, 0.5f));
}

TEST_F(SpriteDocumentTest, SetAnchorClampsValues) {
    doc.addSprite({"s", 0, 0, 32, 32});
    EXPECT_TRUE(doc.setAnchor("s", 1.5f, -0.5f));
    const auto* s = doc.findSprite("s");
    EXPECT_FLOAT_EQ(s->anchorX, 1.0f);
    EXPECT_FLOAT_EQ(s->anchorY, 0.0f);
}

TEST_F(SpriteDocumentTest, SetSourceImageRejectsInvalidDimensions) {
    vde::tools::SpriteDocument d;
    d.setSourceImage("bad.png", 0, 100);
    EXPECT_FALSE(d.hasImage());
    d.setSourceImage("bad.png", 100, -1);
    EXPECT_FALSE(d.hasImage());
}

TEST_F(SerializationTest, ManuallyAddedSpritesRoundTrip) {
    vde::tools::SpriteDocument d;
    d.setSourceImage("manual.png", 512, 512);
    d.addSprite({"manual_1", 10, 20, 50, 50});
    d.addSprite({"manual_2", 200, 100, 64, 64});

    std::string toml = d.serializeToString();
    vde::tools::SpriteDocument loaded;
    EXPECT_TRUE(loaded.deserializeFromString(toml));

    EXPECT_EQ(loaded.getSpriteCount(), 2);
    EXPECT_EQ(loaded.findSprite("manual_1")->x, 10);
    EXPECT_EQ(loaded.findSprite("manual_1")->y, 20);
    EXPECT_EQ(loaded.findSprite("manual_2")->w, 64);
}

TEST_F(SerializationTest, DeserializedAnchorsClamped) {
    // Manually craft TOML with out-of-range anchors
    std::string toml = R"(
[sheet]
image = "test.png"
image_width = 256
image_height = 256

[[sprites]]
name = "bad_anchor"
x = 0
y = 0
w = 32
h = 32
anchor_x = 1.5
anchor_y = -0.5
)";
    vde::tools::SpriteDocument d;
    EXPECT_TRUE(d.deserializeFromString(toml));
    const auto* s = d.findSprite("bad_anchor");
    ASSERT_NE(s, nullptr);
    EXPECT_FLOAT_EQ(s->anchorX, 1.0f);
    EXPECT_FLOAT_EQ(s->anchorY, 0.0f);
}

}  // namespace vde::test
