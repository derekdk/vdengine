#pragma once

/**
 * @file FileOperations.h
 * @brief File dialog and utility functions for the Resource Editor.
 *
 * Wraps platform-native open/save dialogs and provides simple helpers
 * for reading script files and extracting filename stems.
 */

#include <string>
#include <vector>

namespace vde::tools {

/**
 * @brief Static utility class for file I/O and native dialogs.
 */
class FileOperations {
public:
    /**
     * @brief Open a native file dialog for selecting an image to open.
     * @return Selected file path, or empty string if cancelled.
     */
    static std::string openImageDialog();

    /**
     * @brief Open a native save dialog for saving an image.
     * @param defaultName Default filename to suggest.
     * @return Selected file path, or empty string if cancelled.
     */
    static std::string saveImageDialog(const std::string& defaultName = "");

    /**
     * @brief Open a native save dialog for exporting an image.
     * @param defaultName Default filename to suggest.
     * @return Selected file path, or empty string if cancelled.
     */
    static std::string exportImageDialog(const std::string& defaultName = "");

    /**
     * @brief Read a script file, returning non-empty, non-comment lines.
     *
     * Comments (lines starting with '#' or '//') and blank lines are
     * stripped.  Leading/trailing whitespace is trimmed from each line.
     *
     * @param path Path to the script file.
     * @return Vector of command lines.
     */
    static std::vector<std::string> readScriptFile(const std::string& path);

    /**
     * @brief Extract the filename stem (no directory, no extension).
     * @param path Full or relative file path.
     * @return Stem, e.g. "sprite" from "assets/sprite.png".
     */
    static std::string filenameStem(const std::string& path);
};

}  // namespace vde::tools
