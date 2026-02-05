/**
 * @file audio_demo/main.cpp
 * @brief Audio system demonstration
 *
 * This example demonstrates the VDE audio system capabilities:
 * - Background music playback with volume control
 * - Sound effects triggered by key presses
 * - 3D spatial audio with moving sound source
 * - Master, music, and SFX volume controls
 *
 * Controls:
 * - M: Play/stop background music
 * - SPACE: Play sound effect
 * - 1-3: Adjust master volume (1=50%, 2=75%, 3=100%)
 * - 4-6: Adjust music volume (4=50%, 5=75%, 6=100%)
 * - 7-9: Adjust SFX volume (7=50%, 8=75%, 9=100%)
 * - U: Mute/unmute audio
 * - ESC: Exit
 *
 * Setup Instructions:
 * 1. Place audio files in examples/audio_demo/assets/:
 *    - music.wav or music.mp3 (background music)
 *    - beep.wav or beep.mp3 (sound effect)
 * 2. Run the example
 * 3. Use controls above to test audio features
 */

#include <vde/api/AudioManager.h>
#include <vde/api/GameAPI.h>
#include <vde/api/KeyCodes.h>

#include <filesystem>
#include <iostream>

using namespace vde;

class AudioDemoScene : public Scene {
  public:
    void onEnter() override {
        std::cout << "\n=== VDE Audio Demo ===\n";
        std::cout << "Controls:\n";
        std::cout << "  M: Play/stop background music\n";
        std::cout << "  SPACE: Play sound effect\n";
        std::cout << "  1-3: Master volume (50%, 75%, 100%)\n";
        std::cout << "  4-6: Music volume (50%, 75%, 100%)\n";
        std::cout << "  7-9: SFX volume (50%, 75%, 100%)\n";
        std::cout << "  U: Mute/unmute\n";
        std::cout << "  ESC: Exit\n\n";

        // Try to load audio clips
        loadAudioAssets();

        // Create a visual cube that pulses with audio
        auto cube = std::make_shared<MeshEntity>();
        cube->setPosition(0.0f, 0.0f, 0.0f);
        cube->setScale(2.0f);
        cube->setMesh(Mesh::createCube());
        cube->setName("AudioCube");

        // Set a nice material
        auto material = Material::createColored(Color{0.3f, 0.6f, 1.0f, 1.0f});
        material->setEmissionIntensity(0.2f);
        cube->setMaterial(material);

        addEntity(cube);
        m_audioCube = cube.get();

        // Create a moving sound source visualization
        auto soundSource = std::make_shared<MeshEntity>();
        soundSource->setMesh(Mesh::createSphere(0.3f, 16, 16));
        soundSource->setPosition(2.0f, 0.0f, 0.0f);
        soundSource->setName("SoundSource");

        auto soundMaterial = Material::createEmissive(Color{1.0f, 0.8f, 0.2f, 1.0f}, 2.0f);
        soundSource->setMaterial(soundMaterial);

        addEntity(soundSource);
        m_soundSourceEntity = soundSource.get();

        // Set up camera
        auto camera = std::make_unique<OrbitCamera>(Position{0.0f, 0.0f, 0.0f},
                                                    8.0f,   // distance
                                                    20.0f,  // pitch
                                                    0.0f    // yaw
        );
        setCamera(std::move(camera));

        // Set up lighting
        auto lightBox = std::make_unique<ThreePointLightBox>();
        setLightBox(std::move(lightBox));

        setBackgroundColor(Color{0.1f, 0.1f, 0.15f, 1.0f});

        printAudioStatus();
    }

    void onExit() override {
        // Stop all audio on exit
        AudioManager::getInstance().stopAll();
    }

