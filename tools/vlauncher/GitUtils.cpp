#include "GitUtils.h"

#include <array>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace vde::tools {

GitUtils::GitUtils(std::filesystem::path repoRoot) : m_repoRoot(std::move(repoRoot)) {
    CommandResult result = runGitCommand("rev-parse --is-inside-work-tree");
    std::string trimmed = trim(result.output);
    m_gitAvailable = (result.exitCode == 0 && trimmed == "true");
}

bool GitUtils::hasUncommittedChanges(const std::filesystem::path& pathInRepo) const {
    if (!m_gitAvailable) {
        return false;
    }

    std::error_code error;
    std::filesystem::path rel = std::filesystem::relative(pathInRepo, m_repoRoot, error);
    if (error) {
        return false;
    }

    std::ostringstream args;
    args << "status --porcelain -- \"" << rel.generic_string() << "\"";

    CommandResult result = runGitCommand(args.str());
    if (result.exitCode != 0) {
        return false;
    }

    return !trim(result.output).empty();
}

std::optional<std::chrono::system_clock::time_point>
GitUtils::getLastCommitTime(const std::filesystem::path& pathInRepo) const {
    if (!m_gitAvailable) {
        return std::nullopt;
    }

    std::error_code error;
    std::filesystem::path rel = std::filesystem::relative(pathInRepo, m_repoRoot, error);
    if (error) {
        return std::nullopt;
    }

    std::ostringstream args;
    args << "log -1 --format=%ct -- \"" << rel.generic_string() << "\"";

    CommandResult result = runGitCommand(args.str());
    if (result.exitCode != 0) {
        return std::nullopt;
    }

    std::string trimmed = trim(result.output);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    try {
        std::time_t commitEpoch = static_cast<std::time_t>(std::stoll(trimmed));
        return std::chrono::system_clock::from_time_t(commitEpoch);
    } catch (...) {
        return std::nullopt;
    }
}

GitUtils::CommandResult GitUtils::runGitCommand(const std::string& args) const {
    CommandResult result;

    std::ostringstream command;
    command << "git -C \"" << m_repoRoot.string() << "\" " << args;

    FILE* pipe = popen(command.str().c_str(), "r");
    if (!pipe) {
        result.exitCode = -1;
        return result;
    }

    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

    result.exitCode = pclose(pipe);
    return result;
}

std::string GitUtils::trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

}  // namespace vde::tools
