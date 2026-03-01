/**
 * @file FileOperations.h
 * @brief Image file I/O and native file dialog wrappers for the Resource Editor.
 *
 * Provides open/save file dialogs, image loading/saving via stb_image,
 * and script file I/O helpers.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vde {
namespace tools {

/**
 * @brief A file type filter for file dialogs, e.g. {"PNG Images", "*.png"}.
 */
struct ImageFileFilter {
    std::string name;     ///< Display name
    std::string pattern;  ///< Semicolon-separated patterns
};

/**
 * @brief Open a native file-open dialog for images.
 * @param title Dialog title
 * @return Selected file path, or empty string if cancelled
 */
std::string openImageFileDialog(const std::string& title = "Open Image");

/**
 * @brief Open a native file-save dialog for images.
 * @param title Dialog title
 * @param defaultExt Default file extension (e.g. "png")
 * @return Selected file path, or empty string if cancelled
 */
std::string saveImageFileDialog(const std::string& title = "Save Image",
                                const std::string& defaultExt = "png");

/**
 * @brief Open a native file-open dialog for script files.
 * @param title Dialog title
 * @return Selected file path, or empty string if cancelled
 */
std::string openScriptFileDialog(const std::string& title = "Open Script");

/**
 * @brief Open a native file-save dialog for script files.
 * @param title Dialog title
 * @return Selected file path, or empty string if cancelled
 */
std::string saveScriptFileDialog(const std::string& title = "Save Script");

// --- Image I/O wrappers ---

/**
 * @brief Load an image file via stb_image (forced RGBA).
 * @param path File path
 * @param outPixels Output pixel data (RGBA)
 * @param outW Output width
 * @param outH Output height
 * @return true if successful
 */
bool loadImageFile(const std::string& path, std::vector<uint8_t>& outPixels, uint32_t& outW,
                   uint32_t& outH);

/**
 * @brief Save pixels as PNG file.
 */
bool saveImagePNG(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h);

/**
 * @brief Save pixels as BMP file.
 */
bool saveImageBMP(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h);

/**
 * @brief Save pixels as TGA file.
 */
bool saveImageTGA(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h);

// --- Script file helpers ---

/**
 * @brief Read lines from a script file, skipping BOM if present.
 */
std::vector<std::string> readScriptLines(const std::string& path);

/**
 * @brief Write lines to a script file.
 */
bool writeScriptLines(const std::string& path, const std::vector<std::string>& lines);

}  // namespace tools
}  // namespace vde
