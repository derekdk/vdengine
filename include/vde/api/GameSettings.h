#pragma once

/**
 * @file GameSettings.h
 * @brief Game configuration settings for VDE
 * 
 * Provides configuration structures for initializing the game
 * engine with various display, graphics, and audio settings.
 */

#include <string>
#include <cstdint>

namespace vde {

/**
 * @brief Graphics quality presets.
 */
enum class GraphicsQuality {
    Low,
    Medium,
    High,
    Ultra,
    Custom
};

/**
 * @brief VSync modes.
 */
enum class VSyncMode {
    Off,           ///< No VSync, uncapped frame rate
    On,            ///< Standard VSync
    Adaptive       ///< Adaptive VSync (if supported)
};

/**
 * @brief Anti-aliasing modes.
 */
enum class AntiAliasing {
    None,
    MSAA2x,
    MSAA4x,
    MSAA8x,
    FXAA,
    TAA
};

/**
 * @brief Configuration for game window and display.
 */
struct DisplaySettings {
    uint32_t windowWidth = 1280;     ///< Window width in pixels
    uint32_t windowHeight = 720;     ///< Window height in pixels
    bool fullscreen = false;          ///< Fullscreen mode
    bool borderless = false;          ///< Borderless windowed mode
    bool resizable = true;            ///< Allow window resizing
    int32_t monitorIndex = -1;        ///< Monitor to use (-1 = primary)
    VSyncMode vsync = VSyncMode::On;  ///< VSync mode
};

/**
 * @brief Configuration for graphics rendering.
 */
struct GraphicsSettings {
    GraphicsQuality quality = GraphicsQuality::Medium;
    AntiAliasing antiAliasing = AntiAliasing::MSAA4x;
    float renderScale = 1.0f;          ///< Internal render resolution scale
    bool shadows = true;               ///< Enable shadows
    int shadowMapSize = 2048;          ///< Shadow map resolution
    bool bloom = true;                 ///< Enable bloom effect
    bool ambientOcclusion = true;      ///< Enable ambient occlusion
    int maxFPS = 0;                    ///< Max frame rate (0 = unlimited)
};

/**
 * @brief Configuration for audio.
 */
struct AudioSettings {
    float masterVolume = 1.0f;        ///< Master volume (0.0 - 1.0)
    float musicVolume = 1.0f;         ///< Music volume (0.0 - 1.0)
    float sfxVolume = 1.0f;           ///< Sound effects volume (0.0 - 1.0)
    float voiceVolume = 1.0f;         ///< Voice/dialogue volume (0.0 - 1.0)
    bool muted = false;               ///< Mute all audio
};

/**
 * @brief Configuration for debug features.
 */
struct DebugSettings {
    bool enableValidation = false;    ///< Enable Vulkan validation layers
    bool showFPS = false;             ///< Show FPS counter
    bool showStats = false;           ///< Show performance stats
    bool wireframe = false;           ///< Wireframe rendering
    bool logPerformance = false;      ///< Log performance metrics
};

/**
 * @brief Complete game configuration.
 * 
 * This structure contains all settings needed to initialize the game.
 * 
 * @example
 * @code
 * vde::GameSettings settings;
 * settings.gameName = "My Awesome Game";
 * settings.display.windowWidth = 1920;
 * settings.display.windowHeight = 1080;
 * settings.display.fullscreen = true;
 * settings.graphics.quality = vde::GraphicsQuality::High;
 * 
 * vde::Game game;
 * game.initialize(settings);
 * @endcode
 */
struct GameSettings {
    // Basic info
    std::string gameName = "VDE Game";    ///< Game/window title
    std::string gameVersion = "1.0.0";    ///< Game version string
    
    // Subsystem settings
    DisplaySettings display;
    GraphicsSettings graphics;
    AudioSettings audio;
    DebugSettings debug;

    // Convenience setters for common configurations
    
    /**
     * @brief Set the window size.
     */
    GameSettings& setWindowSize(uint32_t width, uint32_t height) {
        display.windowWidth = width;
        display.windowHeight = height;
        return *this;
    }

    /**
     * @brief Set fullscreen mode.
     */
    GameSettings& setFullscreen(bool fs) {
        display.fullscreen = fs;
        return *this;
    }

    /**
     * @brief Set graphics quality preset.
     */
    GameSettings& setQuality(GraphicsQuality quality) {
        graphics.quality = quality;
        
        // Apply quality preset defaults
        switch (quality) {
            case GraphicsQuality::Low:
                graphics.antiAliasing = AntiAliasing::None;
                graphics.shadows = false;
                graphics.bloom = false;
                graphics.ambientOcclusion = false;
                graphics.shadowMapSize = 512;
                break;
            case GraphicsQuality::Medium:
                graphics.antiAliasing = AntiAliasing::MSAA2x;
                graphics.shadows = true;
                graphics.bloom = false;
                graphics.ambientOcclusion = false;
                graphics.shadowMapSize = 1024;
                break;
            case GraphicsQuality::High:
                graphics.antiAliasing = AntiAliasing::MSAA4x;
                graphics.shadows = true;
                graphics.bloom = true;
                graphics.ambientOcclusion = true;
                graphics.shadowMapSize = 2048;
                break;
            case GraphicsQuality::Ultra:
                graphics.antiAliasing = AntiAliasing::MSAA8x;
                graphics.shadows = true;
                graphics.bloom = true;
                graphics.ambientOcclusion = true;
                graphics.shadowMapSize = 4096;
                break;
            case GraphicsQuality::Custom:
                // Don't change anything for custom
                break;
        }
        return *this;
    }

    /**
     * @brief Enable debug features.
     */
    GameSettings& enableDebug(bool validation = true, bool fps = true) {
        debug.enableValidation = validation;
        debug.showFPS = fps;
        return *this;
    }

    // Legacy compatibility for simple width/height initialization
    uint32_t windowWidth = 1280;   ///< @deprecated Use display.windowWidth
    uint32_t windowHeight = 720;   ///< @deprecated Use display.windowHeight
    bool fullscreen = false;       ///< @deprecated Use display.fullscreen
};

} // namespace vde