    void update(float deltaTime) override {
        Scene::update(deltaTime);

        m_time += deltaTime;

        // Rotate the audio cube
        if (m_audioCube) {
            m_audioCube->setRotation(m_time * 20.0f,  // pitch
                                     m_time * 30.0f,  // yaw
                                     m_time * 10.0f   // roll
            );

            // Scale cube based on whether music is playing
            float scale = m_isMusicPlaying ? (2.0f + 0.3f * std::sin(m_time * 3.0f)) : 2.0f;
            m_audioCube->setScale(scale);
        }

        // Move sound source in a circle for spatial audio demo
        if (m_soundSourceEntity) {
            float radius = 3.0f;
            float angle = m_time * 0.5f;
            m_soundSourceEntity->setPosition(radius * std::cos(angle), std::sin(m_time * 2.0f),
                                             radius * std::sin(angle));

            // Update 3D audio position if spatial sound is playing
            if (m_spatialSoundId != 0) {
                auto pos = m_soundSourceEntity->getTransform().position;
                AudioManager::getInstance().setSoundPosition(m_spatialSoundId, pos.x, pos.y, pos.z);
            }
        }

        // Rotate camera
        if (auto* orbitCam = dynamic_cast<OrbitCamera*>(getCamera())) {
            orbitCam->rotate(deltaTime * 5.0f, 0.0f);
        }
    }

    void handleKey(int key, int action) {
        if (action != KEY_PRESS)
            return;

        auto& audio = AudioManager::getInstance();

        if (key == KEY_ESCAPE) {
            getGame()->quit();
        } else if (key == KEY_M) {
            toggleMusic();
        } else if (key == KEY_SPACE) {
            playSoundEffect();
        } else if (key == KEY_S) {
            playSpatialSound();
        } else if (key == KEY_U) {
            audio.setMuted(!audio.isMuted());
            std::cout << "Audio " << (audio.isMuted() ? "muted" : "unmuted") << "\n";
        } else if (key == KEY_1) {
            audio.setMasterVolume(0.5f);
            printAudioStatus();
        } else if (key == KEY_2) {
            audio.setMasterVolume(0.75f);
            printAudioStatus();
        } else if (key == KEY_3) {
            audio.setMasterVolume(1.0f);
            printAudioStatus();
        } else if (key == KEY_4) {
            audio.setMusicVolume(0.5f);
            printAudioStatus();
        } else if (key == KEY_5) {
            audio.setMusicVolume(0.75f);
            printAudioStatus();
        } else if (key == KEY_6) {
            audio.setMusicVolume(1.0f);
            printAudioStatus();
        } else if (key == KEY_7) {
            audio.setSFXVolume(0.5f);
            printAudioStatus();
        } else if (key == KEY_8) {
            audio.setSFXVolume(0.75f);
            printAudioStatus();
        } else if (key == KEY_9) {
            audio.setSFXVolume(1.0f);
            printAudioStatus();
        }
    }

  private:
    void loadAudioAssets() {
        namespace fs = std::filesystem;

        // Try multiple possible asset locations
        std::vector<std::string> possibleDirs = {
            "assets/",                              // Next to executable
            "../assets/",                           // One level up
            "examples/audio_demo/assets/",          // From build root
            "../examples/audio_demo/assets/",       // From Debug folder
            "../../examples/audio_demo/assets/",    // From build/examples/Debug
            "../../../examples/audio_demo/assets/"  // Extra level
        };

        std::string assetsDir;
        for (const auto& dir : possibleDirs) {
            if (fs::exists(dir)) {
                assetsDir = dir;
                std::cout << "Found assets directory: " << assetsDir << "\n";
                break;
            }
        }

        if (assetsDir.empty()) {
            std::cout << "Could not find assets directory. Tried:\n";
            for (const auto& dir : possibleDirs) {
                std::cout << "  - " << fs::absolute(dir) << "\n";
            }
            assetsDir = "assets/";  // Default fallback
        }

        // Try to load music
        std::vector<std::string> musicFiles = {assetsDir + "music.wav", assetsDir + "music.mp3",
                                               assetsDir + "music.ogg"};

        for (const auto& path : musicFiles) {
            if (fs::exists(path)) {
                m_musicClip = std::make_shared<AudioClip>();
                m_musicClip->setStreaming(true);  // Set streaming BEFORE loading
                if (m_musicClip->loadFromFile(path)) {
                    std::cout << "Loaded music: " << path << "\n";
                    break;
                } else {
                    m_musicClip.reset();
                }
            }
        }

        // Try to load sound effect
        std::vector<std::string> sfxFiles = {assetsDir + "beep.wav", assetsDir + "beep.mp3",
                                             assetsDir + "beep.ogg", assetsDir + "click.wav",
                                             assetsDir + "click.mp3"};

        for (const auto& path : sfxFiles) {
            if (fs::exists(path)) {
                m_sfxClip = std::make_shared<AudioClip>();
                if (m_sfxClip->loadFromFile(path)) {
                    std::cout << "Loaded SFX: " << path << "\n";
                    break;
                } else {
                    m_sfxClip.reset();
                }
            }
        }

        if (!m_musicClip && !m_sfxClip) {
            std::cout << "\nNo audio files found in " << assetsDir << "\n";
            std::cout << "Please add music.wav/mp3/ogg and beep.wav/mp3 for full demo.\n";
            std::cout << "(Demo will still run, showing visual elements only)\n\n";
        }
    }

