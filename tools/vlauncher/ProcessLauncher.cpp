#include "ProcessLauncher.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace vde::tools {

bool ProcessLauncher::launchDetached(const std::filesystem::path& executablePath,
                                     std::string& error) {
#ifdef _WIN32
    if (!std::filesystem::exists(executablePath)) {
        error = "Executable not found";
        return false;
    }

    std::string commandLine = "\"" + executablePath.string() + "\"";
    std::string workDir = executablePath.parent_path().string();

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION processInfo{};

    BOOL created =
        CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr,
                       workDir.empty() ? nullptr : workDir.c_str(), &startupInfo, &processInfo);

    if (!created) {
        DWORD code = GetLastError();
        error = "CreateProcess failed with code " + std::to_string(code);
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    (void)executablePath;
    error = "Process launching is currently only implemented for Windows";
    return false;
#endif
}

}  // namespace vde::tools
