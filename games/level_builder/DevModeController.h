#pragma once

#include <glm/vec2.hpp>

#include <optional>

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
    void updateMoveMode(float deltaTime, const glm::vec2& moveAxis);
    bool updateSelectTileMode(float deltaTime, const glm::ivec2& moveAxis,
                              const glm::ivec2& maxTileInclusive);

    void cycleToNextAvailableSubmode();
    void cycleToPreviousAvailableSubmode();

    void setSelectedTile(const glm::ivec2& tileCoordinate);
    void setClipboardTile(int tileId);

    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] DevelopmentSubmode activeSubmode() const { return m_activeSubmode; }
    [[nodiscard]] const char* activeSubmodeName() const;
    [[nodiscard]] glm::vec2 position() const { return m_position; }
    [[nodiscard]] bool hasSelection() const { return m_hasSelection; }
    [[nodiscard]] glm::ivec2 selectedTile() const { return m_selectedTile; }
    [[nodiscard]] bool hasClipboardTile() const { return m_clipboardTile.has_value(); }
    [[nodiscard]] std::optional<int> clipboardTile() const { return m_clipboardTile; }

  private:
    struct AxisRepeatState {
        int direction = 0;
        float timer = 0.0f;
    };

    void cycleSubmode(int direction);
    [[nodiscard]] bool isSubmodeAvailable(DevelopmentSubmode submode) const;
    void resetSelectionRepeatState();
    bool advanceSelectionAxis(float deltaTime, int direction, int maxInclusive, int& value,
                              AxisRepeatState& repeatState);

    bool m_enabled = false;
    DevelopmentSubmode m_activeSubmode = DevelopmentSubmode::MoveMode;
    glm::vec2 m_position{0.0f};
    bool m_hasSelection = false;
    glm::ivec2 m_selectedTile{0, 0};
    std::optional<int> m_clipboardTile;
    AxisRepeatState m_horizontalRepeat;
    AxisRepeatState m_verticalRepeat;
};

}  // namespace levelbuilder