#include "vde/api/AudioClip.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include <miniaudio.h>

namespace vde {

AudioClip::~AudioClip() {
    m_data.clear();
}

bool AudioClip::loadFromFile(const std::string& path) {
    m_path = path;

    std::cout << "AudioClip: Loading file: " << path << std::endl;

    // Decode the audio file using miniaudio
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);

    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
        std::cout << "AudioClip: Failed to initialize decoder for: " << path << std::endl;
        return false;
    }

    // Get format information
    m_format.sampleRate = decoder.outputSampleRate;
    m_format.channels = decoder.outputChannels;
    m_format.bitsPerSample = 32;  // We're using f32 format

    // Get total frame count
    ma_uint64 frameCount;
    ma_result result = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return false;
    }

    m_sampleCount = frameCount * m_format.channels;

    std::cout << "AudioClip: Format - " << m_format.channels << " channels, " << m_format.sampleRate
              << " Hz, " << frameCount << " frames, "
              << "streaming: " << (m_streaming ? "yes" : "no") << std::endl;

    // If streaming, we don't load data into memory - miniaudio will stream from file
    if (!m_streaming) {
        // Allocate buffer for PCM data
        m_data.resize(m_sampleCount);

        // Read all PCM frames (miniaudio ma_decoder_read_pcm_frames signature)
        void* pFramesOut = m_data.data();
        ma_uint64 framesToRead = frameCount;
        ma_uint64 framesRead;
        result = ma_decoder_read_pcm_frames(&decoder, pFramesOut, framesToRead, &framesRead);

        if (result != MA_SUCCESS || framesRead != frameCount) {
            ma_decoder_uninit(&decoder);
            m_data.clear();
            return false;
        }
    }

    ma_decoder_uninit(&decoder);

    m_loaded = true;
    return true;
}

float AudioClip::getDuration() const {
    if (m_format.sampleRate == 0 || m_format.channels == 0) {
        return 0.0f;
    }
    return static_cast<float>(m_sampleCount) / (m_format.sampleRate * m_format.channels);
}

}  // namespace vde
