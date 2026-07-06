#include "DevModeController.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>

namespace {

constexpr float kDevelopmentMoveSpeed = 9.5f;
constexpr float kSelectionInitialRepeatDelay = 0.24f;
constexpr float kSelectionRepeatInterval = 0.10f;
constexpr std::array<levelbuilder::DevelopmentSubmode, 2> kAllSubmodes = {
    levelbuilder::DevelopmentSubmode::MoveMode,
    levelbuilder::DevelopmentSubmode::SelectTileMode,
};

}  // namespace

namespace levelbuilder {

void DevModeController::enter(const glm::vec2& startPosition) {
    m_enabled = true;
    m_activeSubmode = DevelopmentSubmode::MoveMode;
    m_position = startPosition;
    m_hasSelection = false;
    m_clipboardTile.reset();
    resetSelectionRepeatState();
}

void DevModeController::exit() {
    m_enabled = false;
    m_activeSubmode = DevelopmentSubmode::MoveMode;
    m_hasSelection = false;
    m_clipboardTile.reset();
    resetSelectionRepeatState();
}

void DevModeController::setPosition(const glm::vec2& position) {
    m_position = position;
}

void DevModeController::updateMoveMode(float deltaTime, const glm::vec2& moveAxis) {
    if (!m_enabled || m_activeSubmode != DevelopmentSubmode::MoveMode) {
        return;
    }

    glm::vec2 adjustedAxis = moveAxis;
    const float length = glm::length(adjustedAxis);
    if (length > 1.0f) {
        adjustedAxis /= length;
    }

    m_position += adjustedAxis * kDevelopmentMoveSpeed * deltaTime;
}

bool DevModeController::updateSelectTileMode(float deltaTime, const glm::ivec2& moveAxis,
                                             const glm::ivec2& maxTileInclusive) {
    if (!m_enabled || m_activeSubmode != DevelopmentSubmode::SelectTileMode || !m_hasSelection) {
        return false;
    }

    bool changed = false;
    changed |= advanceSelectionAxis(deltaTime, moveAxis.x, maxTileInclusive.x, m_selectedTile.x,
                                    m_horizontalRepeat);
    changed |= advanceSelectionAxis(deltaTime, moveAxis.y, maxTileInclusive.y, m_selectedTile.y,
                                    m_verticalRepeat);
    return changed;
}

void DevModeController::cycleToNextAvailableSubmode() {
    cycleSubmode(1);
}

void DevModeController::cycleToPreviousAvailableSubmode() {
    cycleSubmode(-1);
}

void DevModeController::setSelectedTile(const glm::ivec2& tileCoordinate) {
    m_selectedTile = tileCoordinate;
    m_hasSelection = true;
    resetSelectionRepeatState();
}

void DevModeController::setClipboardTile(int tileId) {
    m_clipboardTile = tileId;
}

const char* DevModeController::activeSubmodeName() const {
    switch (m_activeSubmode) {
    case DevelopmentSubmode::MoveMode:
        return "Move";
    case DevelopmentSubmode::SelectTileMode:
        return "Select Tile";
    default:
        return "Unknown";
    }
}

void DevModeController::cycleSubmode(int direction) {
    if (!m_enabled) {
        return;
    }

    size_t currentIndex = 0;
    for (size_t index = 0; index < kAllSubmodes.size(); ++index) {
        if (kAllSubmodes.at(index) == m_activeSubmode) {
            currentIndex = index;
            break;
        }
    }

    const int count = static_cast<int>(kAllSubmodes.size());
    for (int step = 0; step < count; ++step) {
        currentIndex =
            static_cast<size_t>((static_cast<int>(currentIndex) + direction + count) % count);
        if (isSubmodeAvailable(kAllSubmodes.at(currentIndex))) {
            m_activeSubmode = kAllSubmodes.at(currentIndex);
            resetSelectionRepeatState();
            return;
        }
    }
}

bool DevModeController::isSubmodeAvailable(DevelopmentSubmode submode) const {
    switch (submode) {
    case DevelopmentSubmode::MoveMode:
    case DevelopmentSubmode::SelectTileMode:
        return true;
    default:
        return false;
    }
}

void DevModeController::resetSelectionRepeatState() {
    m_horizontalRepeat = AxisRepeatState{};
    m_verticalRepeat = AxisRepeatState{};
}

bool DevModeController::advanceSelectionAxis(float deltaTime, int direction, int maxInclusive,
                                             int& value, AxisRepeatState& repeatState) {
    if (direction == 0) {
        repeatState = AxisRepeatState{};
        return false;
    }

    if (direction != repeatState.direction) {
        repeatState.direction = direction;
        repeatState.timer = kSelectionInitialRepeatDelay;
        const int nextValue = std::clamp(value + direction, 0, maxInclusive);
        const bool changed = nextValue != value;
        value = nextValue;
        return changed;
    }

    repeatState.timer -= deltaTime;
    if (repeatState.timer > 0.0f) {
        return false;
    }

    repeatState.timer += kSelectionRepeatInterval;
    const int nextValue = std::clamp(value + direction, 0, maxInclusive);
    const bool changed = nextValue != value;
    value = nextValue;
    return changed;
}

}  // namespace levelbuilder