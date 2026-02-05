#pragma once

/**
 * @file ImageLoader.h
 * @brief Static utility class for loading images from disk
 */

#include <cstddef>
#include <cstdint>
#include <string>

namespace vde {

/**
 * @brief Container for image data loaded from disk.
 */
struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = nullptr;

    /**
     * @brief Calculate the size of the image data in bytes.
     * @return Size in bytes (width * height * channels)
     */
    size_t size() const { return static_cast<size_t>(width) * height * channels; }

    /**
     * @brief Check if the image data is valid.
     * @return true if pixels is not null and dimensions are positive
     */
    bool isValid() const { return pixels != nullptr && width > 0 && height > 0 && channels > 0; }
};

/**
 * @brief Static utility class for loading images from disk.
 *
 * Uses stb_image for loading various image formats (PNG, JPEG, BMP, etc.)
 * All images are loaded as RGBA (4 channels) by default for Vulkan compatibility.
 */
class ImageLoader {
  public:
    /**
     * @brief Load an image from file, forcing RGBA format.
     *
     * @param filepath Path to the image file
     * @return ImageData structure with loaded pixels, or invalid ImageData on failure
     */
    static ImageData load(const std::string& filepath);

    /**
     * @brief Load an image from file with specified channel count.
     *
     * @param filepath Path to the image file
     * @param desiredChannels Number of channels to load (1=grey, 3=RGB, 4=RGBA)
     * @return ImageData structure with loaded pixels, or invalid ImageData on failure
     */
    static ImageData load(const std::string& filepath, int desiredChannels);

    /**
     * @brief Free image data loaded by ImageLoader.
     *
     * After calling this, image.pixels will be null and dimensions will be zero.
     * Safe to call multiple times or on invalid images.
     *
     * @param image ImageData structure to free
     */
    static void free(ImageData& image);

    /**
     * @brief Get the last error message from stb_image.
     * @return Error message string, or empty if no error
     */
    static std::string getLastError();
};

}  // namespace vde
