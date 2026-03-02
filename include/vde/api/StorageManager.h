#pragma once

/**
 * @file StorageManager.h
 * @brief Persistent key-value storage backed by SQLite.
 *
 * Provides a simple API for storing and retrieving string and binary data
 * that persists across game sessions. The database is stored in the
 * platform's standard application-data directory:
 *   - Windows: %APPDATA%\\<appname>\\storage.db
 *   - Linux:   ~/.local/share/<appname>/storage.db
 *   - macOS:   ~/Library/Application Support/<appname>/storage.db
 *
 * @example
 * @code
 * // One-time setup (usually in Game::onInitialize)
 * vde::StorageManager::getInstance().init_storage("MyGame");
 *
 * // Store values
 * storage.setStringData("playerName", "Alice");
 * storage.setBinData("highScore", 9001);
 *
 * // Retrieve values
 * auto name  = storage.getStringData("playerName"); // std::optional<std::string>
 * auto score = storage.getBinData<int>("highScore"); // std::optional<int>
 * @endcode
 */

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

// Forward-declare sqlite3 so consumers do not need to include sqlite3.h
struct sqlite3;
struct sqlite3_stmt;

namespace vde {

/**
 * @brief Singleton manager for persistent key-value storage.
 *
 * The storage is backed by a single SQLite database with two tables:
 *  - `string_data`  – UTF-8 text values
 *  - `binary_data`  – raw BLOB values
 *
 * Call init_storage() once before using any get/set methods. The manager
 * is automatically shut down by Game::shutdown(); you do not need to call
 * shutdown() manually in normal usage.
 */
class StorageManager {
  public:
    /**
     * @brief Get the singleton instance.
     */
    static StorageManager& getInstance();

    // Non-copyable, non-movable
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;
    StorageManager(StorageManager&&) = delete;
    StorageManager& operator=(StorageManager&&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Open (or create) the persistent storage database.
     *
     * The database file is placed in the platform application-data directory
     * under a subdirectory named after @p appName.  Calling this a second time
     * while already initialized is a no-op and returns true.
     *
     * @param appName  Application name used as the storage subdirectory.
     * @return true on success, false if the database could not be opened.
     */
    bool init_storage(const std::string& appName);

    /**
     * @brief Close the database and release all resources.
     *
     * Called automatically by Game::shutdown(). Safe to call multiple times.
     */
    void shutdown();

    /**
     * @brief Returns true if the storage has been successfully initialised.
     */
    bool isInitialized() const { return m_db != nullptr; }

    // -------------------------------------------------------------------------
    // String data
    // -------------------------------------------------------------------------

    /**
     * @brief Store a UTF-8 string value under @p key.
     *
     * Inserts a new row or replaces an existing one.
     *
     * @param key    Unique string identifier.
     * @param value  Value to store.
     * @return true on success.
     */
    bool setStringData(const std::string& key, const std::string& value);

    /**
     * @brief Retrieve a previously stored string value.
     *
     * @param key  Unique string identifier.
     * @return The stored value, or std::nullopt if the key does not exist.
     */
    std::optional<std::string> getStringData(const std::string& key);

    // -------------------------------------------------------------------------
    // Binary data – raw bytes overload
    // -------------------------------------------------------------------------

    /**
     * @brief Store raw binary data under @p key.
     *
     * @param key   Unique string identifier.
     * @param data  Bytes to store.
     * @return true on success.
     */
    bool setBinData(const std::string& key, const std::vector<uint8_t>& data);

    /**
     * @brief Retrieve raw binary data stored under @p key.
     *
     * @param key  Unique string identifier.
     * @return The stored bytes, or std::nullopt if the key does not exist.
     */
    std::optional<std::vector<uint8_t>> getBinData(const std::string& key);

    // -------------------------------------------------------------------------
    // Binary data – typed template overloads
    // -------------------------------------------------------------------------

    /**
     * @brief Serialise @p value as a binary blob and store it under @p key.
     *
     * The value is copied byte-for-byte (trivially copyable types only).
     * For complex or pointer-containing types use the raw-bytes overload
     * with a proper serialisation scheme instead.
     *
     * @tparam T   A trivially copyable type.
     * @param key   Unique string identifier.
     * @param value Value to store.
     * @return true on success.
     */
    template <typename T>
    bool setBinData(const std::string& key, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "setBinData<T>: T must be trivially copyable");
        std::vector<uint8_t> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));
        return setBinData(key, bytes);
    }

    /**
     * @brief Retrieve a binary blob and deserialise it as type @p T.
     *
     * Returns std::nullopt if the key does not exist or the stored blob is
     * a different size than sizeof(T).
     *
     * @tparam T  A trivially copyable type.
     * @param key  Unique string identifier.
     * @return The deserialised value, or std::nullopt on failure.
     */
    template <typename T>
    std::optional<T> getBinData(const std::string& key) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "getBinData<T>: T must be trivially copyable");
        auto bytes = getBinData(key);
        if (!bytes.has_value() || bytes->size() != sizeof(T)) {
            return std::nullopt;
        }
        T value{};
        std::memcpy(&value, bytes->data(), sizeof(T));
        return value;
    }

  private:
    StorageManager();
    ~StorageManager();

    /**
     * @brief Resolve the platform-specific application-data directory.
     * @param appName  Subdirectory name for the application.
     * @return Absolute path including the appName subdirectory, or empty on failure.
     */
    std::string resolveStoragePath(const std::string& appName) const;

    /**
     * @brief Create the required tables if they do not already exist.
     * @return true on success.
     */
    bool createTables();

    sqlite3* m_db{nullptr};  ///< Raw SQLite connection handle (nullptr when closed)
};

}  // namespace vde
