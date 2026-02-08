/**
 * @file Scheduler.cpp
 * @brief Implementation of the task graph scheduler
 */

#include <vde/api/Scheduler.h>

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace vde {

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

TaskId Scheduler::addTask(const TaskDescriptor& descriptor) {
    // Validate dependencies exist
    for (TaskId dep : descriptor.dependsOn) {
        if (m_tasks.find(dep) == m_tasks.end()) {
            throw std::invalid_argument("Scheduler::addTask: dependency task ID " +
                                        std::to_string(dep) + " does not exist");
        }
    }

    TaskId id = m_nextId++;
    m_tasks[id] = TaskEntry{id, descriptor};
    return id;
}

void Scheduler::removeTask(TaskId id) {
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) {
        return;
    }

    // Remove this task from all other tasks' dependency lists
    for (auto& [otherId, entry] : m_tasks) {
        auto& deps = entry.descriptor.dependsOn;
        deps.erase(std::remove(deps.begin(), deps.end(), id), deps.end());
    }

    m_tasks.erase(it);
}

void Scheduler::clear() {
    m_tasks.clear();
    m_lastExecutionOrder.clear();
}

void Scheduler::execute() {
    if (m_tasks.empty()) {
        m_lastExecutionOrder.clear();
        return;
    }

    auto sorted = topologicalSort();
    m_lastExecutionOrder = sorted;

    for (TaskId id : sorted) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end() && it->second.descriptor.work) {
            it->second.descriptor.work();
        }
    }
}

size_t Scheduler::getTaskCount() const {
    return m_tasks.size();
}

bool Scheduler::hasTask(TaskId id) const {
    return m_tasks.find(id) != m_tasks.end();
}

std::string Scheduler::getTaskName(TaskId id) const {
    auto it = m_tasks.find(id);
    if (it != m_tasks.end()) {
        return it->second.descriptor.name;
    }
    return "";
}

const std::vector<TaskId>& Scheduler::getLastExecutionOrder() const {
    return m_lastExecutionOrder;
}

std::vector<TaskId> Scheduler::topologicalSort() const {
    // Kahn's algorithm with phase-based tiebreaking via priority queue
    // Build in-degree map and adjacency list
    std::unordered_map<TaskId, int> inDegree;
    std::unordered_map<TaskId, std::vector<TaskId>> dependents;  // task -> tasks that depend on it

    for (const auto& [id, entry] : m_tasks) {
        if (inDegree.find(id) == inDegree.end()) {
            inDegree[id] = 0;
        }
        for (TaskId dep : entry.descriptor.dependsOn) {
            // dep -> id (id depends on dep)
            dependents[dep].push_back(id);
            inDegree[id]++;
        }
    }

    // Priority queue: smaller phase first, then smaller task ID as tiebreaker
    // Using a min-heap with (phase, taskId)
    auto cmp = [this](TaskId a, TaskId b) {
        auto itA = m_tasks.find(a);
        auto itB = m_tasks.find(b);
        uint8_t phaseA = static_cast<uint8_t>(itA->second.descriptor.phase);
        uint8_t phaseB = static_cast<uint8_t>(itB->second.descriptor.phase);
        if (phaseA != phaseB) {
            return phaseA > phaseB;  // min-heap: smaller phase = higher priority
        }
        return a > b;  // min-heap: smaller ID = higher priority (stable registration order)
    };

    std::priority_queue<TaskId, std::vector<TaskId>, decltype(cmp)> ready(cmp);

    // Seed with zero in-degree tasks
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            ready.push(id);
        }
    }

    std::vector<TaskId> result;
    result.reserve(m_tasks.size());

    while (!ready.empty()) {
        TaskId current = ready.top();
        ready.pop();
        result.push_back(current);

        auto depIt = dependents.find(current);
        if (depIt != dependents.end()) {
            for (TaskId dependent : depIt->second) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    ready.push(dependent);
                }
            }
        }
    }

    if (result.size() != m_tasks.size()) {
        throw std::runtime_error("Scheduler::execute: cycle detected in task graph");
    }

    return result;
}

}  // namespace vde
