#include <vde/api/GameState.h>
#include <vde/api/StorageManager.h>

#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

namespace vde {

void GameState::checkThread() const {
    if (std::this_thread::get_id() != m_thread) {
        throw std::logic_error("GameState: access must occur on the game thread");
    }
}

std::string GameState::normalizeKey(const std::string& key) {
    if (key.empty()) {
        throw std::invalid_argument("GameState: key cannot be empty");
    }
    std::string result = key;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return result;
}

void GameState::setValue(const std::string& key, Value value) {
    checkThread();
    if (const auto* number = std::get_if<double>(&value); number && !std::isfinite(*number)) {
        throw std::invalid_argument("GameState: numeric values must be finite");
    }
    m_pendingValues[normalizeKey(key)] = std::move(value);
}

void GameState::eraseValue(const std::string& key) {
    checkThread();
    m_pendingValues[normalizeKey(key)] = std::nullopt;
}

std::optional<GameState::Value> GameState::getValue(const std::string& key) const {
    checkThread();
    auto it = m_values.find(normalizeKey(key));
    return it == m_values.end() ? std::nullopt : std::optional<Value>(it->second);
}

std::optional<double> GameState::getScriptStateValue(const std::string& key) const {
    const auto value = getValue(key);
    if (!value || std::holds_alternative<std::string>(*value)) {
        return std::nullopt;
    }
    return std::visit(
        [](const auto& item) -> std::optional<double> {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(item)>>) {
                return static_cast<double>(item);
            } else {
                return std::nullopt;
            }
        },
        *value);
}

GameState::SubscriptionId GameState::subscribe(ChangeCallback callback) {
    checkThread();
    if (!callback) {
        throw std::invalid_argument("GameState: empty change callback");
    }
    const auto id = m_nextSubscription++;
    m_subscribers.emplace(id, std::make_shared<ChangeCallback>(std::move(callback)));
    return id;
}

void GameState::unsubscribe(SubscriptionId id) {
    checkThread();
    m_subscribers.erase(id);
}

void GameState::commit() {
    checkThread();
    if (m_committing) {
        throw std::logic_error("GameState: recursive commit is forbidden");
    }
    struct CommitGuard {
        bool* flag;
        explicit CommitGuard(bool& value) : flag(&value) { *flag = true; }
        ~CommitGuard() { *flag = false; }
    } guard(m_committing);
    if (m_pendingValues.empty()) {
        if (m_pending.has_value()) {
            m_committed.swap(m_pending);
            m_pending.reset();
        }
        return;
    }
    auto nextValues = m_values;
    std::vector<std::tuple<std::string, std::optional<Value>, std::optional<Value>>> changes;
    for (const auto& [key, value] : m_pendingValues) {
        auto old = getValue(key);
        if (old == value) {
            continue;
        }
        changes.emplace_back(key, old, value);
        if (value) {
            nextValues[key] = *value;
        } else {
            nextValues.erase(key);
        }
    }
    std::vector<SubscriptionId> listeners;
    listeners.reserve(m_subscribers.size());
    for (const auto& [id, callback] : m_subscribers) {
        listeners.push_back(id);
    }
    if (m_pending.has_value()) {
        m_committed.swap(m_pending);
        m_pending.reset();
    }
    m_values.swap(nextValues);
    m_pendingValues.clear();
    for (const auto& [key, oldValue, newValue] : changes) {
        for (auto id : listeners) {
            auto it = m_subscribers.find(id);
            if (it != m_subscribers.end()) {
                auto callback = it->second;
                (*callback)(key, oldValue, newValue);
            }
        }
    }
}

void GameState::save(const std::string& slot) const {
    checkThread();
    if (slot.empty()) {
        throw std::invalid_argument("GameState: save slot cannot be empty");
    }
    if (m_committed.has_value() && !m_encode) {
        throw std::logic_error("GameState: typed saves require a serializer");
    }
    nlohmann::json data = {
        {"version", 1},
        {"schema", m_schema},
        {"typed", m_encode ? nlohmann::json(m_encode(m_committed)) : nlohmann::json(nullptr)},
        {"values", nlohmann::json::object()}};
    for (const auto& [key, value] : m_values) {
        data["values"][key] = {
            {"type", value.index()},
            {"value", std::visit([](const auto& item) { return nlohmann::json(item); }, value)}};
    }
    if (!StorageManager::getInstance().setStringData("vde.game_state.slot." + slot, data.dump())) {
        throw std::runtime_error("GameState: failed to save slot '" + slot + "'");
    }
}

void GameState::load(const std::string& slot) {
    checkThread();
    if (slot.empty()) {
        throw std::invalid_argument("GameState: save slot cannot be empty");
    }
    const auto stored = StorageManager::getInstance().getStringData("vde.game_state.slot." + slot);
    if (!stored) {
        throw std::runtime_error("GameState: cannot read slot '" + slot + "'");
    }
    std::any typed;
    std::map<std::string, Value> values;
    try {
        const auto data = nlohmann::json::parse(*stored);
        if (data.at("version") != 1 || data.at("schema") != m_schema ||
            !data.at("values").is_object() ||
            data.at("typed").is_null() == m_committed.has_value()) {
            throw std::runtime_error("GameState: incompatible save format or schema");
        }
        if (m_committed.has_value()) {
            if (!m_decode) {
                throw std::logic_error("GameState: typed loads require a serializer");
            }
            typed = m_decode(data.at("typed").get<std::string>());
        }
        for (const auto& [key, entry] : data.at("values").items()) {
            if (normalizeKey(key) != key) {
                throw std::runtime_error("GameState: noncanonical saved key");
            }
            const auto& value = entry.at("value");
            Value decoded;
            if (!entry.at("type").is_number_integer() || entry.at("type") < 0 ||
                entry.at("type") > 3) {
                throw std::runtime_error("GameState: invalid saved type tag");
            }
            const auto type = entry.at("type").get<int>();
            if (type == 0 && value.is_boolean()) {
                decoded = value.get<bool>();
            } else if (type == 1 && value.is_number_integer() &&
                       (!value.is_number_unsigned() ||
                        value.get<uint64_t>() <=
                            static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))) {
                decoded = value.get<int64_t>();
            } else if (type == 2 && value.is_number() && std::isfinite(value.get<double>())) {
                decoded = value.get<double>();
            } else if (type == 3 && value.is_string()) {
                decoded = value.get<std::string>();
            } else {
                throw std::runtime_error("GameState: invalid saved value type");
            }
            values.emplace(key, std::move(decoded));
        }
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(std::string("GameState: invalid save data: ") + ex.what());
    }
    std::map<std::string, std::optional<Value>> pending;
    for (const auto& [key, value] : m_values) {
        if (!values.contains(key)) {
            pending[key] = std::nullopt;
        }
    }
    for (auto& [key, value] : values) {
        pending[key] = std::move(value);
    }
    m_pending.swap(typed);
    m_pendingValues.swap(pending);
}

}  // namespace vde
