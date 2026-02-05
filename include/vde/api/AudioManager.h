#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AudioClip.h"

// Forward declare miniaudio structures
struct ma_engine;
struct ma_sound;

namespace vde {

// Forward declarations
class AudioSource;
struct AudioSettings;

/**
 * @brief Central audio management system.
 *
 * Singleton that manages audio playback, mixing, and volume control.
 * Uses miniaudio library internally for cross-platform audio support.
 */
class AudioManager {
  public:
    /**
     * @brief Get the singleton instance.
     */
    static AudioManager& getInstance();

    // Delete copy/move constructors
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) = delete;
    AudioManager& operator=(AudioManager&&) = delete;

    /**
     * @brief Initialize the audio system.
     * @param settings Audio configuration settings
     * @return true if initialization succeeded
     */
    bool initialize(const AudioSettings& settings);

    /**
     * @brief Shutdown the audio system.
     */
    void shutdown();

    /**
     * @brief Check if audio system is initialized.
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Update audio system (process streaming, update 3D positions, etc).
     * @param deltaTime Time since last update in seconds
     */
    void update(float deltaTime);

    // Volume controls
    void setMasterVolume(float volume);
    void setMusicVolume(float volume);
    void setSFXVolume(float volume);
    float getMasterVolume() const { return m_masterVolume; }
    float getMusicVolume() const { return m_musicVolume; }
    float getSFXVolume() const { return m_sfxVolume; }

    // Mute controls
    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    /**
     * @brief Play a sound effect (one-shot).
     * @param clip Audio clip to play
     * @param volume Volume multiplier (0.0-1.0)
     * @param pitch Pitch multiplier (1.0 = normal)
     * @param loop Whether to loop the sound
     * @return Sound ID for controlling the sound (0 if failed)
     */
    uint32_t playSFX(const std::shared_ptr<AudioClip>& clip, float volume = 1.0f,
                     float pitch = 1.0f, bool loop = false);

    /**
     * @brief Play background music.
     * @param clip Audio clip to play
     * @param volume Volume multiplier (0.0-1.0)
     * @param loop Whether to loop the music
     * @param fadeIn Fade in duration in seconds (0 = no fade)
     * @return Sound ID for controlling the music (0 if failed)
     */
    uint32_t playMusic(const std::shared_ptr<AudioClip>& clip, float volume = 1.0f,
                       bool loop = true, float fadeIn = 0.0f);

    /**
     * @brief Stop a playing sound.
     * @param soundId Sound ID returned by playSFX or playMusic
     * @param fadeOut Fade out duration in seconds (0 = immediate stop)
     */
    void stopSound(uint32_t soundId, float fadeOut = 0.0f);

    /**
     * @brief Stop all sounds.
     */
    void stopAll();

    /**
     * @brief Stop all music.
     */
    void stopAllMusic();

    /**
     * @brief Stop all sound effects.
     */
    void stopAllSFX();

    /**
     * @brief Pause a sound.
     */
    void pauseSound(uint32_t soundId);

    /**
     * @brief Resume a paused sound.
     */
    void resumeSound(uint32_t soundId);

    /**
     * @brief Check if a sound is playing.
     */
    bool isPlaying(uint32_t soundId) const;

    /**
     * @brief Set sound position (for 3D audio).
     * @param soundId Sound ID
     * @param x, y, z 3D position
     */
    void setSoundPosition(uint32_t soundId, float x, float y, float z);

    /**
     * @brief Set listener position (for 3D audio).
     * @param x, y, z 3D position
     */
    void setListenerPosition(float x, float y, float z);

    /**
     * @brief Set listener orientation (for 3D audio).
     * @param forwardX, forwardY, forwardZ Forward direction
     * @param upX, upY, upZ Up direction
     */
    void setListenerOrientation(float forwardX, float forwardY, float forwardZ, float upX,
                                float upY, float upZ);

  private:
    AudioManager() = default;
    ~AudioManager();

    struct SoundInstance {
        ma_sound* sound = nullptr;
        uint32_t id = 0;
        bool isMusic = false;
        std::shared_ptr<AudioClip> clip;
    };

    ma_engine* m_engine = nullptr;
    bool m_initialized = false;

    float m_masterVolume = 1.0f;
    float m_musicVolume = 1.0f;
    float m_sfxVolume = 1.0f;
    bool m_muted = false;

    uint32_t m_nextSoundId = 1;
    std::unordered_map<uint32_t, SoundInstance> m_activeSounds;
};

}  // namespace vde
