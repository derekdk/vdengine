#pragma once

/**
 * @file AnimatedSpriteEntity.h
 * @brief SpriteEntity wrapper for named SpriteSheet-based animation states.
 */

#include <vde/api/Entity.h>
#include <vde/api/SpriteAnimation.h>
#include <vde/api/SpriteSheet.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief 2D sprite entity with named frame animation clips.
 *
 * AnimatedSpriteEntity owns a SpriteSheet reference and a set of named
 * SpriteAnimation clips. Playback advances automatically during update()
 * and applies the corresponding UV rect on each frame transition.
 */
class AnimatedSpriteEntity : public SpriteEntity {
  public:
    using FrameCallback = std::function<void()>;

    /**
     * @brief Set the SpriteSheet used by this entity.
     *
     * The underlying atlas texture is applied to SpriteEntity automatically.
     */
    void setSpriteSheet(std::shared_ptr<SpriteSheet> sheet);

    /**
     * @brief Get the active SpriteSheet.
     */
    std::shared_ptr<SpriteSheet> getSpriteSheet() const { return m_spriteSheet; }

    /**
     * @brief Add or replace a named animation clip.
     * @param name State name used with play().
     * @param animation Clip data to store.
     * @throws std::invalid_argument if name is empty or the clip has no frames.
     *
     * Replacing the clip that is currently active restarts that state's
     * playback from frame 0 so the new clip is applied deterministically.
     */
    void addAnimation(const std::string& name, SpriteAnimation animation);

    /**
     * @brief Check whether a named animation exists.
     */
    bool hasAnimation(const std::string& name) const;

    /**
     * @brief Get a named animation clip.
     * @throws std::out_of_range if the animation does not exist.
     */
    const SpriteAnimation& getAnimation(const std::string& name) const;

    /**
     * @brief Start or resume playback of a named animation.
     *
     * If reset is false and the requested animation is already active, the
     * current frame/time are preserved unless the active clip had already
     * finished naturally, in which case playback restarts from frame 0.
     */
    void play(const std::string& name, bool reset = true);

    /**
     * @brief Pause playback without losing the current frame/time.
     */
    void pause();

    /**
     * @brief Resume playback after pause().
     */
    void resume();

    /**
     * @brief Stop playback and return to the first frame of the current clip.
     *
     * This is an explicit reset operation, so any prior finished state is
     * cleared along with the current playback time.
     */
    void stop();

    /**
     * @brief Set playback speed multiplier.
     * @param speed 0 = frozen, 1 = normal speed, 2 = double speed.
     * @throws std::invalid_argument if speed is negative.
     */
    void setSpeed(float speed);

    /**
     * @brief Get playback speed multiplier.
     */
    float getSpeed() const { return m_speed; }

    /**
     * @brief Check whether playback is currently advancing.
     */
    bool isPlaying() const { return m_isPlaying; }

    /**
     * @brief Check whether playback is paused.
     */
    bool isPaused() const { return m_isPaused; }

    /**
     * @brief Check whether the current non-looping animation finished naturally.
     *
     * Returns false after stop() because stop() resets playback state.
     */
    bool isAnimationFinished() const { return m_finished; }

    /**
     * @brief Get the active animation name.
     */
    const std::string& getCurrentAnimation() const { return m_currentAnimationName; }

    /**
     * @brief Get the active frame index within the current animation clip.
     */
    int getCurrentFrame() const { return m_currentFrameIndex; }

    /**
     * @brief Register a callback for when a specific frame becomes active.
     * @param animName Animation state name.
     * @param frameIndex Frame index inside the clip.
     * @param callback Callback invoked each time that frame is entered.
     * @throws std::invalid_argument if frameIndex is negative.
     * @throws std::out_of_range if the animation does not exist or frameIndex is not valid for it.
     */
    void onFrameEvent(const std::string& animName, int frameIndex, FrameCallback callback);

    void update(float deltaTime) override;

  protected:
    /**
     * @brief Apply the current animation frame's UV rect to the sprite.
     */
    void applyCurrentFrame();

  private:
    using FrameCallbackMap = std::unordered_map<int, std::vector<FrameCallback>>;

    const SpriteAnimation* currentAnimation() const;
    void resetPlayback();
    void advanceFrame();
    void fireFrameCallbacks(const std::string& animName, int frameIndex);

    std::shared_ptr<SpriteSheet> m_spriteSheet;
    std::unordered_map<std::string, SpriteAnimation> m_animations;
    std::unordered_map<std::string, FrameCallbackMap> m_frameCallbacks;
    std::string m_currentAnimationName;
    int m_currentFrameIndex = 0;
    float m_currentFrameElapsed = 0.0f;
    float m_speed = 1.0f;
    bool m_isPlaying = false;
    bool m_isPaused = false;
    bool m_finished = false;
};

}  // namespace vde