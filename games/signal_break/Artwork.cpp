#include "Artwork.h"

#include <vde/api/Game.h>
#include <vde/api/ResourceManager.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace signalbreak {
namespace {

constexpr const char* ASSET_DIR = "assets/signal_break";

// Sheets are authored at native pixel size; enlarging them keeps linear filtering crisp.
constexpr uint32_t SPRITE_SCALE = 4;

vde::Texture readImage(const std::string& file) {
    const std::string path = (std::filesystem::path(ASSET_DIR) / file).string();
    vde::Texture image;
    if (!image.loadFromFile(path)) {
        throw std::runtime_error("Unable to load Signal Break artwork: " + path);
    }
    return image;
}

vde::ResourcePtr<vde::Texture> upload(vde::Game& game, const std::string& name,
                                      const vde::ResourcePtr<vde::Texture>& texture) {
    if (!texture->uploadToGPU(game.getVulkanContext())) {
        throw std::runtime_error("Unable to upload Signal Break artwork: " + name);
    }
    game.getResourceManager().add<vde::Texture>("signal_break/" + name, texture);
    return texture;
}

vde::ResourcePtr<vde::Texture> loadImage(vde::Game& game, const std::string& name) {
    return upload(game, name, std::make_shared<vde::Texture>(readImage(name + ".png")));
}

// Returns the frames of an evenly divided sheet in row-major order.
std::vector<vde::ResourcePtr<vde::Texture>> loadSheet(vde::Game& game, const std::string& name,
                                                      uint32_t columns, uint32_t rows) {
    const vde::Texture sheet = readImage(name + ".png");
    if (sheet.getWidth() % columns != 0 || sheet.getHeight() % rows != 0) {
        throw std::runtime_error("Signal Break artwork " + name + ".png must be a grid of " +
                                 std::to_string(columns) + " x " + std::to_string(rows) +
                                 " equally sized frames");
    }
    const uint32_t frameWidth = sheet.getWidth() / columns;
    const uint32_t frameHeight = sheet.getHeight() / rows;
    const uint32_t width = frameWidth * SPRITE_SCALE;
    const uint32_t height = frameHeight * SPRITE_SCALE;
    const uint8_t* source = sheet.getPixelData();
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    std::vector<vde::ResourcePtr<vde::Texture>> frames;
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t column = 0; column < columns; ++column) {
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    const size_t sourceX = column * frameWidth + x / SPRITE_SCALE;
                    const size_t sourceY = row * frameHeight + y / SPRITE_SCALE;
                    std::memcpy(&pixels[(static_cast<size_t>(y) * width + x) * 4],
                                source + (sourceY * sheet.getWidth() + sourceX) * 4, 4);
                }
            }
            auto frame = std::make_shared<vde::Texture>();
            frame->loadFromData(pixels.data(), width, height);
            frames.push_back(upload(game, name + std::to_string(frames.size()), frame));
        }
    }
    return frames;
}

}  // namespace

Artwork createArtwork(vde::Game& game) {
    Artwork art;
    const auto player = loadSheet(game, "player", PLAYER_AIM_COUNT, PLAYER_POSE_COUNT);
    for (size_t pose = 0; pose < art.player.size(); ++pose) {
        for (size_t aim = 0; aim < art.player[pose].size(); ++aim) {
            art.player[pose][aim] = player[pose * PLAYER_AIM_COUNT + aim];
        }
    }
    const auto enemy = loadSheet(game, "enemy", static_cast<uint32_t>(art.enemy.size()), 1);
    std::ranges::copy(enemy, art.enemy.begin());
    art.sky = loadImage(game, "sky");
    art.skyline = loadImage(game, "skyline");
    art.metal = loadImage(game, "metal");
    return art;
}

}  // namespace signalbreak
