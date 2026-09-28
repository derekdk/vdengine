#pragma once

#include <glm/glm.hpp>

#include <memory>

#include "AudioClip.h"

namespace vde {

/**
 * @brief Audio source component for entities.
 *
 * Represents a sound emitter in 3D space. Can be attached to entities
 * to play spatial audio that responds to listener position.
 */
class AudioSource {
  public:
    AudioSource() = default;
    ~AudioSource();

    /**
     * @brief Set the audio clip to play.
     */
    void setClip(const std::shared_ptr<AudioClip>& clip) { m_clip = clip; }

    /**
     * @brief Get the current audio clip.
     */
    [[nodiscard]] const std::shared_ptr<AudioClip>& getClip() const { return m_clip; }

    /**
     * @brief Play the audio clip.
     * @param loop Whether to loop the audio
     */
    void play(bool loop = false);

    /**
     * @brief Stop playback.
     * @param fadeOut Fade out duration in seconds
     */
    void stop(float fadeOut = 0.0f);

    /**
     * @brief Pause playback.
     */
    void pause();

    /**
     * @brief Resume playback.
     */
    void resume();

    /**
     * @brief Check if currently playing.
     */
    [[nodiscard]] bool isPlaying() const;

    // Volume control
    void setVolume(float volume) {
        m_volume = volume;
        updateVolume();
    }
    [[nodiscard]] float getVolume() const { return m_volume; }

    // Pitch control
    void setPitch(float pitch) {
        m_pitch = pitch;
        updatePitch();
    }
    [[nodiscard]] float getPitch() const { return m_pitch; }

    // Spatial audio
    void setPosition(const glm::vec3& position);
    void setPosition(float x, float y, float z);
    [[nodiscard]] const glm::vec3& getPosition() const { return m_position; }

    void setSpatial(bool spatial) { m_spatial = spatial; }
    [[nodiscard]] bool isSpatial() const { return m_spatial; }

    void setMinDistance(float distance) { m_minDistance = distance; }
    [[nodiscard]] float getMinDistance() const { return m_minDistance; }

    void setMaxDistance(float distance) { m_maxDistance = distance; }
    [[nodiscard]] float getMaxDistance() const { return m_maxDistance; }

    void setAttenuation(float attenuation) { m_attenuation = attenuation; }
    [[nodiscard]] float getAttenuation() const { return m_attenuation; }

    // Playback control
    void setPlayOnAwake(bool play) { m_playOnAwake = play; }
    [[nodiscard]] bool getPlayOnAwake() const { return m_playOnAwake; }

    void setLoop(bool loop) { m_loop = loop; }
    [[nodiscard]] bool isLooping() const { return m_loop; }

  private:
    void updateVolume();
    void updatePitch();
    void updatePosition();

    std::shared_ptr<AudioClip> m_clip;
    uint32_t m_soundId = 0;

    float m_volume = 1.0f;
    float m_pitch = 1.0f;
    glm::vec3 m_position{0.0f};

    bool m_spatial = false;
    float m_minDistance = 1.0f;
    float m_maxDistance = 100.0f;
    float m_attenuation = 1.0f;

    bool m_playOnAwake = false;
    bool m_loop = false;
};

}  // namespace vde
