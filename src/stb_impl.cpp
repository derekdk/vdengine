/**
 * @file stb_impl.cpp
 * @brief STB library implementations.
 *
 * This file contains the implementation defines for stb header-only libraries.
 * Only define the implementation macros in exactly one source file to avoid
 * duplicate symbol errors.
 */

// STB Image - Image loading library
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STB Image Write - Image writing library (for screenshot capture)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// STB TrueType - TrueType font rasterization
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
