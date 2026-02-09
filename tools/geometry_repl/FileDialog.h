/**
 * @file FileDialog.h
 * @brief Native file open/save dialog for VDE tools.
 *
 * Uses the Windows COM IFileOpenDialog on Windows.
 * Provides a simple cross-platform-ready interface that can be
 * extended to other platforms later (e.g. GTK on Linux).
 */

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace vde {
namespace tools {

/**
 * @brief A file type filter for file dialogs, e.g. {"OBJ Files", "*.obj"}.
 */
struct FileFilter {
    std::string name;     ///< Display name, e.g. "OBJ Files"
    std::string pattern;  ///< Semicolon-separated patterns, e.g. "*.obj;*.OBJ"
};

/**
 * @brief Open a native file-open dialog.
 *
 * @param title Dialog title
 * @param filters File type filters (empty = all files)
 * @return Selected file path, or empty string if cancelled
 */
std::string openFileDialog(const std::string& title, const std::vector<FileFilter>& filters = {});

}  // namespace tools
}  // namespace vde
