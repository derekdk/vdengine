#pragma once

#include <glm/vec2.hpp>

namespace levelbuilder {

enum class DevelopmentSubmode {
    MoveMode,
    SelectTileMode,
};

class DevModeController {
  public:
    void enter(const glm::vec2& startPosition);
    void exit();

    void setPosition(const glm::vec2& position);
    void update(float deltaTime, const glm::vec2& moveAxis);

    void cycleToNextAvailableSubmode();
    void cycleToPreviousAvailableSubmode();

    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] DevelopmentSubmode activeSubmode() const { return m_activeSubmode; }
    [[nodiscard]] const char* activeSubmodeName() const;
    [[nodiscard]] glm::vec2 position() const { return m_position; }

  private:
    void cycleSubmode(int direction);
    [[nodiscard]] bool isSubmodeAvailable(DevelopmentSubmode submode) const;

    bool m_enabled = false;
    DevelopmentSubmode m_activeSubmode = DevelopmentSubmode::MoveMode;
    glm::vec2 m_position{0.0f};
};

}  // namespace levelbuilder