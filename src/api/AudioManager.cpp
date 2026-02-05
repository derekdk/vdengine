#include "vde/api/AudioManager.h"

#include <algorithm>
#include <iostream>

#include "vde/api/AudioClip.h"
#include "vde/api/GameSettings.h"
#include <miniaudio.h>

namespace vde {

AudioManager::~AudioManager() {
    shutdown();
}

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::initialize(const AudioSettings& settings) {
    if (m_initialized) {
        return true;
    }

    // Create miniaudio engine
    m_engine = new ma_engine();

    ma_engine_config config = ma_engine_config_init();
    config.noAutoStart = MA_FALSE;

    if (ma_engine_init(&config, m_engine) != MA_SUCCESS) {
        delete m_engine;
        m_engine = nullptr;
        std::cerr << "Failed to initialize audio engine" << std::endl;
        return false;
    }

    std::cout << "AudioManager: Engine initialized successfully" << std::endl;
    std::cout << "AudioManager: Engine volume: " << ma_engine_get_volume(m_engine) << std::endl;

    // Apply settings
    setMasterVolume(settings.masterVolume);
    setMusicVolume(settings.musicVolume);
    setSFXVolume(settings.sfxVolume);
    setMuted(settings.muted);

    std::cout << "AudioManager: After settings - Master: " << m_masterVolume
              << ", Music: " << m_musicVolume << ", SFX: " << m_sfxVolume << ", Muted: " << m_muted
              << std::endl;

    m_initialized = true;
    return true;
}

void AudioManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Stop all sounds
    stopAll();

    // Clean up active sounds
    for (auto& [id, instance] : m_activeSounds) {
        if (instance.sound) {
            ma_sound_uninit(instance.sound);
            delete instance.sound;
        }
    }
    m_activeSounds.clear();

    // Uninitialize engine
    if (m_engine) {
        ma_engine_uninit(m_engine);
        delete m_engine;
        m_engine = nullptr;
    }

    m_initialized = false;
}

void AudioManager::update([[maybe_unused]] float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Clean up finished sounds
    std::vector<uint32_t> toRemove;
    for (const auto& [id, instance] : m_activeSounds) {
        if (instance.sound && !ma_sound_is_playing(instance.sound)) {
            toRemove.push_back(id);
        }
    }

    for (uint32_t id : toRemove) {
        auto it = m_activeSounds.find(id);
        if (it != m_activeSounds.end()) {
            if (it->second.sound) {
                ma_sound_uninit(it->second.sound);
                delete it->second.sound;
            }
            m_activeSounds.erase(it);
        }
    }
}

void AudioManager::setMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_engine) {
        ma_engine_set_volume(m_engine, m_masterVolume);
    }
}

void AudioManager::setMusicVolume(float volume) {
    m_musicVolume = std::clamp(volume, 0.0f, 1.0f);

    // Update all active music sounds
    for (const auto& [id, instance] : m_activeSounds) {
        if (instance.isMusic && instance.sound) {
            ma_sound_set_volume(instance.sound, m_musicVolume);
        }
    }
}

void AudioManager::setSFXVolume(float volume) {
    m_sfxVolume = std::clamp(volume, 0.0f, 1.0f);

    // Update all active SFX sounds
    for (const auto& [id, instance] : m_activeSounds) {
        if (!instance.isMusic && instance.sound) {
            ma_sound_set_volume(instance.sound, m_sfxVolume);
        }
    }
}

void AudioManager::setMuted(bool muted) {
    m_muted = muted;
    if (m_engine) {
        if (muted) {
            ma_engine_set_volume(m_engine, 0.0f);
        } else {
            ma_engine_set_volume(m_engine, m_masterVolume);
        }
    }
}

uint32_t AudioManager::playSFX(const std::shared_ptr<AudioClip>& clip, float volume, float pitch,
                               bool loop) {
    if (!m_initialized || !clip || !clip->isLoaded()) {
        return 0;
    }

    // Create a new sound
    ma_sound* sound = new ma_sound();
    ma_uint32 flags = 0;
    if (loop) {
        flags |= MA_SOUND_FLAG_STREAM;
    }

    // Initialize from data decoder
    ma_decoder_config decoderConfig = ma_decoder_config_init(
        ma_format_f32, clip->getFormat().channels, clip->getFormat().sampleRate);

    ma_result result = ma_sound_init_from_file(m_engine, clip->getPath().c_str(), flags,
                                               nullptr,  // Group (nullptr = default)
                                               nullptr,  // Fence
                                               sound);

    if (result != MA_SUCCESS) {
        delete sound;
        return 0;
    }

    // Set properties
    ma_sound_set_volume(sound, volume * m_sfxVolume);
    ma_sound_set_pitch(sound, pitch);
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);

    // Start playing
    ma_sound_start(sound);

    // Track sound
    uint32_t soundId = m_nextSoundId++;
    SoundInstance instance;
    instance.sound = sound;
    instance.id = soundId;
    instance.isMusic = false;
    instance.clip = clip;
    m_activeSounds[soundId] = instance;

    return soundId;
}

