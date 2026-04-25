#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "../GameBase.h"

namespace fishing {

class FishingInput;

struct FishState {
    std::shared_ptr<vde::SpriteEntity> entity;
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;
    float width = 0.0f;
    int scoreValue = 0;
    bool goingRight = true;
    float nibbleProgress = 0.0f;
};

class FishingGameScene : public vde::games::BaseGameScene {
  public:
    enum class LineState {
        Stowed,
        InWater,
        Hooked,
    };

    FishingGameScene();

    void onEnter() override;
    void update(float deltaTime) override;
    void drawDebugUI() override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    void createEnvironment();
    void createHud();
    void resetRound();
    void respawnFish(FishState& fish, bool guaranteeStartFish = false);
    void updateBoat(float deltaTime);
    void updateBobber(float deltaTime);
    void updateFish(float deltaTime);
    void updateHud();
    void deployLine();
    void reelLine();
    void catchFish(FishState& fish);
    fishing::FishingInput* input();

    static constexpr float kViewWidth = 14.0f;
    static constexpr float kViewHeight = 10.0f;
    static constexpr float kWaterSurfaceY = 1.5f;
    static constexpr float kWaterFloorY = -4.2f;
    static constexpr float kBoatMinX = -5.6f;
    static constexpr float kBoatMaxX = 5.6f;
    static constexpr float kBoatSpeed = 4.2f;
    static constexpr float kDepthAdjustSpeed = 2.2f;
    static constexpr float kMinDepth = 0.6f;
    static constexpr float kMaxDepth = 4.7f;
    static constexpr float kHookDistanceX = 0.45f;
    static constexpr float kHookDistanceY = 0.35f;
    static constexpr float kNibbleTime = 1.0f;
    static constexpr int kGoalFish = 3;

    std::shared_ptr<vde::SpriteEntity> m_water;
    std::shared_ptr<vde::SpriteEntity> m_surface;
    std::shared_ptr<vde::SpriteEntity> m_boatHull;
    std::shared_ptr<vde::SpriteEntity> m_boatCabin;
    std::shared_ptr<vde::SpriteEntity> m_bobber;
    std::shared_ptr<vde::TextEntity> m_titleText;
    std::shared_ptr<vde::TextEntity> m_scoreText;
    std::shared_ptr<vde::TextEntity> m_statusText;
    std::shared_ptr<vde::TextEntity> m_controlText;

    std::vector<FishState> m_fish;
    std::minstd_rand m_rng;
    LineState m_lineState = LineState::Stowed;
    int m_hookedFishIndex = -1;
    float m_boatX = 0.0f;
    float m_bobberDepth = 2.3f;
    float m_elapsed = 0.0f;
    int m_score = 0;
    int m_caughtCount = 0;
    fishing::FishingInput* m_input = nullptr;
    std::string m_status = "Move to a fish, cast the line, and wait for a bite.";
};

}  // namespace fishing