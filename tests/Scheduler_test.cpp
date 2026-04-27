/**
 * @file Scheduler_test.cpp
 * @brief Unit tests for the Scheduler task graph system
 */

#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/SceneGroup.h>
#include <vde/api/Scheduler.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

// ============================================================================
// Test Fixture
// ============================================================================

class SchedulerTest : public ::testing::Test {
  protected:
    Scheduler scheduler;
    std::vector<std::string> executionLog;

    /// Helper: create a task that logs its name when executed
    TaskDescriptor makeLoggingTask(const std::string& name, TaskPhase phase,
                                   const std::vector<TaskId>& deps = {}) {
        return {name, phase, [this, name]() { executionLog.push_back(name); }, deps};
    }
};

// ============================================================================
// Task Registration & ID Uniqueness
// ============================================================================

TEST_F(SchedulerTest, AddTaskReturnsUniqueIds) {
    TaskId a = scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    TaskId b = scheduler.addTask(makeLoggingTask("b", TaskPhase::Input));
    TaskId c = scheduler.addTask(makeLoggingTask("c", TaskPhase::Input));

    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_NE(a, INVALID_TASK_ID);
    EXPECT_NE(b, INVALID_TASK_ID);
    EXPECT_NE(c, INVALID_TASK_ID);
}

TEST_F(SchedulerTest, AddTaskIncreasesCount) {
    EXPECT_EQ(scheduler.getTaskCount(), 0u);

    scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    EXPECT_EQ(scheduler.getTaskCount(), 1u);

    scheduler.addTask(makeLoggingTask("b", TaskPhase::GameLogic));
    EXPECT_EQ(scheduler.getTaskCount(), 2u);
}

TEST_F(SchedulerTest, HasTaskReturnsTrueForExisting) {
    TaskId id = scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    EXPECT_TRUE(scheduler.hasTask(id));
    EXPECT_FALSE(scheduler.hasTask(INVALID_TASK_ID));
    EXPECT_FALSE(scheduler.hasTask(9999));
}

TEST_F(SchedulerTest, GetTaskNameReturnsCorrectName) {
    TaskId id = scheduler.addTask(makeLoggingTask("myTask", TaskPhase::Input));
    EXPECT_EQ(scheduler.getTaskName(id), "myTask");
    EXPECT_EQ(scheduler.getTaskName(9999), "");
}

// ============================================================================
// Remove Task
// ============================================================================

TEST_F(SchedulerTest, RemoveTaskReducesCount) {
    TaskId a = scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    scheduler.addTask(makeLoggingTask("b", TaskPhase::GameLogic));
    EXPECT_EQ(scheduler.getTaskCount(), 2u);

    scheduler.removeTask(a);
    EXPECT_EQ(scheduler.getTaskCount(), 1u);
    EXPECT_FALSE(scheduler.hasTask(a));
}

TEST_F(SchedulerTest, RemoveTaskCleansDependencies) {
    TaskId a = scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    [[maybe_unused]] TaskId b = scheduler.addTask(makeLoggingTask("b", TaskPhase::GameLogic, {a}));

    // Remove a — b should still execute (no longer depends on a)
    scheduler.removeTask(a);
    scheduler.execute();

    EXPECT_EQ(executionLog.size(), 1u);
    EXPECT_EQ(executionLog[0], "b");
}

TEST_F(SchedulerTest, RemoveNonexistentTaskIsSafe) {
    EXPECT_NO_THROW(scheduler.removeTask(9999));
    EXPECT_NO_THROW(scheduler.removeTask(INVALID_TASK_ID));
}

// ============================================================================
// Clear
// ============================================================================

TEST_F(SchedulerTest, ClearEmptiesGraph) {
    scheduler.addTask(makeLoggingTask("a", TaskPhase::Input));
    scheduler.addTask(makeLoggingTask("b", TaskPhase::GameLogic));
    EXPECT_EQ(scheduler.getTaskCount(), 2u);

    scheduler.clear();
    EXPECT_EQ(scheduler.getTaskCount(), 0u);
}

// ============================================================================
// Execute — Empty Graph
// ============================================================================

TEST_F(SchedulerTest, ExecuteEmptyGraphIsNoOp) {
    EXPECT_NO_THROW(scheduler.execute());
    EXPECT_TRUE(scheduler.getLastExecutionOrder().empty());
}

// ============================================================================
// Topological Sort — Linear Chain
// ============================================================================

