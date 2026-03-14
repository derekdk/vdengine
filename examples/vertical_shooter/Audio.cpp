/**
 * @file Audio.cpp
 * @brief Procedural WAV sound-effect generation.
 *
 * Each sound is synthesized as 16-bit PCM mono, written to a temporary WAV file,
 * and loaded back via AudioClip::loadFromFile().
 */

#include "Audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace shooter {

// ============================================================================
// WAV writer
// ============================================================================

static bool writeWav(const std::string& path, const std::vector<float>& samples,
                     uint32_t sampleRate = 44100) {
    uint32_t numSamples = static_cast<uint32_t>(samples.size());
    uint16_t channels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    uint16_t blockAlign = channels * bitsPerSample / 8;
    uint32_t dataSize = numSamples * blockAlign;
    uint32_t fileSize = 36 + dataSize;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;

    // RIFF header
    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&fileSize), 4);
    f.write("WAVE", 4);

    // fmt chunk
    f.write("fmt ", 4);
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;  // PCM
    f.write(reinterpret_cast<const char*>(&fmtSize), 4);
    f.write(reinterpret_cast<const char*>(&audioFormat), 2);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    f.write(reinterpret_cast<const char*>(&sampleRate), 4);
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    f.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataSize), 4);

    for (float s : samples) {
        float clamped = std::max(-1.0f, std::min(1.0f, s));
        int16_t pcm = static_cast<int16_t>(clamped * 32767.0f);
        f.write(reinterpret_cast<const char*>(&pcm), 2);
    }

    return f.good();
}

// ============================================================================
// Synthesizers
// ============================================================================

static std::vector<float> synthLaser(float duration, float startFreq, float endFreq,
                                     float volume = 0.4f) {
    constexpr uint32_t RATE = 44100;
    auto count = static_cast<uint32_t>(duration * RATE);
    std::vector<float> out(count);
    float phase = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / count;
        float freq = startFreq + (endFreq - startFreq) * t;
        float env = (1.0f - t);
        phase += freq / RATE;
        out[i] = std::sin(2.0f * static_cast<float>(M_PI) * phase) * env * volume;
    }
    return out;
}

static std::vector<float> synthNoise(float duration, float volume = 0.3f) {
    constexpr uint32_t RATE = 44100;
    auto count = static_cast<uint32_t>(duration * RATE);
    std::vector<float> out(count);
    uint32_t state = 12345;
    for (uint32_t i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / count;
        float env = (1.0f - t) * (1.0f - t);
        // Simple LCG noise
        state = state * 1103515245u + 12345u;
        float noise = (static_cast<float>(state & 0xFFFF) / 32768.0f) - 1.0f;
        out[i] = noise * env * volume;
    }
    return out;
}

static std::vector<float> synthClick(float duration, float freq, float volume = 0.3f) {
    constexpr uint32_t RATE = 44100;
    auto count = static_cast<uint32_t>(duration * RATE);
    std::vector<float> out(count);
    for (uint32_t i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / count;
        float env = (1.0f - t);
        float wave = std::sin(2.0f * static_cast<float>(M_PI) * freq * t * duration) * env * volume;
        out[i] = wave;
    }
    return out;
}

// ============================================================================
// Public API
// ============================================================================

static std::shared_ptr<vde::AudioClip> loadGenerated(const std::string& path,
                                                     const std::vector<float>& samples) {
    if (!writeWav(path, samples)) {
        std::cerr << "Audio: failed to write " << path << std::endl;
        return nullptr;
    }
    auto clip = std::make_shared<vde::AudioClip>();
    if (!clip->loadFromFile(path)) {
        std::cerr << "Audio: failed to load " << path << std::endl;
        return nullptr;
    }
    return clip;
}

SoundBank generateSoundBank(const std::string& tempDir) {
    std::filesystem::create_directories(tempDir);
    SoundBank bank;

    bank.shoot = loadGenerated(tempDir + "/shoot.wav", synthLaser(0.12f, 880.0f, 440.0f, 0.35f));

    bank.spreadShoot =
        loadGenerated(tempDir + "/spread.wav", synthLaser(0.15f, 660.0f, 330.0f, 0.30f));

    bank.rapidShoot =
        loadGenerated(tempDir + "/rapid.wav", synthLaser(0.06f, 1200.0f, 800.0f, 0.25f));

    bank.explosion = loadGenerated(tempDir + "/explosion.wav", synthNoise(0.35f, 0.5f));

    bank.hit = loadGenerated(tempDir + "/hit.wav", synthNoise(0.10f, 0.35f));

    bank.weaponSwitch = loadGenerated(tempDir + "/switch.wav", synthClick(0.08f, 1400.0f, 0.25f));

    return bank;
}

}  // namespace shooter
