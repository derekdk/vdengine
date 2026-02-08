/**
 * @file AudioEvent_test.cpp
 * @brief Unit tests for AudioEvent struct and queue behaviour
 *
 * Tests AudioEvent construction via factory methods, field values,
 * and queue add/drain semantics on Scene.
 */

#include <vde/api/AudioEvent.h>
#include <vde/api/Scene.h>

#include <gtest/gtest.h>

namespace vde::test {

// ============================================================================
// AudioEvent Construction Tests
// ============================================================================

class AudioEventTest : public ::testing::Test {};

TEST_F(AudioEventTest, DefaultConstruction) {
    AudioEvent evt;
    EXPECT_EQ(evt.type, AudioEventType::PlaySFX);
    EXPECT_EQ(evt.clip, nullptr);
    EXPECT_FLOAT_EQ(evt.volume, 1.0f);
    EXPECT_FLOAT_EQ(evt.pitch, 1.0f);
    EXPECT_FALSE(evt.loop);
    EXPECT_FLOAT_EQ(evt.x, 0.0f);
    EXPECT_FLOAT_EQ(evt.y, 0.0f);
    EXPECT_FLOAT_EQ(evt.z, 0.0f);
    EXPECT_EQ(evt.soundId, 0u);
    EXPECT_FLOAT_EQ(evt.fadeTime, 0.0f);
}

TEST_F(AudioEventTest, PlaySFXFactory) {
    auto evt = AudioEvent::playSFX(nullptr, 0.5f, 1.2f, true);
    EXPECT_EQ(evt.type, AudioEventType::PlaySFX);
    EXPECT_EQ(evt.clip, nullptr);
    EXPECT_FLOAT_EQ(evt.volume, 0.5f);
    EXPECT_FLOAT_EQ(evt.pitch, 1.2f);
    EXPECT_TRUE(evt.loop);
}

TEST_F(AudioEventTest, PlaySFXFactoryDefaults) {
    auto evt = AudioEvent::playSFX(nullptr);
    EXPECT_FLOAT_EQ(evt.volume, 1.0f);
    EXPECT_FLOAT_EQ(evt.pitch, 1.0f);
    EXPECT_FALSE(evt.loop);
}

TEST_F(AudioEventTest, PlaySFXAtFactory) {
    auto evt = AudioEvent::playSFXAt(nullptr, 1.0f, 2.0f, 3.0f, 0.7f, 0.9f);
    EXPECT_EQ(evt.type, AudioEventType::PlaySFXAt);
    EXPECT_FLOAT_EQ(evt.x, 1.0f);
    EXPECT_FLOAT_EQ(evt.y, 2.0f);
    EXPECT_FLOAT_EQ(evt.z, 3.0f);
    EXPECT_FLOAT_EQ(evt.volume, 0.7f);
    EXPECT_FLOAT_EQ(evt.pitch, 0.9f);
}

TEST_F(AudioEventTest, PlayMusicFactory) {
    auto evt = AudioEvent::playMusic(nullptr, 0.8f, false, 2.0f);
    EXPECT_EQ(evt.type, AudioEventType::PlayMusic);
    EXPECT_FLOAT_EQ(evt.volume, 0.8f);
    EXPECT_FALSE(evt.loop);
    EXPECT_FLOAT_EQ(evt.fadeTime, 2.0f);
}

TEST_F(AudioEventTest, PlayMusicFactoryDefaults) {
    auto evt = AudioEvent::playMusic(nullptr);
    EXPECT_FLOAT_EQ(evt.volume, 1.0f);
    EXPECT_TRUE(evt.loop);
    EXPECT_FLOAT_EQ(evt.fadeTime, 0.0f);
}

TEST_F(AudioEventTest, StopSoundFactory) {
    auto evt = AudioEvent::stopSound(42, 0.5f);
    EXPECT_EQ(evt.type, AudioEventType::StopSound);
    EXPECT_EQ(evt.soundId, 42u);
    EXPECT_FLOAT_EQ(evt.fadeTime, 0.5f);
}

TEST_F(AudioEventTest, StopAllFactory) {
    auto evt = AudioEvent::stopAll();
    EXPECT_EQ(evt.type, AudioEventType::StopAll);
}

TEST_F(AudioEventTest, PauseSoundFactory) {
    auto evt = AudioEvent::pauseSound(7);
    EXPECT_EQ(evt.type, AudioEventType::PauseSound);
    EXPECT_EQ(evt.soundId, 7u);
}

TEST_F(AudioEventTest, ResumeSoundFactory) {
    auto evt = AudioEvent::resumeSound(7);
    EXPECT_EQ(evt.type, AudioEventType::ResumeSound);
    EXPECT_EQ(evt.soundId, 7u);
}

// ============================================================================
// Scene Audio Event Queue Tests
// ============================================================================

class SceneAudioQueueTest : public ::testing::Test {
  protected:
    std::unique_ptr<Scene> scene;

    void SetUp() override { scene = std::make_unique<Scene>(); }
};

TEST_F(SceneAudioQueueTest, QueueStartsEmpty) {
    EXPECT_EQ(scene->getAudioEventQueueSize(), 0u);
}

TEST_F(SceneAudioQueueTest, QueueAudioEventLvalue) {
    AudioEvent evt;
    evt.type = AudioEventType::StopAll;
    scene->queueAudioEvent(evt);

    EXPECT_EQ(scene->getAudioEventQueueSize(), 1u);
}

TEST_F(SceneAudioQueueTest, QueueAudioEventRvalue) {
    scene->queueAudioEvent(AudioEvent::stopAll());

    EXPECT_EQ(scene->getAudioEventQueueSize(), 1u);
}

TEST_F(SceneAudioQueueTest, PlaySFXConvenienceQueues) {
    scene->playSFX(nullptr, 0.5f, 1.0f, false);

    EXPECT_EQ(scene->getAudioEventQueueSize(), 1u);
}

TEST_F(SceneAudioQueueTest, PlaySFXAtConvenienceQueues) {
    scene->playSFXAt(nullptr, 1.0f, 2.0f, 3.0f, 0.8f);

    EXPECT_EQ(scene->getAudioEventQueueSize(), 1u);
}

TEST_F(SceneAudioQueueTest, MultipleEventsAccumulate) {
    scene->playSFX(nullptr);
    scene->playSFX(nullptr);
    scene->queueAudioEvent(AudioEvent::stopAll());

    EXPECT_EQ(scene->getAudioEventQueueSize(), 3u);
}

TEST_F(SceneAudioQueueTest, UpdateAudioDrainsQueue) {
    scene->playSFX(nullptr);
    scene->playSFX(nullptr);
    EXPECT_EQ(scene->getAudioEventQueueSize(), 2u);

    // updateAudio drains — AudioManager is not initialized so events
    // are simply cleared.
    scene->updateAudio(0.016f);

    EXPECT_EQ(scene->getAudioEventQueueSize(), 0u);
}

TEST_F(SceneAudioQueueTest, EmptyQueueDrainIsSafe) {
    EXPECT_EQ(scene->getAudioEventQueueSize(), 0u);
    // Should not crash
    scene->updateAudio(0.016f);
    EXPECT_EQ(scene->getAudioEventQueueSize(), 0u);
}

}  // namespace vde::test
