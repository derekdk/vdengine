#pragma once

/**
 * @file SceneGroup.h
 * @brief Scene group for simultaneous multi-scene updates
 *
 * Provides the SceneGroup struct that describes a set of scenes
 * to be active simultaneously. The scheduler builds a task graph
 * that updates every scene in the group each frame.
 */

#include <initializer_list>
#include <string>
#include <vector>

namespace vde {

/**
 * @brief Describes a group of scenes that are active simultaneously.
 *
 * When a SceneGroup is set as active, the scheduler builds a task
 * graph that updates every scene in the group each frame.  The first
 * scene in the list is the "primary" scene and is the one whose
 * camera and background color are used for rendering.
 *
 * @example
 * @code
 * auto group = vde::SceneGroup::create("gameplay",
 *     {"world", "hud", "minimap"});
 * game.setActiveSceneGroup(group);
 * @endcode
 */
struct SceneGroup {
    /// Human-readable name for the group.
    std::string name;

    /// Ordered list of scene names.  The first entry is the primary
    /// (rendered) scene; the rest receive update() calls but do not
    /// control the camera or clear color.
    std::vector<std::string> sceneNames;

    /**
     * @brief Convenience factory.
     * @param groupName  Name for the group
     * @param scenes     Ordered list of scene names
     * @return A populated SceneGroup
     */
    static SceneGroup create(const std::string& groupName,
                             std::initializer_list<std::string> scenes) {
        return SceneGroup{groupName, scenes};
    }

    /**
     * @brief Convenience factory (vector overload).
     * @param groupName  Name for the group
     * @param scenes     Ordered list of scene names
     * @return A populated SceneGroup
     */
    static SceneGroup create(const std::string& groupName, const std::vector<std::string>& scenes) {
        return SceneGroup{groupName, scenes};
    }

    /**
     * @brief Check if the group is empty (contains no scenes).
     */
    bool empty() const { return sceneNames.empty(); }

    /**
     * @brief Get the number of scenes in the group.
     */
    size_t size() const { return sceneNames.size(); }
};

}  // namespace vde
