#pragma once

#include <vde/Texture.h>

#include <array>

namespace vde {
class Game;
}

namespace signalbreak {

struct Artwork {
    std::array<vde::ResourcePtr<vde::Texture>, 2> player;
    std::array<vde::ResourcePtr<vde::Texture>, 2> enemy;
    vde::ResourcePtr<vde::Texture> sky;
    vde::ResourcePtr<vde::Texture> skyline;
    vde::ResourcePtr<vde::Texture> metal;
};

Artwork createArtwork(vde::Game& game);

}  // namespace signalbreak