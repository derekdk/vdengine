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

#include "ViewportRect.h"

namespace vde {

/**
 * @brief Entry describing a scene within a group, with optional viewport.
 */
struct SceneGroupEntry {
    /// Scene name (must match getScene() name).
    std::string sceneName;

    /// Viewport rectangle for this scene (default is full window).
    ViewportRect viewport = ViewportRect::fullWindow();
};

/**
 * @brief Describes a group of scenes that are active simultaneously.
 *
 * When a SceneGroup is set as active, the scheduler builds a task
 * graph that updates every scene in the group each frame.  The first
 * scene in the list is the "primary" scene and is the one whose
 * background color is used for the initial clear.
 *
 * @example
 * @code
 * // Simple group (all scenes get fullWindow viewport)
 * auto group = vde::SceneGroup::create("gameplay",
 *     {"world", "hud", "minimap"});
 * game.setActiveSceneGroup(group);
 *
 * // Group with explicit viewports for split-screen
 * auto group = vde::SceneGroup::createWithViewports("splitscreen", {
 *     {"player1", ViewportRect::leftHalf()},
 *     {"player2", ViewportRect::rightHalf()},
 * });
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

    /// Optional per-scene viewport entries.  When non-empty, each
    /// entry's viewport is applied to the corresponding scene.
    /// When empty, all scenes use fullWindow().
    std::vector<SceneGroupEntry> entries;

    /**
     * @brief Convenience factory.
     * @param groupName  Name for the group
     * @param scenes     Ordered list of scene names
     * @return A populated SceneGroup
     */
    static SceneGroup create(const std::string& groupName,
                             std::initializer_list<std::string> scenes) {
        SceneGroup group{groupName, scenes, {}};
        return group;
    }

    /**
     * @brief Convenience factory (vector overload).
     * @param groupName  Name for the group
     * @param scenes     Ordered list of scene names
     * @return A populated SceneGroup
     */
    static SceneGroup create(const std::string& groupName, const std::vector<std::string>& scenes) {
        SceneGroup group{groupName, scenes, {}};
        return group;
    }

    /**
     * @brief Factory that creates a group with explicit viewport assignments.
     * @param groupName  Name for the group
     * @param viewportEntries  Scene entries with viewport rects
     * @return A populated SceneGroup with viewport layout
     */
    static SceneGroup createWithViewports(const std::string& groupName,
                                          std::initializer_list<SceneGroupEntry> viewportEntries) {
        SceneGroup group;
        group.name = groupName;
        group.entries = viewportEntries;
        for (const auto& entry : group.entries) {
            group.sceneNames.push_back(entry.sceneName);
        }
        return group;
    }

    /**
     * @brief Factory that creates a group with explicit viewport assignments (vector overload).
     * @param groupName  Name for the group
     * @param viewportEntries  Scene entries with viewport rects
     * @return A populated SceneGroup with viewport layout
     */
    static SceneGroup createWithViewports(const std::string& groupName,
                                          const std::vector<SceneGroupEntry>& viewportEntries) {
        SceneGroup group;
        group.name = groupName;
        group.entries = viewportEntries;
        for (const auto& entry : group.entries) {
            group.sceneNames.push_back(entry.sceneName);
        }
        return group;
    }

    /**
     * @brief Check if the group has explicit viewport entries.
     */
    bool hasViewports() const { return !entries.empty(); }

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
