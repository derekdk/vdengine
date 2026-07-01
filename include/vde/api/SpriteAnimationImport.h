#pragma once

/**
 * @file SpriteAnimationImport.h
 * @brief Import helpers for SpriteSheet and SpriteAnimation data.
 */

#include <vde/Texture.h>
#include <vde/api/SpriteAnimation.h>
#include <vde/api/SpriteSheet.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Imported atlas and animation clips.
 */
struct ImportedSpriteAnimationSet {
    SpriteSheet::Ref spriteSheet;
    std::unordered_map<std::string, SpriteAnimation> animations;
    std::vector<std::string> frameNames;
};

/**
 * @brief Import SpriteSheet and SpriteAnimation data from external metadata.
 *
 * The initial implementation supports Aseprite JSON exports. Texture loading
 * remains separate: callers provide an already loaded Texture that matches the
 * exported atlas image.
 */
class SpriteAnimationImport {
  public:
    /**
     * @brief Import Aseprite JSON metadata from a string.
     * @param texture Atlas texture matching the exported metadata.
     * @param jsonText Aseprite JSON export text.
     * @return Imported atlas plus any named animation clips from frame tags.
     */
    static ImportedSpriteAnimationSet importAsepriteJson(std::shared_ptr<Texture> texture,
                                                         const std::string& jsonText);

    /**
     * @brief Import Aseprite JSON metadata from a file.
     * @param texture Atlas texture matching the exported metadata.
     * @param jsonPath Path to the exported Aseprite JSON metadata file.
     * @return Imported atlas plus any named animation clips from frame tags.
     */
    static ImportedSpriteAnimationSet importAsepriteJsonFile(std::shared_ptr<Texture> texture,
                                                             const std::string& jsonPath);
};

}  // namespace vde