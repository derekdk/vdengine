/**
 * @file AudioManager_test.cpp
 * @brief Unit tests for AudioManager class (Phase 6 — Audit Remediation)
 *
 * Tests volume control, mute state, and sound-ID queries on the singleton
 * AudioManager without initialising a real audio device.
 */

#include <vde/api/AudioClip.h>
#include <vde/api/AudioManager.h>

#include <memory>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// Fixture — resets AudioManager volume/mute state between tests
// ============================================================================

class AudioManagerTest : public ::testing::Test {
  protected:
    AudioManager& mgr = AudioManager::getInstance();

    void SetUp() override {
        // Reset to sensible defaults (AudioManager is a singleton,
        // so state leaks between tests without explicit reset).
        mgr.setMasterVolume(1.0f);
        mgr.setMusicVolume(1.0f);
        mgr.setSFXVolume(1.0f);
        mgr.setMuted(false);
    }
};

// ============================================================================
// Singleton access
// ============================================================================

TEST_F(AudioManagerTest, SingletonReturnsSameInstance) {
    AudioManager& a = AudioManager::getInstance();
    AudioManager& b = AudioManager::getInstance();
    EXPECT_EQ(&a, &b);
}

// ============================================================================
// Initialization state
// ============================================================================

TEST_F(AudioManagerTest, NotInitializedWithoutExplicitInit) {
    // We intentionally do NOT call initialize() in this test suite
    // to keep tests GPU/audio-device-independent.
    EXPECT_FALSE(mgr.isInitialized());
}

// ============================================================================
// Volume controls (work without an initialised audio device)
// ============================================================================

TEST_F(AudioManagerTest, DefaultMasterVolume) {
    EXPECT_FLOAT_EQ(mgr.getMasterVolume(), 1.0f);
}

TEST_F(AudioManagerTest, DefaultMusicVolume) {
    EXPECT_FLOAT_EQ(mgr.getMusicVolume(), 1.0f);
}

TEST_F(AudioManagerTest, DefaultSFXVolume) {
    EXPECT_FLOAT_EQ(mgr.getSFXVolume(), 1.0f);
}

TEST_F(AudioManagerTest, SetMasterVolume) {
    mgr.setMasterVolume(0.5f);
    EXPECT_FLOAT_EQ(mgr.getMasterVolume(), 0.5f);
}

TEST_F(AudioManagerTest, SetMusicVolume) {
    mgr.setMusicVolume(0.3f);
    EXPECT_FLOAT_EQ(mgr.getMusicVolume(), 0.3f);
}

TEST_F(AudioManagerTest, SetSFXVolume) {
    mgr.setSFXVolume(0.7f);
    EXPECT_FLOAT_EQ(mgr.getSFXVolume(), 0.7f);
}

TEST_F(AudioManagerTest, VolumeClampedToZero) {
    mgr.setMasterVolume(-0.5f);
    EXPECT_FLOAT_EQ(mgr.getMasterVolume(), 0.0f);

    mgr.setMusicVolume(-1.0f);
    EXPECT_FLOAT_EQ(mgr.getMusicVolume(), 0.0f);

    mgr.setSFXVolume(-999.0f);
    EXPECT_FLOAT_EQ(mgr.getSFXVolume(), 0.0f);
}

TEST_F(AudioManagerTest, VolumeClampedToOne) {
    mgr.setMasterVolume(2.0f);
    EXPECT_FLOAT_EQ(mgr.getMasterVolume(), 1.0f);

    mgr.setMusicVolume(100.0f);
    EXPECT_FLOAT_EQ(mgr.getMusicVolume(), 1.0f);

    mgr.setSFXVolume(1.5f);
    EXPECT_FLOAT_EQ(mgr.getSFXVolume(), 1.0f);
}

// ============================================================================
// Mute controls
// ============================================================================

TEST_F(AudioManagerTest, NotMutedByDefault) {
    EXPECT_FALSE(mgr.isMuted());
}

TEST_F(AudioManagerTest, SetMuted) {
    mgr.setMuted(true);
    EXPECT_TRUE(mgr.isMuted());
}

TEST_F(AudioManagerTest, UnmuteRestoresState) {
    mgr.setMuted(true);
    EXPECT_TRUE(mgr.isMuted());

    mgr.setMuted(false);
    EXPECT_FALSE(mgr.isMuted());
}

// ============================================================================
// Playback with invalid state (not initialised)
// ============================================================================

TEST_F(AudioManagerTest, PlaySFXReturnsZeroWhenNotInitialized) {
    auto clip = std::make_shared<AudioClip>();
    uint32_t id = mgr.playSFX(clip);
    EXPECT_EQ(id, 0u);
}

TEST_F(AudioManagerTest, PlayMusicReturnsZeroWhenNotInitialized) {
    auto clip = std::make_shared<AudioClip>();
    uint32_t id = mgr.playMusic(clip);
    EXPECT_EQ(id, 0u);
}

TEST_F(AudioManagerTest, PlaySFXWithNullClipReturnsZero) {
    uint32_t id = mgr.playSFX(nullptr);
    EXPECT_EQ(id, 0u);
}

TEST_F(AudioManagerTest, PlayMusicWithNullClipReturnsZero) {
    uint32_t id = mgr.playMusic(nullptr);
    EXPECT_EQ(id, 0u);
}

TEST_F(AudioManagerTest, PlaySFXWithUnloadedClipReturnsZero) {
    // Simulate loading from an invalid path — clip stays in unloaded state
    auto clip = std::make_shared<AudioClip>();
    // clip is default-constructed (not loaded), so isLoaded() returns false
    EXPECT_EQ(mgr.playSFX(clip), 0u);
}

TEST_F(AudioManagerTest, PlayMusicWithUnloadedClipReturnsZero) {
    auto clip = std::make_shared<AudioClip>();
    EXPECT_EQ(mgr.playMusic(clip), 0u);
}

// ============================================================================
// Sound queries
// ============================================================================

TEST_F(AudioManagerTest, IsPlayingReturnsFalseForInvalidId) {
    EXPECT_FALSE(mgr.isPlaying(0));
    EXPECT_FALSE(mgr.isPlaying(99999));
}

// ============================================================================
// Stop operations are safe when no sounds are active
// ============================================================================

TEST_F(AudioManagerTest, StopAllIsSafeWhenEmpty) {
    mgr.stopAll();
    // No crash — pass
}

TEST_F(AudioManagerTest, StopSoundIsSafeForInvalidId) {
    mgr.stopSound(0);
    mgr.stopSound(99999);
    // No crash — pass
}

}  // namespace test
}  // namespace vde