uint32_t AudioManager::playMusic(const std::shared_ptr<AudioClip>& clip, float volume, bool loop,
                                 float fadeIn) {
    if (!m_initialized || !clip || !clip->isLoaded()) {
        std::cout << "AudioManager::playMusic failed - initialized: " << m_initialized
                  << ", clip: " << (clip != nullptr)
                  << ", loaded: " << (clip ? clip->isLoaded() : false) << std::endl;
        return 0;
    }

    std::cout << "AudioManager::playMusic - File: " << clip->getPath() << ", volume: " << volume
              << ", musicVolume: " << m_musicVolume << ", loop: " << loop << std::endl;

    // Create a new sound
    ma_sound* sound = new ma_sound();
    ma_uint32 flags = MA_SOUND_FLAG_STREAM;  // Always stream music

    ma_result result =
        ma_sound_init_from_file(m_engine, clip->getPath().c_str(), flags, nullptr, nullptr, sound);

    if (result != MA_SUCCESS) {
        std::cout << "AudioManager::playMusic - Failed to init sound from file, error code: "
                  << result << std::endl;
        delete sound;
        return 0;
    }

    std::cout << "AudioManager::playMusic - Sound initialized successfully" << std::endl;

    // Set properties
    ma_sound_set_volume(sound, volume * m_musicVolume);
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);

    // Apply fade in if requested
    if (fadeIn > 0.0f) {
        ma_sound_set_volume(sound, 0.0f);
        ma_sound_set_fade_in_milliseconds(sound, 0.0f, volume * m_musicVolume,
                                          static_cast<ma_uint64>(fadeIn * 1000));
    }

    // Start playing
    ma_result startResult = ma_sound_start(sound);
    std::cout << "AudioManager::playMusic - Sound start result: " << startResult
              << ", is playing: " << ma_sound_is_playing(sound) << std::endl;

    // Track sound
    uint32_t soundId = m_nextSoundId++;
    SoundInstance instance;
    instance.sound = sound;
    instance.id = soundId;
    instance.isMusic = true;
    instance.clip = clip;
    m_activeSounds[soundId] = instance;

    return soundId;
}

void AudioManager::stopSound(uint32_t soundId, float fadeOut) {
    auto it = m_activeSounds.find(soundId);
    if (it == m_activeSounds.end()) {
        return;
    }

    if (it->second.sound) {
        if (fadeOut > 0.0f) {
            // ma_sound_get_volume doesn't exist in miniaudio
            // Use the volume we set or default to 1.0
            float currentVolume = it->second.isMusic ? m_musicVolume : m_sfxVolume;
            ma_sound_set_fade_in_milliseconds(it->second.sound, currentVolume, 0.0f,
                                              static_cast<ma_uint64>(fadeOut * 1000));
        } else {
            ma_sound_stop(it->second.sound);
        }
    }
}

void AudioManager::stopAll() {
    for (auto& [id, instance] : m_activeSounds) {
        if (instance.sound) {
            ma_sound_stop(instance.sound);
        }
    }
}

void AudioManager::stopAllMusic() {
    for (auto& [id, instance] : m_activeSounds) {
        if (instance.isMusic && instance.sound) {
            ma_sound_stop(instance.sound);
        }
    }
}

void AudioManager::stopAllSFX() {
    for (auto& [id, instance] : m_activeSounds) {
        if (!instance.isMusic && instance.sound) {
            ma_sound_stop(instance.sound);
        }
    }
}

void AudioManager::pauseSound(uint32_t soundId) {
    auto it = m_activeSounds.find(soundId);
    if (it != m_activeSounds.end() && it->second.sound) {
        ma_sound_stop(it->second.sound);
    }
}

void AudioManager::resumeSound(uint32_t soundId) {
    auto it = m_activeSounds.find(soundId);
    if (it != m_activeSounds.end() && it->second.sound) {
        ma_sound_start(it->second.sound);
    }
}

bool AudioManager::isPlaying(uint32_t soundId) const {
    auto it = m_activeSounds.find(soundId);
    if (it != m_activeSounds.end() && it->second.sound) {
        return ma_sound_is_playing(it->second.sound);
    }
    return false;
}

void AudioManager::setSoundPosition(uint32_t soundId, float x, float y, float z) {
    auto it = m_activeSounds.find(soundId);
    if (it != m_activeSounds.end() && it->second.sound) {
        ma_sound_set_position(it->second.sound, x, y, z);
    }
}

void AudioManager::setListenerPosition(float x, float y, float z) {
    if (m_engine) {
        ma_engine_listener_set_position(m_engine, 0, x, y, z);
    }
}

void AudioManager::setListenerOrientation(float forwardX, float forwardY, float forwardZ, float upX,
                                          float upY, float upZ) {
    if (m_engine) {
        ma_engine_listener_set_direction(m_engine, 0, forwardX, forwardY, forwardZ);
        ma_engine_listener_set_world_up(m_engine, 0, upX, upY, upZ);
    }
}

}  // namespace vde
