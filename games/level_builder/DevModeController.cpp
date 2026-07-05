#include "DevModeController.h"

#include <glm/geometric.hpp>

#include <array>

namespace {

constexpr float kDevelopmentMoveSpeed = 9.5f;
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
}

void DevModeController::exit() {
    m_enabled = false;
    m_activeSubmode = DevelopmentSubmode::MoveMode;
}

void DevModeController::setPosition(const glm::vec2& position) {
    m_position = position;
}

void DevModeController::update(float deltaTime, const glm::vec2& moveAxis) {
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

void DevModeController::cycleToNextAvailableSubmode() {
    cycleSubmode(1);
}

void DevModeController::cycleToPreviousAvailableSubmode() {
    cycleSubmode(-1);
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
            return;
        }
    }
}

bool DevModeController::isSubmodeAvailable(DevelopmentSubmode submode) const {
    return submode == DevelopmentSubmode::MoveMode;
}

}  // namespace levelbuilder