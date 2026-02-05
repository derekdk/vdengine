#include "vde/api/AudioSource.h"

#include "vde/api/AudioManager.h"

namespace vde {

AudioSource::~AudioSource() {
    if (m_soundId != 0) {
        AudioManager::getInstance().stopSound(m_soundId);
    }
}

void AudioSource::play(bool loop) {
    if (!m_clip) {
        return;
    }

    // Stop current sound if playing
    if (m_soundId != 0) {
        AudioManager::getInstance().stopSound(m_soundId);
        m_soundId = 0;
    }

    m_loop = loop;

    // Play the sound
    m_soundId = AudioManager::getInstance().playSFX(m_clip, m_volume, m_pitch, m_loop);

    // Update spatial properties if needed
    if (m_spatial && m_soundId != 0) {
        updatePosition();
    }
}

void AudioSource::stop(float fadeOut) {
    if (m_soundId != 0) {
        AudioManager::getInstance().stopSound(m_soundId, fadeOut);
        m_soundId = 0;
    }
}

void AudioSource::pause() {
    if (m_soundId != 0) {
        AudioManager::getInstance().pauseSound(m_soundId);
    }
}

void AudioSource::resume() {
    if (m_soundId != 0) {
        AudioManager::getInstance().resumeSound(m_soundId);
    }
}

bool AudioSource::isPlaying() const {
    if (m_soundId == 0) {
        return false;
    }
    return AudioManager::getInstance().isPlaying(m_soundId);
}

void AudioSource::setPosition(const glm::vec3& position) {
    m_position = position;
    updatePosition();
}

void AudioSource::setPosition(float x, float y, float z) {
    m_position = glm::vec3(x, y, z);
    updatePosition();
}

void AudioSource::updateVolume() {
    // Volume changes will apply to next playback
    // For active sounds, we'd need to track and update them
}

void AudioSource::updatePitch() {
    // Pitch changes will apply to next playback
}

void AudioSource::updatePosition() {
    if (m_spatial && m_soundId != 0) {
        AudioManager::getInstance().setSoundPosition(m_soundId, m_position.x, m_position.y,
                                                     m_position.z);
    }
}

}  // namespace vde
