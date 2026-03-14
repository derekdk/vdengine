#pragma once

/**
 * @file Audio.h
 * @brief Procedural sound effect generation for the vertical shooter.
 */

#include <vde/api/AudioClip.h>

#include <memory>
#include <string>

namespace shooter {

struct SoundBank {
    std::shared_ptr<vde::AudioClip> shoot;
    std::shared_ptr<vde::AudioClip> spreadShoot;
    std::shared_ptr<vde::AudioClip> rapidShoot;
    std::shared_ptr<vde::AudioClip> explosion;
    std::shared_ptr<vde::AudioClip> hit;
    std::shared_ptr<vde::AudioClip> weaponSwitch;
};

/**
 * @brief Generate all procedural sound effects and return a populated SoundBank.
 *
 * Writes temporary WAV files to the given directory, then loads them via AudioClip.
 */
SoundBank generateSoundBank(const std::string& tempDir);

}  // namespace shooter
