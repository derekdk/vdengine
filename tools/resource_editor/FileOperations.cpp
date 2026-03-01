/**
 * @file FileOperations.cpp
 * @brief Implementation of file I/O and native file dialogs for the Resource Editor.
 */

#include "FileOperations.h"

#include <fstream>

#include <stb_image.h>
#include <stb_image_write.h>

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
// Platform helpers
// =============================================================================

#ifdef _WIN32

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty())
        return {};
    int size =
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), result.data(), size);
    return result;
}

static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr,
                                   0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}

/**
 * @brief Internal helper to show a Windows file dialog.
 * @param isSave true for save dialog, false for open dialog
 * @param title Dialog title
 * @param filters File type filters
 * @param defaultExt Default extension for save dialogs (empty = none)
 * @return Selected file path, or empty if cancelled
 */
static std::string showFileDialog(bool isSave, const std::string& title,
                                  const std::vector<ImageFileFilter>& filters,
                                  const std::string& defaultExt = "") {
    std::string result;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool weInitialisedCom = (hrInit == S_OK || hrInit == S_FALSE);

    if (FAILED(hrInit) && hrInit != RPC_E_CHANGED_MODE) {
        return result;
    }

    IFileDialog* pDialog = nullptr;
    HRESULT hr;

    if (isSave) {
        hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_IFileSaveDialog,
                              reinterpret_cast<void**>(&pDialog));
    } else {
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
                              reinterpret_cast<void**>(&pDialog));
    }

    if (SUCCEEDED(hr)) {
        std::wstring wTitle = utf8ToWide(title);
        pDialog->SetTitle(wTitle.c_str());

        if (!filters.empty()) {
            std::vector<COMDLG_FILTERSPEC> specs;
            std::vector<std::wstring> wNames;
            std::vector<std::wstring> wPatterns;
            wNames.reserve(filters.size());
            wPatterns.reserve(filters.size());

            for (const auto& f : filters) {
                wNames.push_back(utf8ToWide(f.name));
                wPatterns.push_back(utf8ToWide(f.pattern));
                specs.push_back({wNames.back().c_str(), wPatterns.back().c_str()});
            }

            pDialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
            pDialog->SetFileTypeIndex(1);
        }

        if (isSave && !defaultExt.empty()) {
            std::wstring wExt = utf8ToWide(defaultExt);
            pDialog->SetDefaultExtension(wExt.c_str());
        }

        hr = pDialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    result = wideToUtf8(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }

        pDialog->Release();
    }

    if (weInitialisedCom) {
        CoUninitialize();
    }
    return result;
}

#endif  // _WIN32

// =============================================================================
// File dialogs
// =============================================================================

std::string openImageFileDialog(const std::string& title) {
#ifdef _WIN32
    std::vector<ImageFileFilter> filters = {
        {"Image Files", "*.png;*.bmp;*.tga;*.jpg;*.jpeg"},
        {"PNG Files", "*.png"},
        {"BMP Files", "*.bmp"},
        {"TGA Files", "*.tga"},
        {"All Files", "*.*"},
    };
    return showFileDialog(false, title, filters);
#else
    (void)title;
    return {};
#endif
}

std::string saveImageFileDialog(const std::string& title, const std::string& defaultExt) {
#ifdef _WIN32
    std::vector<ImageFileFilter> filters = {
        {"PNG Files", "*.png"},
        {"BMP Files", "*.bmp"},
        {"TGA Files", "*.tga"},
        {"All Files", "*.*"},
    };
    return showFileDialog(true, title, filters, defaultExt);
#else
    (void)title;
    (void)defaultExt;
    return {};
#endif
}

std::string openScriptFileDialog(const std::string& title) {
#ifdef _WIN32
    std::vector<ImageFileFilter> filters = {
        {"Script Files", "*.txt;*.script"},
        {"All Files", "*.*"},
    };
    return showFileDialog(false, title, filters);
#else
    (void)title;
    return {};
#endif
}

std::string saveScriptFileDialog(const std::string& title) {
#ifdef _WIN32
    std::vector<ImageFileFilter> filters = {
        {"Script Files", "*.txt;*.script"},
        {"All Files", "*.*"},
    };
    return showFileDialog(true, title, filters, "txt");
#else
    (void)title;
    return {};
#endif
}

// =============================================================================
// Image I/O
// =============================================================================

bool loadImageFile(const std::string& path, std::vector<uint8_t>& outPixels, uint32_t& outW,
                   uint32_t& outH) {
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data)
        return false;

    outW = static_cast<uint32_t>(w);
    outH = static_cast<uint32_t>(h);
    size_t dataSize = static_cast<size_t>(w) * h * 4;
    outPixels.assign(data, data + dataSize);
    stbi_image_free(data);

    return true;
}

bool saveImagePNG(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h) {
    int stride = static_cast<int>(w) * 4;
    return stbi_write_png(path.c_str(), static_cast<int>(w), static_cast<int>(h), 4, pixels,
                          stride) != 0;
}

bool saveImageBMP(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h) {
    return stbi_write_bmp(path.c_str(), static_cast<int>(w), static_cast<int>(h), 4, pixels) != 0;
}

bool saveImageTGA(const std::string& path, const uint8_t* pixels, uint32_t w, uint32_t h) {
    return stbi_write_tga(path.c_str(), static_cast<int>(w), static_cast<int>(h), 4, pixels) != 0;
}

// =============================================================================
// Script file helpers
// =============================================================================

std::vector<std::string> readScriptLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open())
        return lines;

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        // Strip BOM from first line
        if (firstLine) {
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }
            firstLine = false;
        }
        lines.push_back(line);
    }

    return lines;
}

bool writeScriptLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream file(path);
    if (!file.is_open())
        return false;

    for (const auto& line : lines) {
        file << line << "\n";
    }

    return true;
}

}  // namespace tools
}  // namespace vde
