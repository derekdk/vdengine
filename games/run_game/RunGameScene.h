#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../GameBase.h"

namespace rungame {

class RunGameInput;
class ParallaxBackground;
class RunnerCharacter;
class ObstacleCourse;

class RunGameScene : public vde::games::BaseGameScene {
  public:
    RunGameScene();
    ~RunGameScene() override;

    void onEnter() override;
    void update(float deltaTime) override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;
    std::optional<double> getScriptStateValue(const std::string& key) const override;

  private:
    void createHud();
    void resetGame();
    void updateHud();

    std::unique_ptr<ParallaxBackground> m_background;
    std::unique_ptr<RunnerCharacter> m_runner;
    std::unique_ptr<ObstacleCourse> m_obstacles;
    std::shared_ptr<vde::TextEntity> m_titleText;
    std::shared_ptr<vde::TextEntity> m_scoreText;
    std::shared_ptr<vde::TextEntity> m_statusText;
    float m_elapsed = 0.0f;
    float m_distance = 0.0f;
    int m_displayedScore = -1;
    bool m_gameOver = false;
};

}  // namespace rungame
