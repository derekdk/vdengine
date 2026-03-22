#pragma once

/**
 * @file Sprites.h
 * @brief Procedural sprite/texture generation for the vertical shooter.
 */

#include <vde/Texture.h>
#include <vde/VulkanContext.h>

#include <memory>

#include "Types.h"

namespace shooter {

std::shared_ptr<vde::Texture> createPlayerTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createBulletTexture(vde::VulkanContext* ctx, WeaponType weapon);
std::shared_ptr<vde::Texture> createEnemyTexture(vde::VulkanContext* ctx, EnemyType type);
std::shared_ptr<vde::Texture> createEnemyBulletTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createStarTexture(vde::VulkanContext* ctx);
std::shared_ptr<vde::Texture> createExplosionTexture(vde::VulkanContext* ctx);

}  // namespace shooter
