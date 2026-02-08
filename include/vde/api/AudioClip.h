#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Resource.h"

namespace vde {

/**
 * @brief Audio clip resource for sound effects and music.
 *
 * Represents audio data loaded from a file (WAV, MP3, OGG, FLAC).
 * Supports both in-memory playback and streaming for large files.
 */
class AudioClip : public Resource {
  public:
    /**
     * @brief Audio format information.
     */
    struct Format {
        uint32_t sampleRate = 44100;  ///< Samples per second
        uint32_t channels = 2;        ///< Number of audio channels (1=mono, 2=stereo)
        uint32_t bitsPerSample = 16;  ///< Bits per sample (typically 16 or 32)
    };

    AudioClip() = default;
    virtual ~AudioClip();

    // Resource interface
    bool loadFromFile(const std::string& path);
    const char* getTypeName() const override { return "AudioClip"; }

    /**
     * @brief Get audio format information.
     */
    const Format& getFormat() const { return m_format; }

    /**
     * @brief Get sample count.
     */
    uint64_t getSampleCount() const { return m_sampleCount; }

    /**
     * @brief Get duration in seconds.
     */
    float getDuration() const;

    /**
     * @brief Get raw PCM data.
     * @return Pointer to PCM data (float format, interleaved)
     */
    const float* getData() const { return m_data.data(); }

    /**
     * @brief Get data size in floats.
     */
    size_t getDataSize() const { return m_data.size(); }

    /**
     * @brief Check if this is a streaming clip (for large music files).
     */
    bool isStreaming() const { return m_streaming; }

    /**
     * @brief Set streaming mode.
     * @param streaming If true, audio will be streamed instead of fully loaded
     */
    void setStreaming(bool streaming) { m_streaming = streaming; }

  private:
    Format m_format;
    uint64_t m_sampleCount = 0;
    std::vector<float> m_data;  // PCM data in float format
    bool m_streaming = false;
};

}  // namespace vde
