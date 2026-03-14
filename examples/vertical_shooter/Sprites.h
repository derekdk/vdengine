#pragma once

/**
 * @file Sprites.h
 * @brief Procedural sprite/texture generation for the vertical shooter.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>

#include <memory>
#include <string>

#include "Types.h"

namespace shooter {

std::shared_ptr<vde::Texture> createPlayerTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createBulletTexture(vde::VulkanContext* ctx, WeaponType weapon);
std::shared_ptr<vde::Texture> createEnemyTexture(vde::VulkanContext* ctx, EnemyType type);
std::shared_ptr<vde::Texture> createEnemyBulletTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createStarTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createExplosionTexture(vde::VulkanContext* ctx);

/// Render @p text using a built-in 5x7 pixel font.
/// @p pixelScale enlarges each pixel (1 = tiny, 3 = comfortable).
/// RGBA colour is set by r, g, b (alpha always 255 for lit pixels, 0 for background).
std::shared_ptr<vde::Texture> createTextTexture(vde::VulkanContext* ctx, const std::string& text,
                                                int pixelScale, uint8_t r, uint8_t g, uint8_t b);

}  // namespace shooter
