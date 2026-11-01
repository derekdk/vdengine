#pragma once

/**
 * @file GameState.h
 * @brief Game-owned, frame-staged typed and key/value state.
 */

#include <any>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

namespace vde {

class Game;

/**
 * @brief Shared state with immutable committed reads and staged writes.
 *
 * All operations belong to the constructing (game) thread. Typed state must
 * have value semantics: copies must not share mutable pointees. Updates compose
 * on a pending copy; reads never see that copy until Game publishes it.
 */
class GameState {
  public:
    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;
    GameState(GameState&&) = delete;
    GameState& operator=(GameState&&) = delete;

    using Value = std::variant<bool, int64_t, double, std::string>;
    using ChangeCallback = std::function<void(const std::string&, const std::optional<Value>&,
                                              const std::optional<Value>&)>;
    using SubscriptionId = uint64_t;

    /// @brief Install one copyable state type before use. Reinitialization throws.
    template <typename T>
    void initialize(T initial) {
        checkThread();
        if (m_committed.has_value() || m_initializationLocked) {
            throw std::logic_error("GameState: initialize typed state once before the first frame");
        }
        m_committed = std::move(initial);
    }

    /// @brief Read committed typed state; missing/wrong types throw std::logic_error.
    /// References are valid only until the next commit. Copy to retain a snapshot.
    template <typename T>
    [[nodiscard]] const T& state() const {
        checkThread();
        const auto* value = std::any_cast<T>(&m_committed);
        if (!value) {
            throw std::logic_error("GameState: missing or mismatched state type");
        }
        return *value;
    }

    /// @brief Stage a mutation on a copy; throwing callbacks leave pending state unchanged.
    /// Multiple writes compose in submission order; conflicting writes are last-writer-wins.
    template <typename T, typename F>
    void update(F&& mutation) {
        checkThread();
        (void)state<T>();
        std::any next = m_pending.has_value() ? m_pending : m_committed;
        std::invoke(std::forward<F>(mutation), std::any_cast<T&>(next));
        m_pending.swap(next);
    }

    /// @brief Register explicit serialization for typed save data. Schema is a stable version ID.
    /// Encoder returns an opaque string; decoder must validate it and throw on invalid data.
    template <typename T>
    void setSerializer(std::string schema, std::function<std::string(const T&)> encode,
                       std::function<T(const std::string&)> decode) {
        checkThread();
        (void)state<T>();
        if (schema.empty() || !encode || !decode) {
            throw std::invalid_argument("GameState: serializer requires schema and callbacks");
        }
        m_schema = std::move(schema);
        m_encode = [encode = std::move(encode)](const std::any& value) {
            return encode(std::any_cast<const T&>(value));
        };
        m_decode = [decode = std::move(decode)](const std::string& value) -> std::any {
            return decode(value);
        };
    }

    /// @brief Stage a key/value write. Keys are nonempty, ASCII-case-insensitive.
    /// Non-finite doubles are rejected with std::invalid_argument.
    void setValue(const std::string& key, Value value);
    /// @brief Stage removal; removing a missing key does not emit a notification.
    void eraseValue(const std::string& key);
    /// @brief Read a committed value (nullopt for missing keys).
    [[nodiscard]] std::optional<Value> getValue(const std::string& key) const;
    /// @brief Resolve bool/integer/double values for numeric script assertions.
    [[nodiscard]] std::optional<double> getScriptStateValue(const std::string& key) const;
    /// @brief Subscribe to all changed keys. Notifications run after the entire commit.
    /// Old/new nullopt denote insertion/removal. Callback writes wait for the next commit.
    SubscriptionId subscribe(ChangeCallback callback);
    /// @brief Remove a subscription, including during notification dispatch.
    void unsubscribe(SubscriptionId id);
    /// @brief Save only committed state to a StorageManager slot; errors throw.
    /// Typed state requires an explicit serializer. Pending writes are not saved.
    void save(const std::string& slot) const;
    /// @brief Validate a slot and stage a complete replacement; errors throw without mutation.
    /// Loaded state becomes visible at the same boundary as ordinary writes.
    void load(const std::string& slot);

  private:
    friend class Game;
    void commit();
    void checkThread() const;
    static std::string normalizeKey(const std::string& key);

    std::thread::id m_thread = std::this_thread::get_id();
    bool m_committing = false;
    bool m_initializationLocked = false;
    std::any m_committed;
    std::any m_pending;
    std::map<std::string, Value> m_values;
    std::map<std::string, std::optional<Value>> m_pendingValues;
    std::map<SubscriptionId, std::shared_ptr<ChangeCallback>> m_subscribers;
    SubscriptionId m_nextSubscription = 1;
    std::string m_schema;
    std::function<std::string(const std::any&)> m_encode;
    std::function<std::any(const std::string&)> m_decode;
};

}  // namespace vde
