#pragma once

/**
 * @file AudioEvent.h
 * @brief Audio event types for per-scene audio queuing
 *
 * Provides the AudioEvent struct used by Scene to queue audio
 * operations that are drained during the Audio phase of the
 * scheduler.  This decouples game logic from direct
 * AudioManager calls, enabling future thread-safe batching.
 */

#include <cstdint>
#include <memory>
#include <string>

namespace vde {

// Forward declarations
class AudioClip;

/**
 * @brief Types of audio events that can be queued.
 */
enum class AudioEventType : uint8_t {
    PlaySFX,      ///< Play a one-shot sound effect
    PlaySFXAt,    ///< Play a positional sound effect (3D)
    PlayMusic,    ///< Play background music
    StopSound,    ///< Stop a specific sound by ID
    StopAll,      ///< Stop all sounds
    PauseSound,   ///< Pause a specific sound
    ResumeSound,  ///< Resume a paused sound
    SetVolume     ///< Change volume of a playing sound (reserved)
};

/**
 * @brief Describes a single audio action to be processed during the Audio phase.
 *
 * Audio events are queued from game logic (GameLogic phase) and
 * drained by the default `Scene::updateAudio()` implementation
 * during the Audio phase of the scheduler.
 *
 * @example
 * @code
 * // In a scene's updateGameLogic():
 * AudioEvent evt;
 * evt.type = AudioEventType::PlaySFX;
 * evt.clip = myClip;
 * evt.volume = 0.8f;
 * queueAudioEvent(evt);
 *
 * // Or use the convenience helper:
 * playSFX(myClip, 0.75f);
 * @endcode
 */
struct AudioEvent {
    AudioEventType type = AudioEventType::PlaySFX;  ///< Event type
    std::shared_ptr<AudioClip> clip;                ///< Audio clip (for Play* events)
    float volume = 1.0f;                            ///< Volume multiplier (0.0 – 1.0)
    float pitch = 1.0f;                             ///< Pitch multiplier (1.0 = normal)
    bool loop = false;                              ///< Whether to loop the sound
    float x = 0.0f;                                 ///< 3D position X (for PlaySFXAt)
    float y = 0.0f;                                 ///< 3D position Y (for PlaySFXAt)
    float z = 0.0f;                                 ///< 3D position Z (for PlaySFXAt)
    uint32_t soundId = 0;                           ///< Sound ID (for Stop/Pause/Resume)
    float fadeTime = 0.0f;                          ///< Fade duration in seconds (for music/stop)

    // ---------------------------------------------------------------
    // Factory helpers
    // ---------------------------------------------------------------

    /**
     * @brief Create a PlaySFX event.
     * @param clip Audio clip to play
     * @param volume Volume multiplier
     * @param pitch Pitch multiplier
     * @param loop Whether to loop
     * @return Configured AudioEvent
     */
    static AudioEvent playSFX(std::shared_ptr<AudioClip> clip, float volume = 1.0f,
                              float pitch = 1.0f, bool loop = false) {
        AudioEvent evt;
        evt.type = AudioEventType::PlaySFX;
        evt.clip = std::move(clip);
        evt.volume = volume;
        evt.pitch = pitch;
        evt.loop = loop;
        return evt;
    }

    /**
     * @brief Create a PlaySFXAt event (positional audio).
     * @param clip Audio clip to play
     * @param posX X position
     * @param posY Y position
     * @param posZ Z position
     * @param volume Volume multiplier
     * @param pitch Pitch multiplier
     * @return Configured AudioEvent
     */
    static AudioEvent playSFXAt(std::shared_ptr<AudioClip> clip, float posX, float posY, float posZ,
                                float volume = 1.0f, float pitch = 1.0f) {
        AudioEvent evt;
        evt.type = AudioEventType::PlaySFXAt;
        evt.clip = std::move(clip);
        evt.x = posX;
        evt.y = posY;
        evt.z = posZ;
        evt.volume = volume;
        evt.pitch = pitch;
        return evt;
    }

    /**
     * @brief Create a PlayMusic event.
     * @param clip Audio clip to play
     * @param volume Volume multiplier
     * @param loop Whether to loop (default: true)
     * @param fadeIn Fade in duration in seconds
     * @return Configured AudioEvent
     */
    static AudioEvent playMusic(std::shared_ptr<AudioClip> clip, float volume = 1.0f,
                                bool loop = true, float fadeIn = 0.0f) {
        AudioEvent evt;
        evt.type = AudioEventType::PlayMusic;
        evt.clip = std::move(clip);
        evt.volume = volume;
        evt.loop = loop;
        evt.fadeTime = fadeIn;
        return evt;
    }

    /**
     * @brief Create a StopSound event.
     * @param id Sound ID to stop
     * @param fadeOut Fade out duration in seconds
     * @return Configured AudioEvent
     */
    static AudioEvent stopSound(uint32_t id, float fadeOut = 0.0f) {
        AudioEvent evt;
        evt.type = AudioEventType::StopSound;
        evt.soundId = id;
        evt.fadeTime = fadeOut;
        return evt;
    }

    /**
     * @brief Create a StopAll event.
     * @return Configured AudioEvent
     */
    static AudioEvent stopAll() {
        AudioEvent evt;
        evt.type = AudioEventType::StopAll;
        return evt;
    }

    /**
     * @brief Create a PauseSound event.
     * @param id Sound ID to pause
     * @return Configured AudioEvent
     */
    static AudioEvent pauseSound(uint32_t id) {
        AudioEvent evt;
        evt.type = AudioEventType::PauseSound;
        evt.soundId = id;
        return evt;
    }

    /**
     * @brief Create a ResumeSound event.
     * @param id Sound ID to resume
     * @return Configured AudioEvent
     */
    static AudioEvent resumeSound(uint32_t id) {
        AudioEvent evt;
        evt.type = AudioEventType::ResumeSound;
        evt.soundId = id;
        return evt;
    }
};

}  // namespace vde
