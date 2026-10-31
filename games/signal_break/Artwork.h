#pragma once

#include <vde/Texture.h>

#include <array>

namespace vde {
class Game;
}

namespace signalbreak {

// Player frames are indexed [pose][aim]; aims are ordered down, level, up.
// assets/signal_break/player.png lays these out with one row per pose and one column per aim.
inline constexpr int PLAYER_IDLE_POSE = 0;  // 2 frames
inline constexpr int PLAYER_RUN_POSE = 2;   // 4 frames
inline constexpr int PLAYER_JUMP_POSE = 6;
inline constexpr int PLAYER_POSE_COUNT = 7;
inline constexpr int PLAYER_AIM_COUNT = 3;

struct Artwork {
    std::array<std::array<vde::ResourcePtr<vde::Texture>, PLAYER_AIM_COUNT>, PLAYER_POSE_COUNT>
        player;
    std::array<vde::ResourcePtr<vde::Texture>, 2> enemy;
    vde::ResourcePtr<vde::Texture> sky;
    vde::ResourcePtr<vde::Texture> skyline;
    vde::ResourcePtr<vde::Texture> metal;
};

Artwork createArtwork(vde::Game& game);

}  // namespace signalbreak