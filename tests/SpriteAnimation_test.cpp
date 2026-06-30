/**
 * @file SpriteAnimation_test.cpp
 * @brief Unit tests for SpriteAnimation and AnimatedSpriteEntity.
 */

#include <vde/Texture.h>
#include <vde/api/AnimatedSpriteEntity.h>
#include <vde/api/SpriteAnimationImport.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

namespace {

std::shared_ptr<Texture> makeTestTexture(uint32_t width, uint32_t height) {
    auto texture = std::make_shared<Texture>();
    std::vector<uint8_t> pixels(static_cast<std::vector<uint8_t>::size_type>(width) *
                                    static_cast<std::vector<uint8_t>::size_type>(height) * 4,
                                255);
    texture->loadFromData(pixels.data(), width, height);
    return texture;
}

SpriteAnimation makeThreeFrameLoop(const std::string& name = "run") {
    SpriteAnimation animation(name);
    animation.addFrame(0, 0.1f);
    animation.addFrame(1, 0.1f);
    animation.addFrame(2, 0.1f);
    return animation;
}

class TestAnimatedSpriteEntity : public AnimatedSpriteEntity {
  public:
    [[nodiscard]] float getUVX() const { return m_uvX; }
    [[nodiscard]] float getUVWidth() const { return m_uvWidth; }
};

}  // namespace

TEST(SpriteAnimationTest, LoopingFrameLookupWrapsToBeginning) {
    SpriteAnimation animation = makeThreeFrameLoop();

    EXPECT_EQ(animation.getFrameAtTime(0.00f), 0);
    EXPECT_EQ(animation.getFrameAtTime(0.12f), 1);
    EXPECT_EQ(animation.getFrameAtTime(0.24f), 2);
    EXPECT_EQ(animation.getFrameAtTime(0.35f), 0);
}

TEST(SpriteAnimationTest, OneShotFrameLookupClampsToLastFrame) {
    SpriteAnimation animation = makeThreeFrameLoop("attack");
    animation.setLooping(false);

    EXPECT_EQ(animation.getFrameAtTime(0.35f), 2);
    EXPECT_EQ(animation.getFrameAtTime(1.50f), 2);
}

TEST(AnimatedSpriteEntityTest, PlayWithoutResetPreservesCurrentFrameAndUv) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());

    entity.play("run");
    entity.update(0.12f);
    ASSERT_EQ(entity.getCurrentFrame(), 1);
    float preservedU = entity.getUVX();

    entity.play("run", false);
    EXPECT_EQ(entity.getCurrentFrame(), 1);
    EXPECT_FLOAT_EQ(entity.getUVX(), preservedU);

    entity.play("run", true);
    EXPECT_EQ(entity.getCurrentFrame(), 0);
    EXPECT_FLOAT_EQ(entity.getUVX(), 0.0f);
    EXPECT_FLOAT_EQ(entity.getUVWidth(), 1.0f / 3.0f);
}

TEST(AnimatedSpriteEntityTest, PauseAndResumeControlPlayback) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());

    entity.play("run");
    entity.pause();
    entity.update(0.25f);
    EXPECT_EQ(entity.getCurrentFrame(), 0);
    EXPECT_TRUE(entity.isPaused());

    entity.resume();
    entity.update(0.12f);
    EXPECT_EQ(entity.getCurrentFrame(), 1);
    EXPECT_FALSE(entity.isPaused());
}

TEST(AnimatedSpriteEntityTest, SpeedMultiplierAdvancesFramesFaster) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());
    entity.setSpeed(2.0f);

    entity.play("run");
    entity.update(0.06f);

    EXPECT_EQ(entity.getCurrentFrame(), 1);
    EXPECT_FLOAT_EQ(entity.getUVX(), 1.0f / 3.0f);
}

TEST(AnimatedSpriteEntityTest, FrameCallbacksFireOncePerPassThroughFrame) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());

    int hitCount = 0;
    entity.onFrameEvent("run", 1, [&hitCount]() { ++hitCount; });

    entity.play("run");
    entity.update(0.11f);
    EXPECT_EQ(hitCount, 1);

    entity.update(0.05f);
    EXPECT_EQ(hitCount, 1);

    entity.update(0.30f);
    EXPECT_EQ(hitCount, 2);
}

