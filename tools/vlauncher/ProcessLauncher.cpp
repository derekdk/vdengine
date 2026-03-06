#include "ProcessLauncher.h"

#include <array>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vde::tools {

namespace {

constexpr size_t kMaxOutputReadBytes = 8 * 1024 * 1024;

std::filesystem::path makeTempOutputPath() {
#ifdef _WIN32
    std::error_code error;
    auto tempDir = std::filesystem::temp_directory_path(error);
    if (error || tempDir.empty()) {
        return {};
    }

    auto nowTicks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string fileName = "vde_vlauncher_" + std::to_string(GetCurrentProcessId()) + "_" +
                           std::to_string(nowTicks) + ".log";
    return tempDir / fileName;
#else
    return {};
#endif
}

}  // namespace

bool ProcessLauncher::launchDetached(const std::filesystem::path& executablePath,
                                     std::string& error) {
#ifdef _WIN32
    if (!std::filesystem::exists(executablePath)) {
        error = "Executable not found";
        return false;
    }

    std::string commandLine = "\"" + executablePath.string() + "\"";
    std::vector<char> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back('\0');
    std::string workDir = executablePath.parent_path().string();

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(STARTUPINFOA);

    PROCESS_INFORMATION processInfo{};

    BOOL created =
        CreateProcessA(nullptr, commandLineBuffer.data(), nullptr, nullptr, FALSE,
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

bool ProcessLauncher::launchWithOutputCapture(const std::filesystem::path& executablePath,
                                              LaunchedProcess& launchedProcess,
                                              std::string& error) {
#ifdef _WIN32
    launchedProcess = {};

    if (!std::filesystem::exists(executablePath)) {
        error = "Executable not found";
        return false;
    }

    const std::filesystem::path outputPath = makeTempOutputPath();
    if (outputPath.empty()) {
        error = "Failed to determine temp output path";
        return false;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.bInheritHandle = TRUE;

    HANDLE outputFileHandle =
        CreateFileA(outputPath.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (outputFileHandle == INVALID_HANDLE_VALUE) {
        error = "Failed to create temp output file";
        return false;
    }

    std::string commandLine = "\"" + executablePath.string() + "\"";
    std::vector<char> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back('\0');

    std::string workDir = executablePath.parent_path().string();

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(STARTUPINFOA);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = outputFileHandle;
    startupInfo.hStdError = outputFileHandle;

    PROCESS_INFORMATION processInfo{};

    BOOL created =
        CreateProcessA(nullptr, commandLineBuffer.data(), nullptr, nullptr,
                       TRUE,  // inherit stdout/stderr handle
                       CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr,
                       workDir.empty() ? nullptr : workDir.c_str(), &startupInfo, &processInfo);

    CloseHandle(outputFileHandle);

    if (!created) {
        DWORD code = GetLastError();
        std::error_code removeError;
        std::filesystem::remove(outputPath, removeError);
        error = "CreateProcess failed with code " + std::to_string(code);
        return false;
    }

    CloseHandle(processInfo.hThread);

    launchedProcess.processHandle = reinterpret_cast<std::uintptr_t>(processInfo.hProcess);
    launchedProcess.outputPath = outputPath;
    launchedProcess.commandLine = commandLine;
    return true;
#else
    (void)executablePath;
    (void)launchedProcess;
    error = "Process launching is currently only implemented for Windows";
    return false;
#endif
}

bool ProcessLauncher::pollCompletion(const LaunchedProcess& launchedProcess, bool& completed,
                                     uint32_t& exitCode, std::string& error) {
#ifdef _WIN32
    completed = false;
    exitCode = 0;

    if (launchedProcess.processHandle == 0) {
        error = "Invalid process handle";
        return false;
    }

    HANDLE processHandle = reinterpret_cast<HANDLE>(launchedProcess.processHandle);
    DWORD waitResult = WaitForSingleObject(processHandle, 0);
    if (waitResult == WAIT_TIMEOUT) {
        return true;
    }

    if (waitResult != WAIT_OBJECT_0) {
        error = "WaitForSingleObject failed";
        return false;
    }

    DWORD winExitCode = 0;
    if (!GetExitCodeProcess(processHandle, &winExitCode)) {
        error = "GetExitCodeProcess failed";
        return false;
    }

    completed = true;
    exitCode = static_cast<uint32_t>(winExitCode);
    return true;
#else
    (void)launchedProcess;
    (void)completed;
    (void)exitCode;
    error = "Process polling is currently only implemented for Windows";
    return false;
#endif
}

void ProcessLauncher::release(LaunchedProcess& launchedProcess) {
#ifdef _WIN32
    if (launchedProcess.processHandle != 0) {
        HANDLE processHandle = reinterpret_cast<HANDLE>(launchedProcess.processHandle);
        CloseHandle(processHandle);
        launchedProcess.processHandle = 0;
    }
#else
    (void)launchedProcess;
#endif
}

bool ProcessLauncher::readOutputFile(const std::filesystem::path& outputPath, std::string& output,
                                     std::string& error) {
    std::ifstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        error = "Failed to open output log file";
        return false;
    }

    output.clear();
    output.reserve(32 * 1024);

    std::array<char, 4096> buffer{};
    size_t totalRead = 0;

    while (file.good()) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize readCount = file.gcount();
        if (readCount <= 0) {
            break;
        }

        const size_t bytesRead = static_cast<size_t>(readCount);
        const size_t remaining =
            (kMaxOutputReadBytes > totalRead) ? (kMaxOutputReadBytes - totalRead) : 0;
        const size_t toAppend = (bytesRead < remaining) ? bytesRead : remaining;

        if (toAppend > 0) {
            output.append(buffer.data(), toAppend);
            totalRead += toAppend;
        }

        if (totalRead >= kMaxOutputReadBytes) {
            output += "\n\n[vlauncher] Output truncated while reading temporary log file.\n";
            break;
        }
    }

    return true;
}

bool ProcessLauncher::openFileInVSCode(const std::filesystem::path& filePath, std::string& error) {
#ifdef _WIN32
    // On Windows, 'code' is a batch script, so we must invoke it through cmd.
    std::string commandLine = "cmd /c code \"" + filePath.string() + "\"";
    std::vector<char> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi{};

    BOOL created = CreateProcessA(nullptr, commandLineBuffer.data(), nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi);

    if (!created) {
        DWORD code = GetLastError();
        error = "CreateProcess failed with code " + std::to_string(code);
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    (void)filePath;
    error = "openFileInVSCode is not yet implemented on this platform";
    return false;
#endif
}

}  // namespace vde::tools