    void toggleMusic() {
        if (m_musicClip == nullptr) {
            std::cout << "No music file loaded\n";
            return;
        }

        auto& audio = AudioManager::getInstance();

        if (m_isMusicPlaying && m_musicSoundId != 0) {
            audio.stopSound(m_musicSoundId, 1.0f);  // 1 second fade out
            m_musicSoundId = 0;
            m_isMusicPlaying = false;
            std::cout << "Music stopped\n";
        } else {
            m_musicSoundId = audio.playMusic(m_musicClip, 1.0f, true, 0.0f);  // No fade for testing
            m_isMusicPlaying = true;
            std::cout << "Music playing (ID: " << m_musicSoundId << ")\n";
        }
    }

    void playSoundEffect() {
        if (m_sfxClip == nullptr) {
            std::cout << "No SFX file loaded\n";
            return;
        }

        auto& audio = AudioManager::getInstance();
        audio.playSFX(m_sfxClip, 1.0f, 1.0f, false);
        std::cout << "Playing sound effect\n";
    }

    void playSpatialSound() {
        if (m_sfxClip == nullptr) {
            std::cout << "No SFX file loaded\n";
            return;
        }

        auto& audio = AudioManager::getInstance();

        // Stop previous spatial sound
        if (m_spatialSoundId != 0) {
            audio.stopSound(m_spatialSoundId);
        }

        // Play new spatial sound
        m_spatialSoundId = audio.playSFX(m_sfxClip, 1.0f, 1.0f, true);  // looping

        if (m_spatialSoundId != 0 && m_soundSourceEntity) {
            auto pos = m_soundSourceEntity->getTransform().position;
            audio.setSoundPosition(m_spatialSoundId, pos.x, pos.y, pos.z);
            std::cout << "Playing spatial sound (moving sphere)\n";
        }
    }

    void printAudioStatus() {
        auto& audio = AudioManager::getInstance();
        std::cout << "Volume - Master: " << int(audio.getMasterVolume() * 100) << "%"
                  << " | Music: " << int(audio.getMusicVolume() * 100) << "%"
                  << " | SFX: " << int(audio.getSFXVolume() * 100) << "%\n";
    }

    MeshEntity* m_audioCube = nullptr;
    MeshEntity* m_soundSourceEntity = nullptr;
    float m_time = 0.0f;

    std::shared_ptr<AudioClip> m_musicClip;
    std::shared_ptr<AudioClip> m_sfxClip;

    uint32_t m_musicSoundId = 0;
    uint32_t m_spatialSoundId = 0;
    bool m_isMusicPlaying = false;
};

int main() {
    try {
        Game game;

        GameSettings settings;
        settings.gameName = "VDE Audio Demo";
        settings.display.windowWidth = 1280;
        settings.display.windowHeight = 720;
        settings.display.vsync = VSyncMode::On;

        // Enable audio with default settings
        settings.audio.masterVolume = 0.8f;
        settings.audio.musicVolume = 0.7f;
        settings.audio.sfxVolume = 1.0f;
        settings.audio.muted = false;

        if (!game.initialize(settings)) {
            std::cerr << "Failed to initialize game\n";
            return 1;
        }

        // Create and add the demo scene
        auto scene = std::make_unique<AudioDemoScene>();
        auto* scenePtr = scene.get();
        game.addScene("demo", std::move(scene));
        game.setActiveScene("demo");

        // Set up input handler - use a class that overrides onKeyPress
        class AudioKeyHandler : public InputHandler {
          public:
            AudioKeyHandler(AudioDemoScene* scene) : m_scene(scene) {}
            void onKeyPress(int key) override { m_scene->handleKey(key, KEY_PRESS); }

          private:
            AudioDemoScene* m_scene;
        };

        auto inputHandler = std::make_unique<AudioKeyHandler>(scenePtr);
        game.setInputHandler(inputHandler.get());

        game.run();
        game.shutdown();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
