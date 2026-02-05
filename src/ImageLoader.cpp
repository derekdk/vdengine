#include <vde/ImageLoader.h>

#include "stb_image.h"

namespace vde {

ImageData ImageLoader::load(const std::string& filepath) {
    // Default to RGBA (4 channels) for Vulkan compatibility
    return load(filepath, 4);
}

ImageData ImageLoader::load(const std::string& filepath, int desiredChannels) {
    ImageData image;

    // Load image with stb_image
    image.pixels = stbi_load(filepath.c_str(), &image.width, &image.height,
                             &image.channels,  // Original channels in file (output)
                             desiredChannels   // Force this many channels
    );

    if (!image.pixels) {
        image.width = 0;
        image.height = 0;
        image.channels = 0;
        return image;
    }

    // Update channels to reflect what we actually loaded
    image.channels = desiredChannels;

    return image;
}

void ImageLoader::free(ImageData& image) {
    if (image.pixels) {
        stbi_image_free(image.pixels);
        image.pixels = nullptr;
    }
    image.width = 0;
    image.height = 0;
    image.channels = 0;
}

std::string ImageLoader::getLastError() {
    const char* reason = stbi_failure_reason();
    return reason ? std::string(reason) : std::string();
}

}  // namespace vde
