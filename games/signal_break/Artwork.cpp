#include "Artwork.h"

#include <vde/api/Game.h>
#include <vde/api/ResourceManager.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace signalbreak {
namespace {

void pixel(std::vector<uint8_t>& pixels, int width, int column, int row, uint32_t rgb,
           uint8_t alpha = 255) {
    const size_t offset = (static_cast<size_t>(row) * width + column) * 4;
    pixels[offset] = static_cast<uint8_t>(rgb >> 16);
    pixels[offset + 1] = static_cast<uint8_t>(rgb >> 8);
    pixels[offset + 2] = static_cast<uint8_t>(rgb);
    pixels[offset + 3] = alpha;
}

vde::ResourcePtr<vde::Texture> upload(vde::Game& game, const std::string& name,
                                      const std::vector<uint8_t>& pixels, uint32_t width,
                                      uint32_t height) {
    auto texture = std::make_shared<vde::Texture>();
    if (!texture->loadFromData(pixels.data(), width, height) ||
        !texture->uploadToGPU(game.getVulkanContext())) {
        throw std::runtime_error("Unable to create Signal Break artwork: " + name);
    }
    game.getResourceManager().add<vde::Texture>("signal_break/" + name, texture);
    return texture;
}

vde::ResourcePtr<vde::Texture> character(vde::Game& game, bool hostile, int frame) {
    constexpr std::array<std::string_view, 20> rows{
        "................", ".....OOOOO......", "....OAAAAAO.....", "....OAVVVVO.....",
        "....OAFFFFO.....", ".....OFFFO......", "....OOOOOO......", "...OAAAAAAO.....",
        "..OABBAAAAOOOO..", "..OABBFFFFGGGGO.", "..OABBAAAAGGGGO.", "...OAAAAAAOOOO..",
        "...OAAAAAAO.....", "....OOOOOO......", "....OAAOAAO.....", "....OAAOAAO.....",
        "....OAAOAAO.....", "....OGGOGGO.....", "...OGGGOGGGO....", "................",
    };
    constexpr int width = 16;
    constexpr int height = 20;
    constexpr int scale = 4;
    std::vector<uint8_t> pixels(width * height * scale * scale * 4, 0);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            int sourceColumn = column;
            if (frame == 1 && row >= 15) {
                sourceColumn += column < 8 ? 1 : -1;
            }
            const char value = rows[row][std::clamp(sourceColumn, 0, width - 1)];
            uint32_t color = 0;
            switch (value) {
            case 'O':
                color = 0x152a31;
                break;
            case 'A':
                color = hostile ? 0xec655b : 0x43d7c6;
                break;
            case 'B':
                color = hostile ? 0x993c4a : 0x208a88;
                break;
            case 'V':
                color = 0xffdc72;
                break;
            case 'F':
                color = 0xf4debc;
                break;
            case 'G':
                color = 0xdbe8e2;
                break;
            default:
                break;
            }
            for (int offsetY = 0; offsetY < scale; ++offsetY) {
                for (int offsetX = 0; offsetX < scale; ++offsetX) {
                    pixel(pixels, width * scale, column * scale + offsetX, row * scale + offsetY,
                          color, value == '.' ? 0 : 255);
                }
            }
        }
    }
    return upload(game, std::string(hostile ? "enemy" : "player") + std::to_string(frame), pixels,
                  width * scale, height * scale);
}

}  // namespace

Artwork createArtwork(vde::Game& game) {
    Artwork art;
    for (int frame = 0; frame < 2; ++frame) {
        art.player[frame] = character(game, false, frame);
        art.enemy[frame] = character(game, true, frame);
    }
    constexpr int skyWidth = 256;
    constexpr int skyHeight = 128;
    std::vector<uint8_t> sky(skyWidth * skyHeight * 4, 0);
    for (int row = 0; row < skyHeight; ++row) {
        for (int column = 0; column < skyWidth; ++column) {
            uint32_t color = row < 48 ? 0xaecbc9 : (row < 86 ? 0xc5d9cd : 0xe3e4c6);
            if ((row >= 22 && row < 27 && column > 26 && column < 98) ||
                (row >= 40 && row < 46 && column > 153 && column < 235) ||
                (row >= 65 && row < 68 && column > 78 && column < 155)) {
                color = 0xf6efe0;
            }
            pixel(sky, skyWidth, column, row, color);
        }
    }
    art.sky = upload(game, "sky", sky, skyWidth, skyHeight);
    constexpr int cityWidth = 256;
    constexpr int cityHeight = 128;
    std::vector<uint8_t> skyline(cityWidth * cityHeight * 4, 0);
    for (int column = 0; column < cityWidth; ++column) {
        const int block = column / 19;
        const int roof = 35 + ((block * 37) % 60);
        for (int row = roof; row < cityHeight; ++row) {
            const bool litWindow = (column % 9 < 2) && (row % 13 < 3) && (row > roof + 5);
            pixel(skyline, cityWidth, column, row, litWindow ? 0xc3cdb3 : 0x78978f);
        }
    }
    art.skyline = upload(game, "skyline", skyline, cityWidth, cityHeight);
    std::vector<uint8_t> metal(64 * 16 * 4, 0);
    for (int row = 0; row < 16; ++row) {
        for (int column = 0; column < 64; ++column) {
            uint32_t color = row < 3 ? 0x94b3a4 : (row > 12 ? 0x1c2f35 : 0x3c5356);
            if (column % 16 == 0) {
                color = 0x182d33;
            }
            if ((row == 5 || row == 10) && column % 16 == 3) {
                color = 0xb7c8bb;
            }
            pixel(metal, 64, column, row, color);
        }
    }
    art.metal = upload(game, "metal", metal, 64, 16);
    return art;
}

}  // namespace signalbreak