// Simple miniaudio test to verify audio output works
#define MINIAUDIO_IMPLEMENTATION
#include <chrono>
#include <iostream>
#include <thread>

#include "build/_deps/glfw-src/deps/miniaudio.h"

int main(int argc, char** argv) {
    std::cout << "Testing miniaudio engine..." << std::endl;

    ma_engine engine;
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        std::cout << "Failed to initialize engine: " << result << std::endl;
        return -1;
    }

    std::cout << "Engine initialized successfully!" << std::endl;
    std::cout << "Engine volume: " << ma_engine_get_volume(&engine) << std::endl;

    if (argc < 2) {
        std::cout << "Usage: test_audio <audio_file.mp3>" << std::endl;
        ma_engine_uninit(&engine);
        return 0;
    }

    std::cout << "Playing: " << argv[1] << std::endl;

    ma_sound sound;
    result = ma_sound_init_from_file(&engine, argv[1], MA_SOUND_FLAG_STREAM, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        std::cout << "Failed to load sound: " << result << std::endl;
        ma_engine_uninit(&engine);
        return -1;
    }

    std::cout << "Sound loaded successfully!" << std::endl;
    ma_sound_set_volume(&sound, 1.0f);
    ma_sound_set_looping(&sound, MA_FALSE);

    result = ma_sound_start(&sound);
    std::cout << "Sound start result: " << result << std::endl;
    std::cout << "Is playing: " << ma_sound_is_playing(&sound) << std::endl;
    std::cout << "At end: " << ma_sound_at_end(&sound) << std::endl;

    std::cout << "Playing for 5 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "Is still playing: " << ma_sound_is_playing(&sound) << std::endl;
    std::cout << "At end: " << ma_sound_at_end(&sound) << std::endl;

    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);

    std::cout << "Test complete!" << std::endl;
    return 0;
}
