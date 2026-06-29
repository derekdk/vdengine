/**
 * @file SpriteAnimation_test.cpp
 * @brief Unit tests for SpriteAnimation and AnimatedSpriteEntity.
 */

#include <vde/Texture.h>
#include <vde/api/AnimatedSpriteEntity.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

namespace {

std::shared_ptr<Texture> makeTestTexture(uint32_t width, uint32_t height) {
    auto texture = std::make_shared<Texture>();
    std::vector<uint8_t> pixels(width * height * 4, 255);
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
    float getUVX() const { return m_uvX; }
    float getUVWidth() const { return m_uvWidth; }
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

}  // namespace vde::test