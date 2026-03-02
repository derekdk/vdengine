/**
 * @file StorageManager.cpp
 * @brief Implementation of StorageManager – persistent SQLite key-value store.
 */

#include <vde/api/StorageManager.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#if defined(_WIN32)
#include <windows.h>
// Prevent <windows.h> macro pollution
#undef min
#undef max
#elif defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#else
// Linux / other POSIX
#include <pwd.h>
#include <unistd.h>
#endif

namespace vde {

// ============================================================================
// Singleton
// ============================================================================

StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

StorageManager::StorageManager() = default;

StorageManager::~StorageManager() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool StorageManager::init_storage(const std::string& appName) {
    if (m_db) {
        return true;  // already open
    }

    if (appName.empty()) {
        std::cerr << "[StorageManager] init_storage: appName must not be empty\n";
        return false;
    }

    const std::string dbPath = resolveStoragePath(appName);
    if (dbPath.empty()) {
        std::cerr << "[StorageManager] init_storage: could not resolve storage path\n";
        return false;
    }

    // Create parent directories if they do not exist
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::path(dbPath).parent_path();
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "[StorageManager] init_storage: failed to create directory '" << dir.string()
                  << "': " << ec.message() << "\n";
        return false;
    }

    const int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "[StorageManager] init_storage: sqlite3_open failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent read performance
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    if (!createTables()) {
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    return true;
}

void StorageManager::shutdown() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

// ============================================================================
// Internal helpers
// ============================================================================

std::string StorageManager::resolveStoragePath(const std::string& appName) const {
    std::filesystem::path base;

#if defined(_WIN32)
    // %APPDATA% (e.g. C:\Users\<user>\AppData\Roaming)
    char buf[MAX_PATH] = {};
    const DWORD len = GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    base = buf;

#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    if (!home) {
        return {};
    }
    base = std::filesystem::path(home) / "Library" / "Application Support";

#else  // Linux / other POSIX
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0') {
        base = xdgData;
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            home = getpwuid(getuid())->pw_dir;
        }
        if (!home) {
            return {};
        }
        base = std::filesystem::path(home) / ".local" / "share";
    }
#endif

    return (base / appName / "storage.db").string();
}

bool StorageManager::createTables() {
    const char* sql = "CREATE TABLE IF NOT EXISTS string_data ("
                      "    key   TEXT PRIMARY KEY NOT NULL,"
                      "    value TEXT NOT NULL"
                      ");"
                      "CREATE TABLE IF NOT EXISTS binary_data ("
                      "    key   TEXT PRIMARY KEY NOT NULL,"
                      "    value BLOB NOT NULL"
                      ");";

    char* errMsg = nullptr;
    const int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[StorageManager] createTables failed: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ============================================================================
// String data
// ============================================================================

bool StorageManager::setStringData(const std::string& key, const std::string& value) {
    if (!m_db) {
        std::cerr << "[StorageManager] setStringData: storage not initialised\n";
        return false;
    }

    const char* sql = "INSERT OR REPLACE INTO string_data (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[StorageManager] setStringData: prepare failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), static_cast<int>(value.size()), SQLITE_STATIC);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[StorageManager] setStringData: step failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        return false;
    }
    return true;
}

std::optional<std::string> StorageManager::getStringData(const std::string& key) {
    if (!m_db) {
        std::cerr << "[StorageManager] getStringData: storage not initialised\n";
        return std::nullopt;
    }

    const char* sql = "SELECT value FROM string_data WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[StorageManager] getStringData: prepare failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_STATIC);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const int bytes = sqlite3_column_bytes(stmt, 0);
        if (text) {
            result = std::string(text, static_cast<std::string::size_type>(bytes));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

// ============================================================================
// Binary data
// ============================================================================

bool StorageManager::setBinData(const std::string& key, const std::vector<uint8_t>& data) {
    if (!m_db) {
        std::cerr << "[StorageManager] setBinData: storage not initialised\n";
        return false;
    }

    const char* sql = "INSERT OR REPLACE INTO binary_data (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[StorageManager] setBinData: prepare failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_STATIC);
    // sqlite3_bind_blob with size 0 stores NULL; use sqlite3_bind_zeroblob for empty blobs
    if (data.empty()) {
        sqlite3_bind_zeroblob(stmt, 2, 0);
    } else {
        sqlite3_bind_blob(stmt, 2, data.data(), static_cast<int>(data.size()), SQLITE_STATIC);
    }

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[StorageManager] setBinData: step failed: " << sqlite3_errmsg(m_db) << "\n";
        return false;
    }
    return true;
}

std::optional<std::vector<uint8_t>> StorageManager::getBinData(const std::string& key) {
    if (!m_db) {
        std::cerr << "[StorageManager] getBinData: storage not initialised\n";
        return std::nullopt;
    }

    const char* sql = "SELECT value FROM binary_data WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[StorageManager] getBinData: prepare failed: " << sqlite3_errmsg(m_db)
                  << "\n";
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), static_cast<int>(key.size()), SQLITE_STATIC);

    std::optional<std::vector<uint8_t>> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // sqlite3_column_blob may return null for a zero-length blob; check column type
        const int bytes = sqlite3_column_bytes(stmt, 0);
        if (bytes > 0) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            const auto* src = static_cast<const uint8_t*>(blob);
            result = std::vector<uint8_t>(src, src + bytes);
        } else {
            // Zero-length blob (stored via sqlite3_bind_zeroblob) — return empty vector
            result = std::vector<uint8_t>{};
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

}  // namespace vde