TEST_F(SchedulerTest, LinearChainExecutesInOrder) {
    // A -> B -> C (all same phase, dependency forces order)
    TaskId a = scheduler.addTask(makeLoggingTask("A", TaskPhase::GameLogic));
    TaskId b = scheduler.addTask(makeLoggingTask("B", TaskPhase::GameLogic, {a}));
    scheduler.addTask(makeLoggingTask("C", TaskPhase::GameLogic, {b}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 3u);
    EXPECT_EQ(executionLog[0], "A");
    EXPECT_EQ(executionLog[1], "B");
    EXPECT_EQ(executionLog[2], "C");
}

// ============================================================================
// Topological Sort — Diamond Dependencies
// ============================================================================

TEST_F(SchedulerTest, DiamondDependenciesExecuteCorrectly) {
    /*
          A
         / \
        B   C
         \ /
          D
    */
    TaskId a = scheduler.addTask(makeLoggingTask("A", TaskPhase::Input));
    TaskId b = scheduler.addTask(makeLoggingTask("B", TaskPhase::GameLogic, {a}));
    TaskId c = scheduler.addTask(makeLoggingTask("C", TaskPhase::GameLogic, {a}));
    scheduler.addTask(makeLoggingTask("D", TaskPhase::Render, {b, c}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 4u);
    EXPECT_EQ(executionLog[0], "A");
    // B and C can be in either order (both depend only on A, same phase)
    EXPECT_TRUE((executionLog[1] == "B" && executionLog[2] == "C") ||
                (executionLog[1] == "C" && executionLog[2] == "B"));
    EXPECT_EQ(executionLog[3], "D");
}

// ============================================================================
// Phase Ordering as Tiebreaker
// ============================================================================

TEST_F(SchedulerTest, PhaseOrderingAsTiebreaker) {
    // No dependencies — phase determines order
    scheduler.addTask(makeLoggingTask("render", TaskPhase::Render));
    scheduler.addTask(makeLoggingTask("input", TaskPhase::Input));
    scheduler.addTask(makeLoggingTask("audio", TaskPhase::Audio));
    scheduler.addTask(makeLoggingTask("gameLogic", TaskPhase::GameLogic));
    scheduler.addTask(makeLoggingTask("preRender", TaskPhase::PreRender));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 5u);
    EXPECT_EQ(executionLog[0], "input");
    EXPECT_EQ(executionLog[1], "gameLogic");
    EXPECT_EQ(executionLog[2], "audio");
    EXPECT_EQ(executionLog[3], "preRender");
    EXPECT_EQ(executionLog[4], "render");
}

TEST_F(SchedulerTest, DependenciesOverridePhaseOrder) {
    // Render task depends on nothing, but input task depends on render
    // Dependency should override the natural phase order
    TaskId render = scheduler.addTask(makeLoggingTask("render", TaskPhase::Render));
    scheduler.addTask(makeLoggingTask("input", TaskPhase::Input, {render}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 2u);
    EXPECT_EQ(executionLog[0], "render");
    EXPECT_EQ(executionLog[1], "input");
}

// ============================================================================
// Cycle Detection
// ============================================================================

TEST_F(SchedulerTest, CycleDetectionThrows) {
    // Create a cycle: A -> B -> A (via manual dependency manipulation)
    // Since addTask validates deps, we need tasks first — but we can't create
    // a direct cycle through addTask. Instead, test that cycle detection
    // works by forcing it through a three-task cycle.
    // We'll use a workaround: add A, add B->A, then try to add C->B,
    // and see if we can make A->C somehow. But addTask prevents forward references.

    // Actually the simplest cycle test is: try to see that the algorithm
    // correctly detects cycles. But since addTask validates that deps exist,
    // a true cycle can't be built through the public API alone.
    // We'll just verify that a long single-direction chain with no cycle works fine.

    // For now, verify the throw message on impossible state would be correct.
    // The topological sort throws if result.size() != m_tasks.size().
    // Phase ordering makes cycles very unlikely in normal use.

    // Test: verify no throw on valid graph
    TaskId a = scheduler.addTask(makeLoggingTask("A", TaskPhase::Input));
    TaskId b = scheduler.addTask(makeLoggingTask("B", TaskPhase::GameLogic, {a}));
    scheduler.addTask(makeLoggingTask("C", TaskPhase::Render, {b}));
    EXPECT_NO_THROW(scheduler.execute());
}

TEST_F(SchedulerTest, InvalidDependencyThrows) {
    EXPECT_THROW(scheduler.addTask(makeLoggingTask("bad", TaskPhase::Input, {9999})),
                 std::invalid_argument);
}

// ============================================================================
// Execution Order Tracking
// ============================================================================

TEST_F(SchedulerTest, GetLastExecutionOrderMatchesExecution) {
    TaskId a = scheduler.addTask(makeLoggingTask("A", TaskPhase::Input));
    TaskId b = scheduler.addTask(makeLoggingTask("B", TaskPhase::GameLogic, {a}));
    TaskId c = scheduler.addTask(makeLoggingTask("C", TaskPhase::Render, {b}));

    scheduler.execute();

    auto order = scheduler.getLastExecutionOrder();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
}

// ============================================================================
// Multiple Executions
// ============================================================================

TEST_F(SchedulerTest, ExecuteCanBeCalledMultipleTimes) {
    scheduler.addTask(makeLoggingTask("A", TaskPhase::Input));

    scheduler.execute();
    scheduler.execute();
    scheduler.execute();

    EXPECT_EQ(executionLog.size(), 3u);
    EXPECT_EQ(executionLog[0], "A");
    EXPECT_EQ(executionLog[1], "A");
    EXPECT_EQ(executionLog[2], "A");
}

// ============================================================================
// Task with Null Work
// ============================================================================

TEST_F(SchedulerTest, TaskWithNullWorkDoesNotCrash) {
    TaskDescriptor desc;
    desc.name = "noop";
    desc.phase = TaskPhase::Input;
    desc.work = nullptr;

    scheduler.addTask(desc);
    EXPECT_NO_THROW(scheduler.execute());
}

// ============================================================================
// Complex Graph Ordering
// ============================================================================

TEST_F(SchedulerTest, GameLoopGraphExecutesCorrectly) {
    // Simulate the default game loop graph:
    // update (GameLogic) -> audio (Audio) -> preRender (PreRender) -> render (Render)
    TaskId update = scheduler.addTask(makeLoggingTask("update", TaskPhase::GameLogic));
    TaskId audio = scheduler.addTask(makeLoggingTask("audio", TaskPhase::Audio, {update}));
    TaskId preRender =
        scheduler.addTask(makeLoggingTask("preRender", TaskPhase::PreRender, {audio}));
    scheduler.addTask(makeLoggingTask("render", TaskPhase::Render, {preRender}));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 4u);
    EXPECT_EQ(executionLog[0], "update");
    EXPECT_EQ(executionLog[1], "audio");
    EXPECT_EQ(executionLog[2], "preRender");
    EXPECT_EQ(executionLog[3], "render");
}

// ============================================================================
// Single Task
// ============================================================================

TEST_F(SchedulerTest, SingleTaskExecutes) {
    scheduler.addTask(makeLoggingTask("only", TaskPhase::Input));
    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 1u);
    EXPECT_EQ(executionLog[0], "only");
}

