/**
 * @file FileDialog.cpp
 * @brief Platform-specific native file dialog implementation.
 */

#include "FileDialog.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <codecvt>
#include <locale>

#include <ShObjIdl.h>
#include <Windows.h>
#endif

namespace vde {
namespace tools {

#ifdef _WIN32

/**
 * @brief Convert UTF-8 string to wide string.
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
 * @brief Convert wide string to UTF-8 string.
 */
static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr,
                                   0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}

std::string openFileDialog(const std::string& title, const std::vector<FileFilter>& filters) {
    std::string result;

    // Try to initialize COM.  If the thread already has COM in a different
    // mode (e.g. GLFW initialises MTA), CoInitializeEx returns
    // RPC_E_CHANGED_MODE.  That is fine — COM is usable, we just must not
    // call CoUninitialize for a session we did not start.
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool weInitialisedCom = (hrInit == S_OK || hrInit == S_FALSE);

    // RPC_E_CHANGED_MODE means COM is already initialised with a different
    // threading model.  The dialog still works — just skip the uninit.
    if (FAILED(hrInit) && hrInit != RPC_E_CHANGED_MODE) {
        return result;
    }

    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
                                  reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        // Set title
        std::wstring wTitle = utf8ToWide(title);
        pFileOpen->SetTitle(wTitle.c_str());

        // Set filters
        if (!filters.empty()) {
            std::vector<COMDLG_FILTERSPEC> specs;
            // Keep wide strings alive until dialog closes
            std::vector<std::wstring> wNames;
            std::vector<std::wstring> wPatterns;
            wNames.reserve(filters.size());
            wPatterns.reserve(filters.size());

            for (const auto& f : filters) {
                wNames.push_back(utf8ToWide(f.name));
                wPatterns.push_back(utf8ToWide(f.pattern));
                specs.push_back({wNames.back().c_str(), wPatterns.back().c_str()});
            }

            pFileOpen->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
            pFileOpen->SetFileTypeIndex(1);  // 1-based
        }

        // Show the dialog
        hr = pFileOpen->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
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

        pFileOpen->Release();
    }

    if (weInitialisedCom) {
        CoUninitialize();
    }
    return result;
}

#else

// Stub for non-Windows platforms
std::string openFileDialog(const std::string& /*title*/,
                           const std::vector<FileFilter>& /*filters*/) {
    return {};
}

#endif

}  // namespace tools
}  // namespace vde
