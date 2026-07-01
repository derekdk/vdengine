#pragma once

/**
 * @file SpriteAnimation.h
 * @brief Frame-based sprite animation clips for SpriteSheet-backed entities.
 */

#include <string>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Named frame sequence used by AnimatedSpriteEntity.
 *
 * Each frame references a sprite index inside a SpriteSheet and defines how
 * long that frame should remain visible during playback.
 */
class SpriteAnimation {
  public:
    /**
     * @brief One animation frame.
     */
    struct Frame {
        int spriteIndex = 0;
        float duration = 0.1f;
    };

    SpriteAnimation() = default;
    explicit SpriteAnimation(std::string name);

    /**
     * @brief Append a frame to the animation.
     * @param spriteIndex Zero-based index into the SpriteSheet.
     * @param duration Seconds this frame remains visible.
     * @throws std::invalid_argument if spriteIndex is negative or duration is not positive.
     */
    void addFrame(int spriteIndex, float duration = 0.1f);

    /**
     * @brief Set whether the animation loops back to the first frame.
     */
    void setLooping(bool loop);

    /**
     * @brief Check whether playback loops.
     */
    bool isLooping() const { return m_looping; }

    /**
     * @brief Set the clip name.
     */
    void setName(std::string name);

    /**
     * @brief Get the clip name.
     */
    const std::string& getName() const { return m_name; }

    /**
     * @brief Get the number of frames in this clip.
     */
    int getFrameCount() const { return static_cast<int>(m_frames.size()); }

    /**
     * @brief Get the full playback duration of one pass through this clip.
     */
    float getTotalDuration() const { return m_totalDuration; }

    /**
     * @brief Resolve which frame is active at a given elapsed time.
     *
     * Looping animations wrap around. Non-looping animations clamp to the
     * final frame once the elapsed time exceeds the total duration.
     *
     * @param time Elapsed playback time in seconds.
     * @return Zero-based frame index inside this clip.
     * @throws std::logic_error if the clip has no frames.
     */
    int getFrameAtTime(float time) const;

    /**
     * @brief Get a frame by index.
     * @param index Zero-based frame index.
     * @throws std::out_of_range if index is invalid.
     */
    const Frame& getFrame(int index) const;

  private:
    std::string m_name;
    std::vector<Frame> m_frames;
    float m_totalDuration = 0.0f;
    bool m_looping = true;
};

}  // namespace vde