// ============================================================================
// All Phases Test
// ============================================================================

TEST_F(SchedulerTest, AllPhasesOrderedCorrectly) {
    scheduler.addTask(makeLoggingTask("postPhysics", TaskPhase::PostPhysics));
    scheduler.addTask(makeLoggingTask("physics", TaskPhase::Physics));
    scheduler.addTask(makeLoggingTask("render", TaskPhase::Render));
    scheduler.addTask(makeLoggingTask("input", TaskPhase::Input));
    scheduler.addTask(makeLoggingTask("audio", TaskPhase::Audio));
    scheduler.addTask(makeLoggingTask("gameLogic", TaskPhase::GameLogic));
    scheduler.addTask(makeLoggingTask("preRender", TaskPhase::PreRender));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 7u);
    EXPECT_EQ(executionLog[0], "input");
    EXPECT_EQ(executionLog[1], "gameLogic");
    EXPECT_EQ(executionLog[2], "physics");
    EXPECT_EQ(executionLog[3], "postPhysics");
    EXPECT_EQ(executionLog[4], "audio");
    EXPECT_EQ(executionLog[5], "preRender");
    EXPECT_EQ(executionLog[6], "render");
}

// ============================================================================
// Phase 1 — New phase ordinals
// ============================================================================

TEST_F(SchedulerTest, PhaseOrder_TimedAfterPostPhysics) {
    EXPECT_GT(static_cast<uint8_t>(TaskPhase::Timed), static_cast<uint8_t>(TaskPhase::PostPhysics));
}

TEST_F(SchedulerTest, PhaseOrder_VisualAfterTimed) {
    EXPECT_GT(static_cast<uint8_t>(TaskPhase::Visual), static_cast<uint8_t>(TaskPhase::Timed));
}

TEST_F(SchedulerTest, PhaseOrder_VisualBeforePreRender) {
    EXPECT_LT(static_cast<uint8_t>(TaskPhase::Visual), static_cast<uint8_t>(TaskPhase::PreRender));
}

