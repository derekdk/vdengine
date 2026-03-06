#include "RunLogStorage.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include <vde/api/StorageManager.h>

namespace vde::tools {

namespace {

constexpr uint32_t kRunLogHistoryVersion = 1u;

uint64_t fnv1a64(const std::string& value) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char byte : value) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string normalizePathForKey(const std::filesystem::path& repositoryRoot,
                                const std::filesystem::path& executablePath) {
    std::error_code error;
    std::filesystem::path absolutePath = std::filesystem::absolute(executablePath, error);
    if (error) {
        absolutePath = executablePath;
    }
    absolutePath = absolutePath.lexically_normal();

    std::filesystem::path relativePath = std::filesystem::relative(absolutePath, repositoryRoot, error);
    std::string keyPath = error ? absolutePath.generic_string() : relativePath.generic_string();

    std::transform(keyPath.begin(), keyPath.end(), keyPath.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return keyPath;
}

void appendUint8(std::vector<uint8_t>& bytes, uint8_t value) {
    bytes.push_back(value);
}

void appendUint32(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

bool readUint8(const std::vector<uint8_t>& bytes, size_t& offset, uint8_t& value) {
    if (offset >= bytes.size()) {
        return false;
    }

    value = bytes[offset];
    ++offset;
    return true;
}

bool readUint32(const std::vector<uint8_t>& bytes, size_t& offset, uint32_t& value) {
    if (offset + 4 > bytes.size()) {
        return false;
    }

    value = static_cast<uint32_t>(bytes[offset]) |
            (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool appendEntry(std::vector<uint8_t>& bytes, const StoredRunLog& run) {
    if (run.timestamp.size() > std::numeric_limits<uint32_t>::max() ||
        run.commandLine.size() > std::numeric_limits<uint32_t>::max() ||
        run.output.size() > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    appendUint32(bytes, static_cast<uint32_t>(static_cast<int32_t>(run.exitCode)));
    appendUint32(bytes, static_cast<uint32_t>(run.timestamp.size()));
    appendUint32(bytes, static_cast<uint32_t>(run.commandLine.size()));
    appendUint32(bytes, static_cast<uint32_t>(run.output.size()));

    bytes.insert(bytes.end(), run.timestamp.begin(), run.timestamp.end());
    bytes.insert(bytes.end(), run.commandLine.begin(), run.commandLine.end());
    bytes.insert(bytes.end(), run.output.begin(), run.output.end());
    return true;
}

bool readEntry(const std::vector<uint8_t>& bytes, size_t& offset, StoredRunLog& run) {
    uint32_t exitCodeRaw = 0;
    uint32_t timestampSize = 0;
    uint32_t commandSize = 0;
    uint32_t outputSize = 0;

    if (!readUint32(bytes, offset, exitCodeRaw) || !readUint32(bytes, offset, timestampSize) ||
        !readUint32(bytes, offset, commandSize) || !readUint32(bytes, offset, outputSize)) {
        return false;
    }

    const uint64_t payloadSize = static_cast<uint64_t>(timestampSize) +
                                 static_cast<uint64_t>(commandSize) +
                                 static_cast<uint64_t>(outputSize);
    if (payloadSize > bytes.size() ||
        static_cast<uint64_t>(offset) + payloadSize > static_cast<uint64_t>(bytes.size())) {
        return false;
    }

    run.timestamp.assign(reinterpret_cast<const char*>(bytes.data() + offset), timestampSize);
    offset += timestampSize;

    run.commandLine.assign(reinterpret_cast<const char*>(bytes.data() + offset), commandSize);
    offset += commandSize;

    run.output.assign(reinterpret_cast<const char*>(bytes.data() + offset), outputSize);
    offset += outputSize;

    run.exitCode = static_cast<int>(static_cast<int32_t>(exitCodeRaw));
    return true;
}

bool serializeHistory(const std::array<std::optional<StoredRunLog>, 2>& history,
                      std::vector<uint8_t>& bytes) {
    bytes.clear();
    bytes.reserve(512);

    appendUint32(bytes, kRunLogHistoryVersion);

    for (const auto& slot : history) {
        if (!slot.has_value()) {
            appendUint8(bytes, 0);
            continue;
        }

        appendUint8(bytes, 1);
        if (!appendEntry(bytes, *slot)) {
            return false;
        }
    }

    return true;
}

bool deserializeHistory(const std::vector<uint8_t>& bytes,
                        std::array<std::optional<StoredRunLog>, 2>& history) {
    history = {};

    size_t offset = 0;
    uint32_t version = 0;
    if (!readUint32(bytes, offset, version)) {
        return false;
    }

    if (version != kRunLogHistoryVersion) {
        return false;
    }

    for (size_t slot = 0; slot < history.size(); ++slot) {
        uint8_t hasEntry = 0;
        if (!readUint8(bytes, offset, hasEntry)) {
            return false;
        }

        if (hasEntry == 0) {
            continue;
        }

        StoredRunLog run;
        if (!readEntry(bytes, offset, run)) {
            return false;
        }

        history[slot] = std::move(run);
    }

    if (offset != bytes.size()) {
        return false;
    }

    return true;
}

}  // namespace

std::string RunLogStorage::buildTargetId(const std::filesystem::path& repositoryRoot,
                                         const std::filesystem::path& executablePath) {
    const std::string keyPath = normalizePathForKey(repositoryRoot, executablePath);

    std::ostringstream stream;
    stream << "target_" << std::hex << std::setw(16) << std::setfill('0') << fnv1a64(keyPath);
    return stream.str();
}

bool RunLogStorage::saveLatestRun(const std::string& targetId, const StoredRunLog& run,
                                  std::string& error) {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        error = "Storage is not initialized";
        return false;
    }

    const auto currentHistory = loadRecentRuns(targetId);

    std::array<std::optional<StoredRunLog>, 2> updatedHistory;
    updatedHistory[0] = run;
    if (currentHistory[0].has_value()) {
        updatedHistory[1] = currentHistory[0];
    }

    std::vector<uint8_t> bytes;
    if (!serializeHistory(updatedHistory, bytes)) {
        error = "Failed to serialize run log history";
        return false;
    }

    if (!storage.setBinData(makeHistoryKey(targetId), bytes)) {
        error = "Failed to persist run log history";
        return false;
    }

    return true;
}

std::array<std::optional<StoredRunLog>, 2> RunLogStorage::loadRecentRuns(const std::string& targetId) {
    auto& storage = vde::StorageManager::getInstance();
    if (!storage.isInitialized()) {
        return {};
    }

    const auto blob = storage.getBinData(makeHistoryKey(targetId));
    if (!blob.has_value()) {
        return {};
    }

    std::array<std::optional<StoredRunLog>, 2> history;
    if (!deserializeHistory(*blob, history)) {
        return {};
    }

    return history;
}

std::string RunLogStorage::makeHistoryKey(const std::string& targetId) {
    return "vlauncher.runlog." + targetId + ".history";
}

}  // namespace vde::tools