TEST(AnimatedSpriteEntityTest, OneShotAnimationStopsOnLastFrameAndReportsFinished) {
    auto texture = makeTestTexture(64, 32);
    auto sheet = SpriteSheet::createGrid(texture, 2, 1);

    SpriteAnimation attack("attack");
    attack.addFrame(0, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("attack", attack);
    entity.play("attack");
    entity.update(0.25f);

    EXPECT_EQ(entity.getCurrentFrame(), 1);
    EXPECT_FALSE(entity.isPlaying());
    EXPECT_TRUE(entity.isAnimationFinished());
    EXPECT_FLOAT_EQ(entity.getUVX(), 0.5f);
}

TEST(AnimatedSpriteEntityTest, StopResetsFinishedStateAndReturnsToFirstFrame) {
    auto texture = makeTestTexture(64, 32);
    auto sheet = SpriteSheet::createGrid(texture, 2, 1);

    SpriteAnimation attack("attack");
    attack.addFrame(0, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("attack", attack);
    entity.play("attack");
    entity.update(0.25f);
    ASSERT_TRUE(entity.isAnimationFinished());

    entity.stop();

    EXPECT_FALSE(entity.isAnimationFinished());
    EXPECT_EQ(entity.getCurrentFrame(), 0);
    EXPECT_FLOAT_EQ(entity.getUVX(), 0.0f);
}

TEST(AnimatedSpriteEntityTest, PlayWithoutResetRestartsFinishedOneShotAnimation) {
    auto texture = makeTestTexture(64, 32);
    auto sheet = SpriteSheet::createGrid(texture, 2, 1);

    SpriteAnimation attack("attack");
    attack.addFrame(0, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("attack", attack);
    entity.play("attack");
    entity.update(0.25f);
    ASSERT_TRUE(entity.isAnimationFinished());

    entity.play("attack", false);

    EXPECT_FALSE(entity.isAnimationFinished());
    EXPECT_TRUE(entity.isPlaying());
    EXPECT_EQ(entity.getCurrentFrame(), 0);
}

TEST(AnimatedSpriteEntityTest, ResumeDoesNothingAfterOneShotAnimationFinished) {
    auto texture = makeTestTexture(64, 32);
    auto sheet = SpriteSheet::createGrid(texture, 2, 1);

    SpriteAnimation attack("attack");
    attack.addFrame(0, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("attack", attack);
    entity.play("attack");
    entity.update(0.25f);
    ASSERT_TRUE(entity.isAnimationFinished());

    entity.pause();
    entity.resume();

    EXPECT_FALSE(entity.isPlaying());
    EXPECT_TRUE(entity.isAnimationFinished());
    EXPECT_EQ(entity.getCurrentFrame(), 1);
}

TEST(AnimatedSpriteEntityTest, FrameEventRegistrationRequiresExistingAnimation) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());

    EXPECT_THROW(entity.onFrameEvent("missing", 0, []() {}), std::out_of_range);
}

TEST(AnimatedSpriteEntityTest, ReplacingActiveAnimationRestartsPlaybackFromFirstFrame) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());
    entity.play("run");
    entity.update(0.12f);
    ASSERT_EQ(entity.getCurrentFrame(), 1);

    SpriteAnimation replacement("run");
    replacement.addFrame(2, 0.2f);
    replacement.addFrame(1, 0.2f);
    entity.addAnimation("run", replacement);

    EXPECT_EQ(entity.getCurrentFrame(), 0);
    EXPECT_FLOAT_EQ(entity.getUVX(), 2.0f / 3.0f);
}

TEST(AnimatedSpriteEntityTest, FinishedTransitionAutomaticallyPlaysTargetAnimation) {
    auto texture = makeTestTexture(64, 32);
    auto sheet = SpriteSheet::createGrid(texture, 2, 1);

    SpriteAnimation attack("attack");
    attack.addFrame(0, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);

    SpriteAnimation idle("idle");
    idle.addFrame(0, 0.2f);
    idle.addFrame(1, 0.2f);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("attack", attack);
    entity.addAnimation("idle", idle);
    entity.addFinishedTransition("attack", "idle");

    entity.play("attack");
    entity.update(0.25f);

    EXPECT_EQ(entity.getCurrentAnimation(), "idle");
    EXPECT_TRUE(entity.isPlaying());
    EXPECT_FALSE(entity.isAnimationFinished());
    EXPECT_EQ(entity.getCurrentFrame(), 0);
}

TEST(AnimatedSpriteEntityTest, ConditionalTransitionBlendCallbackRunsToCompletion) {
    auto texture = makeTestTexture(96, 32);
    auto sheet = SpriteSheet::createGrid(texture, 3, 1);

    TestAnimatedSpriteEntity entity;
    entity.setSpriteSheet(sheet);
    entity.addAnimation("run", makeThreeFrameLoop());

    SpriteAnimation attack("attack");
    attack.addFrame(2, 0.1f);
    attack.addFrame(1, 0.1f);
    attack.setLooping(false);
    entity.addAnimation("attack", attack);

    bool shouldTransition = false;
    std::vector<float> blendProgresses;
    entity.addConditionalTransition(
        "run", "attack",
        [&shouldTransition](const AnimatedSpriteEntity&) { return shouldTransition; }, 0.2f,
        [&blendProgresses](AnimatedSpriteEntity&, const std::string& from, const std::string& to,
                           float progress) {
            EXPECT_EQ(from, "run");
            EXPECT_EQ(to, "attack");
            blendProgresses.push_back(progress);
        });

    entity.play("run");
    shouldTransition = true;
    entity.update(0.01f);

    ASSERT_EQ(entity.getCurrentAnimation(), "attack");
    ASSERT_TRUE(entity.hasActiveBlend());
    ASSERT_FALSE(blendProgresses.empty());
    EXPECT_FLOAT_EQ(blendProgresses.front(), 0.0f);

    entity.update(0.25f);

    EXPECT_FALSE(entity.hasActiveBlend());
    ASSERT_FALSE(blendProgresses.empty());
    EXPECT_FLOAT_EQ(blendProgresses.back(), 1.0f);
}