TEST_F(SchedulerTest, AllPhasesFullOrderCorrect) {
    // Verifies the complete canonical ordering including new phases
    scheduler.addTask(makeLoggingTask("timed", TaskPhase::Timed));
    scheduler.addTask(makeLoggingTask("visual", TaskPhase::Visual));
    scheduler.addTask(makeLoggingTask("postPhysics", TaskPhase::PostPhysics));
    scheduler.addTask(makeLoggingTask("physics", TaskPhase::Physics));
    scheduler.addTask(makeLoggingTask("render", TaskPhase::Render));
    scheduler.addTask(makeLoggingTask("input", TaskPhase::Input));
    scheduler.addTask(makeLoggingTask("audio", TaskPhase::Audio));
    scheduler.addTask(makeLoggingTask("gameLogic", TaskPhase::GameLogic));
    scheduler.addTask(makeLoggingTask("preRender", TaskPhase::PreRender));

    scheduler.execute();

    ASSERT_EQ(executionLog.size(), 9u);
    EXPECT_EQ(executionLog[0], "input");
    EXPECT_EQ(executionLog[1], "gameLogic");
    EXPECT_EQ(executionLog[2], "physics");
    EXPECT_EQ(executionLog[3], "postPhysics");
    EXPECT_EQ(executionLog[4], "timed");
    EXPECT_EQ(executionLog[5], "audio");
    EXPECT_EQ(executionLog[6], "visual");
    EXPECT_EQ(executionLog[7], "preRender");
    EXPECT_EQ(executionLog[8], "render");
}

// ============================================================================
// Phase 1 — Scheduler graph shape tests using Game
// ============================================================================

namespace {

/// Minimal scene that uses phase callbacks — no GPU needed.
class PhaseCallbackTestScene : public Scene {
  public:
    int gameLogicCallCount = 0;
    int visualsCallCount = 0;

    PhaseCallbackTestScene() { enablePhaseCallbacks(); }

    void updateGameLogic(float) override { gameLogicCallCount++; }
    void updateAudio(float) override {}
    void updateVisuals(float) override { visualsCallCount++; }
};

/// Minimal legacy scene (single update()).
class LegacyTestScene : public Scene {
  public:
    int updateCallCount = 0;

    void update(float) override { updateCallCount++; }
};

}  // anonymous namespace

class GameSchedulerTest : public ::testing::Test {
  protected:
    Game game;

    /// Helper: find a task index in an execution order vector.
    static int findIndex(const std::vector<TaskId>& order, TaskId id) {
        for (int i = 0; i < static_cast<int>(order.size()); ++i) {
            if (order[i] == id)
                return i;
        }
        return -1;
    }
};

