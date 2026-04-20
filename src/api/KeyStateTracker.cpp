#include <vde/api/KeyStateTracker.h>

namespace vde {

void KeyStateTracker::bindHeld(int keyCode, const std::string& name) {
    m_bindings[keyCode].push_back({name, BindingType::Held});
    // Initialize held count to zero if not already present
    m_heldCounts.try_emplace(name, 0);
}

void KeyStateTracker::bindOneShot(int keyCode, const std::string& name) {
    m_bindings[keyCode].push_back({name, BindingType::OneShot});
    // Initialize one-shot state to false if not already present
    m_oneShotTriggered.try_emplace(name, false);
}

bool KeyStateTracker::isHeld(const std::string& name) const {
    auto it = m_heldCounts.find(name);
    if (it == m_heldCounts.end()) {
        return false;
    }
    return it->second > 0;
}

bool KeyStateTracker::consume(const std::string& name) {
    auto it = m_oneShotTriggered.find(name);
    if (it == m_oneShotTriggered.end()) {
        return false;
    }
    bool triggered = it->second;
    it->second = false;
    return triggered;
}

void KeyStateTracker::handlePress(int keyCode) {
    // Only process transitions from up to down to avoid count drift on repeat events
    if (!m_pressedKeys.insert(keyCode).second) {
        return;
    }
    auto it = m_bindings.find(keyCode);
    if (it == m_bindings.end()) {
        return;
    }
    for (const auto& binding : it->second) {
        if (binding.type == BindingType::Held) {
            m_heldCounts[binding.name]++;
        } else {
            m_oneShotTriggered[binding.name] = true;
        }
    }
}

void KeyStateTracker::handleRelease(int keyCode) {
    m_pressedKeys.erase(keyCode);
    auto it = m_bindings.find(keyCode);
    if (it == m_bindings.end()) {
        return;
    }
    for (const auto& binding : it->second) {
        if (binding.type == BindingType::Held) {
            auto& count = m_heldCounts[binding.name];
            if (count > 0) {
                count--;
            }
        }
    }
}

}  // namespace vde
