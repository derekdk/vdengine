/**
 * @file FileOperations.cpp
 * @brief Platform-specific file dialogs and utility functions.
 */

#include "FileOperations.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <ShObjIdl.h>
#include <Windows.h>
#endif

namespace vde {
namespace tools {

// =============================================================================
// Platform helpers (Windows)
// =============================================================================

#ifdef _WIN32

/**
 * @brief Convert a UTF-8 string to a wide string.
 */
static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) {
        return {};
    }
    int size =
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), result.data(), size);
    return result;
}

/**
 * @brief Convert a wide string to a UTF-8 string.
 */
static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                                   nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}

/**
 * @brief Ensure COM is initialised for the current thread, tolerating
 *        RPC_E_CHANGED_MODE if another library (e.g. GLFW) already did it.
 * @return true if WE initialised COM and should call CoUninitialize.
 */
static bool ensureCom() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    return (hr == S_OK || hr == S_FALSE);
}

// Image file filter specs (shared by all dialogs)
static const COMDLG_FILTERSPEC kImageFilters[] = {
    {L"PNG Images", L"*.png"},
    {L"BMP Images", L"*.bmp"},
    {L"TGA Images", L"*.tga"},
    {L"All Image Files", L"*.png;*.bmp;*.tga"},
    {L"All Files", L"*.*"},
};
static constexpr UINT kImageFilterCount =
    static_cast<UINT>(sizeof(kImageFilters) / sizeof(kImageFilters[0]));

std::string FileOperations::openImageDialog() {
    std::string result;
    bool weInitCom = ensureCom();

    IFileOpenDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pDialog));
    if (SUCCEEDED(hr)) {
        pDialog->SetTitle(L"Open Image");
        pDialog->SetFileTypes(kImageFilterCount, kImageFilters);
        pDialog->SetFileTypeIndex(4);  // Default to "All Image Files"

        hr = pDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr)) {
                    result = wideToUtf8(pszPath);
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }

    if (weInitCom) {
        CoUninitialize();
    }
    return result;
}

/**
 * @brief Internal helper for save/export dialogs (they differ only in title).
 */
static std::string showSaveDialog(const wchar_t* title, const std::string& defaultName) {
    std::string result;
    bool weInitCom = ensureCom();

    IFileSaveDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileSaveDialog, reinterpret_cast<void**>(&pDialog));
    if (SUCCEEDED(hr)) {
        pDialog->SetTitle(title);
        pDialog->SetFileTypes(kImageFilterCount, kImageFilters);
        pDialog->SetFileTypeIndex(1);  // Default to PNG
        pDialog->SetDefaultExtension(L"png");

        if (!defaultName.empty()) {
            std::wstring wName = utf8ToWide(defaultName);
            pDialog->SetFileName(wName.c_str());
        }

        hr = pDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr)) {
                    result = wideToUtf8(pszPath);
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }

    if (weInitCom) {
        CoUninitialize();
    }
    return result;
}

std::string FileOperations::saveImageDialog(const std::string& defaultName) {
    return showSaveDialog(L"Save Image", defaultName);
}

std::string FileOperations::exportImageDialog(const std::string& defaultName) {
    return showSaveDialog(L"Export Image", defaultName);
}

#else

// ---- Non-Windows stubs ----

std::string FileOperations::openImageDialog() {
    return {};
}

std::string FileOperations::saveImageDialog(const std::string& /*defaultName*/) {
    return {};
}

std::string FileOperations::exportImageDialog(const std::string& /*defaultName*/) {
    return {};
}

#endif  // _WIN32

// =============================================================================
// Script file reading
// =============================================================================

std::vector<std::string> FileOperations::readScriptFile(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open()) {
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Trim leading whitespace
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;  // Empty / whitespace-only line
        }
        line = line.substr(start);

        // Trim trailing whitespace
        auto end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        // Skip comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') {
            continue;
        }

        lines.push_back(line);
    }
    return lines;
}

// =============================================================================
// Path utilities
// =============================================================================

std::string FileOperations::filenameStem(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

}  // namespace tools
}  // namespace vde