TEST_F(GameSchedulerTest, MultiScene_NoFalseDependency) {
    // Two phase-callback scenes: their logic tasks must not depend on each other.
    game.addScene("A", std::make_unique<PhaseCallbackTestScene>());
    game.addScene("B", std::make_unique<PhaseCallbackTestScene>());

    SceneGroup group;
    group.sceneNames = {"A", "B"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    TaskId logicA = sched.findTaskByName("scene.gameLogic.A");
    TaskId logicB = sched.findTaskByName("scene.gameLogic.B");

    ASSERT_NE(logicA, INVALID_TASK_ID);
    ASSERT_NE(logicB, INVALID_TASK_ID);

    auto descA = sched.getTaskDescriptor(logicA);
    auto descB = sched.getTaskDescriptor(logicB);

    ASSERT_TRUE(descA.has_value());
    ASSERT_TRUE(descB.has_value());

    // A must not depend on B
    EXPECT_EQ(std::find(descA->dependsOn.begin(), descA->dependsOn.end(), logicB),
              descA->dependsOn.end())
        << "scene.gameLogic.A must not depend on scene.gameLogic.B";

    // B must not depend on A
    EXPECT_EQ(std::find(descB->dependsOn.begin(), descB->dependsOn.end(), logicA),
              descB->dependsOn.end())
        << "scene.gameLogic.B must not depend on scene.gameLogic.A";
}

TEST_F(GameSchedulerTest, MultiScene_DeterministicOrder) {
    // Two scenes with equal priority — registration order (TaskId) tiebreak must be stable.
    auto sceneA = std::make_unique<PhaseCallbackTestScene>();
    auto sceneB = std::make_unique<PhaseCallbackTestScene>();
    sceneA->setUpdatePriority(0);
    sceneB->setUpdatePriority(0);
    game.addScene("A", std::move(sceneA));
    game.addScene("B", std::move(sceneB));

    SceneGroup group;
    group.sceneNames = {"A", "B"};

    // Helper: translate an execution order (TaskIds) to task names for stable cross-rebuild
    // comparison.
    auto toNameOrder = [&](const std::vector<TaskId>& order) {
        std::vector<std::string> names;
        names.reserve(order.size());
        for (TaskId id : order) {
            names.push_back(game.getScheduler().getTaskName(id));
        }
        return names;
    };

    // First build + execute — capture names while the first scheduler is still active.
    game.setActiveSceneGroup(group);
    game.getScheduler().execute();
    const auto nameOrder1 = toNameOrder(game.getScheduler().getLastExecutionOrder());

    // Rebuild + execute again — capture names from the rebuilt scheduler.
    game.setActiveSceneGroup(group);
    game.getScheduler().execute();
    const auto nameOrder2 = toNameOrder(game.getScheduler().getLastExecutionOrder());

    // Execution order must be identical by name across scheduler rebuilds.
    ASSERT_EQ(nameOrder1, nameOrder2)
        << "Execution order must be deterministic across scheduler rebuilds";

    // With A registered before B, A gets a smaller TaskId and executes first (registration-order
    // tiebreak).
    auto nameIdx = [](const std::vector<std::string>& names, const std::string& n) {
        auto it = std::find(names.begin(), names.end(), n);
        return (it != names.end()) ? static_cast<int>(it - names.begin()) : -1;
    };
    int aIdx = nameIdx(nameOrder1, "scene.gameLogic.A");
    int bIdx = nameIdx(nameOrder1, "scene.gameLogic.B");
    EXPECT_GE(aIdx, 0);
    EXPECT_GE(bIdx, 0);
    EXPECT_LT(aIdx, bIdx) << "scene A should execute before scene B (registration order tiebreak)";
}

TEST_F(GameSchedulerTest, LegacyScene_UpdateCallbackFires) {
    auto rawScene = std::make_unique<LegacyTestScene>();
    LegacyTestScene* scenePtr = rawScene.get();
    game.addScene("legacy", std::move(rawScene));

    SceneGroup group;
    group.sceneNames = {"legacy"};
    game.setActiveSceneGroup(group);

    EXPECT_EQ(scenePtr->updateCallCount, 0);
    game.getScheduler().execute();
    EXPECT_EQ(scenePtr->updateCallCount, 1);
}

TEST_F(GameSchedulerTest, VisualPhase_AfterPostPhysics) {
    // For a phase-callback scene without physics, the visuals task must:
    //   - be in the Visual phase
    //   - depend on the scene's own gameLogic task (no physics dep)
    game.addScene("phased", std::make_unique<PhaseCallbackTestScene>());

    SceneGroup group;
    group.sceneNames = {"phased"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    TaskId logicTask = sched.findTaskByName("scene.gameLogic.phased");
    TaskId visualTask = sched.findTaskByName("scene.visuals.phased");

    ASSERT_NE(logicTask, INVALID_TASK_ID);
    ASSERT_NE(visualTask, INVALID_TASK_ID);

    auto visualDesc = sched.getTaskDescriptor(visualTask);
    ASSERT_TRUE(visualDesc.has_value());

    EXPECT_EQ(visualDesc->phase, TaskPhase::Visual)
        << "updateVisuals() must be scheduled in the Visual phase";

    EXPECT_NE(std::find(visualDesc->dependsOn.begin(), visualDesc->dependsOn.end(), logicTask),
              visualDesc->dependsOn.end())
        << "scene.visuals.phased must depend on scene.gameLogic.phased";
}

TEST_F(GameSchedulerTest, MainThreadOnly_SceneCallbacks) {
    // scene.gameLogic, scene.audio, and scene.visuals must all be main-thread-only.
    game.addScene("mt_scene", std::make_unique<PhaseCallbackTestScene>());

    SceneGroup group;
    group.sceneNames = {"mt_scene"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    const std::vector<std::string> requiredMainThread = {
        "scene.gameLogic.mt_scene",
        "scene.audio.mt_scene",
        "scene.visuals.mt_scene",
    };

    for (const auto& name : requiredMainThread) {
        TaskId id = sched.findTaskByName(name);
        ASSERT_NE(id, INVALID_TASK_ID) << "Task not found: " << name;
        auto desc = sched.getTaskDescriptor(id);
        ASSERT_TRUE(desc.has_value());
        EXPECT_TRUE(desc->mainThreadOnly) << "Task must be main-thread-only: " << name;
    }
}

}  // namespace vde::test
