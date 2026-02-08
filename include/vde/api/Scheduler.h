#pragma once

/**
 * @file Scheduler.h
 * @brief Task scheduler for VDE game loop
 *
 * Provides a single-threaded task graph scheduler that executes
 * tasks in topologically sorted order with phase-based tiebreaking.
 */

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vde {

/**
 * @brief Unique identifier for a scheduled task.
 */
using TaskId = uint32_t;

/**
 * @brief Sentinel value indicating an invalid task ID.
 */
constexpr TaskId INVALID_TASK_ID = 0;

/**
 * @brief Execution phases that determine task ordering as a tiebreaker.
 *
 * When two tasks have no dependency relationship, they are ordered
 * by phase. Lower-valued phases execute first.
 */
enum class TaskPhase : uint8_t {
    Input = 0,        ///< Input processing
    GameLogic = 1,    ///< Game logic / scene update
    Audio = 2,        ///< Audio processing
    Physics = 3,      ///< Physics simulation
    PostPhysics = 4,  ///< Post-physics sync (e.g., transform copy)
    PreRender = 5,    ///< Pre-render setup (camera, lights)
    Render = 6        ///< Rendering
};

/**
 * @brief Describes a task to be scheduled.
 */
struct TaskDescriptor {
    std::string name;                        ///< Human-readable task name
    TaskPhase phase = TaskPhase::GameLogic;  ///< Execution phase (tiebreaker)
    std::function<void()> work;              ///< The work to execute
    std::vector<TaskId> dependsOn;           ///< Tasks that must complete before this one
    bool mainThreadOnly = true;              ///< Must run on the main thread (future use)
};

/**
 * @brief Single-threaded task graph scheduler.
 *
 * Tasks are registered with dependencies and phases. On execute(),
 * they are topologically sorted (with phase as tiebreaker) and
 * run sequentially.
 *
 * @example
 * @code
 * vde::Scheduler scheduler;
 * auto inputTask = scheduler.addTask({"input", vde::TaskPhase::Input, [&]{ processInput(); }});
 * auto updateTask = scheduler.addTask({"update", vde::TaskPhase::GameLogic, [&]{ update(dt); },
 * {inputTask}}); auto renderTask = scheduler.addTask({"render", vde::TaskPhase::Render, [&]{
 * render(); }, {updateTask}}); scheduler.execute();
 * @endcode
 */
class Scheduler {
  public:
    Scheduler();
    ~Scheduler();

    /**
     * @brief Add a task to the scheduler.
     * @param descriptor Task description including name, phase, work, and dependencies
     * @return Unique task ID
     * @throws std::invalid_argument if a dependency references an unknown task ID
     */
    TaskId addTask(const TaskDescriptor& descriptor);

    /**
     * @brief Remove a task by ID.
     *
     * Also removes this task from other tasks' dependency lists.
     * @param id Task to remove
     */
    void removeTask(TaskId id);

    /**
     * @brief Remove all tasks from the scheduler.
     */
    void clear();

    /**
     * @brief Execute all tasks in topologically sorted order.
     *
     * Tasks are sorted by dependency order, with phase as a tiebreaker
     * for tasks that have no ordering constraint between them.
     *
     * @throws std::runtime_error if the task graph contains a cycle
     */
    void execute();

    /**
     * @brief Get the number of registered tasks.
     */
    size_t getTaskCount() const;

    /**
     * @brief Check if a task with the given ID exists.
     * @param id Task ID to check
     * @return true if the task exists
     */
    bool hasTask(TaskId id) const;

    /**
     * @brief Get the name of a task.
     * @param id Task ID
     * @return Task name, or empty string if not found
     */
    std::string getTaskName(TaskId id) const;

    /**
     * @brief Get the execution order from the last execute() call.
     *
     * Useful for debugging and testing.
     * @return Ordered list of task IDs as they were executed
     */
    const std::vector<TaskId>& getLastExecutionOrder() const;

  private:
    struct TaskEntry {
        TaskId id;
        TaskDescriptor descriptor;
    };

    TaskId m_nextId = 1;
    std::unordered_map<TaskId, TaskEntry> m_tasks;
    std::vector<TaskId> m_lastExecutionOrder;

    /**
     * @brief Topologically sort the task graph.
     * @return Sorted list of task IDs
     * @throws std::runtime_error if a cycle is detected
     */
    std::vector<TaskId> topologicalSort() const;
};

}  // namespace vde