TEST(SpriteAnimationImportTest, ImportAsepriteJsonBuildsSpriteSheetAndTaggedAnimations) {
    auto texture = makeTestTexture(64, 16);

    const std::string jsonText = R"json(
{
    "frames": [
        {"filename": "idle_0", "frame": {"x": 0, "y": 0, "w": 16, "h": 16}, "duration": 100},
        {"filename": "idle_1", "frame": {"x": 16, "y": 0, "w": 16, "h": 16}, "duration": 120},
        {"filename": "run_0",  "frame": {"x": 32, "y": 0, "w": 16, "h": 16}, "duration": 80},
        {"filename": "run_1",  "frame": {"x": 48, "y": 0, "w": 16, "h": 16}, "duration": 90}
    ],
    "meta": {
        "image": "hero.png",
        "size": {"w": 64, "h": 16},
        "frameTags": [
            {"name": "idle", "from": 0, "to": 1, "direction": "forward"},
            {"name": "run",  "from": 2, "to": 3, "direction": "pingpong"}
        ]
    }
}
)json";

    auto imported = SpriteAnimationImport::importAsepriteJson(texture, jsonText);

    ASSERT_NE(imported.spriteSheet, nullptr);
    EXPECT_EQ(imported.spriteSheet->getSpriteCount(), 4);
    EXPECT_EQ(imported.frameNames.size(), 4);
    ASSERT_TRUE(imported.animations.contains("idle"));
    ASSERT_TRUE(imported.animations.contains("run"));

    const auto& idle = imported.animations.at("idle");
    EXPECT_EQ(idle.getFrameCount(), 2);
    EXPECT_NEAR(idle.getTotalDuration(), 0.22f, 0.0001f);
    EXPECT_EQ(idle.getFrame(0).spriteIndex, 0);
    EXPECT_EQ(idle.getFrame(1).spriteIndex, 1);

    const auto& run = imported.animations.at("run");
    EXPECT_EQ(run.getFrameCount(), 2);
    EXPECT_EQ(run.getFrame(0).spriteIndex, 2);
    EXPECT_EQ(run.getFrame(1).spriteIndex, 3);
}

TEST(SpriteAnimationImportTest, ImportAsepriteJsonPingPongTagPreservesReverseStep) {
    auto texture = makeTestTexture(48, 16);

    const std::string jsonText = R"json(
{
    "frames": [
        {"filename": "run_0",  "frame": {"x": 0,  "y": 0, "w": 16, "h": 16}, "duration": 80},
        {"filename": "run_1",  "frame": {"x": 16, "y": 0, "w": 16, "h": 16}, "duration": 90},
        {"filename": "run_2",  "frame": {"x": 32, "y": 0, "w": 16, "h": 16}, "duration": 100}
    ],
    "meta": {
        "image": "hero.png",
        "size": {"w": 48, "h": 16},
        "frameTags": [
            {"name": "run", "from": 0, "to": 2, "direction": "pingpong"}
        ]
    }
}
)json";

    auto imported = SpriteAnimationImport::importAsepriteJson(texture, jsonText);

    ASSERT_TRUE(imported.animations.contains("run"));
    const auto& run = imported.animations.at("run");
    ASSERT_EQ(run.getFrameCount(), 4);
    EXPECT_EQ(run.getFrame(0).spriteIndex, 0);
    EXPECT_EQ(run.getFrame(1).spriteIndex, 1);
    EXPECT_EQ(run.getFrame(2).spriteIndex, 2);
    EXPECT_EQ(run.getFrame(3).spriteIndex, 1);
}

TEST(SpriteAnimationImportTest, ImportAsepriteJsonFileReadsMetadataFromDisk) {
    auto texture = makeTestTexture(16, 16);

    const std::string jsonText = R"json(
{
    "frames": {
        "single": {"frame": {"x": 0, "y": 0, "w": 16, "h": 16}, "duration": 75}
    },
    "meta": {
        "image": "single.png",
        "size": {"w": 16, "h": 16}
    }
}
)json";

    const auto jsonPath =
        std::filesystem::temp_directory_path() / "vde_sprite_animation_import_test.json";
    {
        std::ofstream output(jsonPath);
        output << jsonText;
    }

    auto imported = SpriteAnimationImport::importAsepriteJsonFile(texture, jsonPath.string());
    std::filesystem::remove(jsonPath);

    ASSERT_NE(imported.spriteSheet, nullptr);
    ASSERT_TRUE(imported.animations.contains("single"));
    EXPECT_EQ(imported.animations.at("single").getFrameCount(), 1);
    EXPECT_NEAR(imported.animations.at("single").getTotalDuration(), 0.075f, 0.0001f);
}

}  // namespace vde::test