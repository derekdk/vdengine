/**
 * @file GameState_test.cpp
 * @brief CPU-only tests of Game-owned shared state and visibility.
 */

#include <vde/api/Game.h>
#include <vde/api/GameState.h>
#include <vde/api/Scene.h>

#include <limits>
#include <thread>

#include <gtest/gtest.h>

namespace vde::test {

struct SharedProgress {
    int score = 0;
    std::string name = "player";
};

TEST(GameState, TypedSceneAccessAndComposingWritesUseCommittedSnapshot) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SharedProgress{});
    game.addScene("a", std::make_unique<Scene>());
    game.addScene("b", std::make_unique<Scene>());
    auto* a = game.getScene("a");
    auto* b = game.getScene("b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    a->getSharedState().update<SharedProgress>([](auto& value) { value.score += 2; });
    EXPECT_EQ(b->state<SharedProgress>().score, 0);
    b->getSharedState().update<SharedProgress>([](auto& value) { value.score += 3; });
    EXPECT_EQ(a->state<SharedProgress>().score, 0);
    game.commitSharedState();
    EXPECT_EQ(a->state<SharedProgress>().score, 5);
    EXPECT_EQ(b->state<SharedProgress>().score, 5);
    EXPECT_THROW((void)shared.state<int>(), std::logic_error);
    EXPECT_THROW(shared.initialize(SharedProgress{}), std::logic_error);
}

TEST(GameState, SceneOrderDoesNotAffectReadsOrAdditiveWrites) {
    for (bool reverse : {false, true}) {
        Game game;
        game.getSharedState().initialize(SharedProgress{});
        for (int amount : reverse ? std::vector<int>{3, 2} : std::vector<int>{2, 3}) {
            EXPECT_EQ(game.state<SharedProgress>().score, 0);
            game.getSharedState().update<SharedProgress>(
                [amount](auto& value) { value.score += amount; });
        }
        game.commitSharedState();
        EXPECT_EQ(game.state<SharedProgress>().score, 5);
    }
}

TEST(GameState, TypedOnlyCommitPreservesLooseValuesWithoutNotifications) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SharedProgress{});
    shared.setValue("ready", true);
    game.commitSharedState();

    int notifications = 0;
    shared.subscribe([&](const auto&, const auto&, const auto&) { ++notifications; });
    shared.update<SharedProgress>([](auto& value) { value.score = 7; });
    game.commitSharedState();

    EXPECT_EQ(game.state<SharedProgress>().score, 7);
    EXPECT_EQ(shared.getScriptStateValue("ready"), 1.0);
    EXPECT_EQ(notifications, 0);
}

TEST(GameState, FailedMutationPreservesPendingState) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SharedProgress{});
    shared.update<SharedProgress>([](auto& value) { value.score = 7; });
    EXPECT_THROW(shared.update<SharedProgress>([](auto& value) {
        value.score = 99;
        throw std::runtime_error("failed mutation");
    }),
                 std::runtime_error);
    game.commitSharedState();
    EXPECT_EQ(game.state<SharedProgress>().score, 7);
    Scene detached;
    EXPECT_THROW((void)detached.state<SharedProgress>(), std::logic_error);
}

TEST(GameState, NotificationsObserveAtomicCommitAndStageNextFrame) {
    Game game;
    auto& shared = game.getSharedState();
    shared.initialize(SharedProgress{});
    int notifications = 0;
    shared.setValue("ready", true);
    game.commitSharedState();
    auto id = shared.subscribe([&](const auto& key, const auto& oldValue, const auto& newValue) {
        ++notifications;
        EXPECT_EQ(key, "score");
        EXPECT_FALSE(oldValue);
        ASSERT_TRUE(newValue);
        EXPECT_EQ(game.state<SharedProgress>().score, 4);
        EXPECT_EQ(shared.getScriptStateValue("ready"), 1.0);
        shared.setValue("next", true);
        EXPECT_FALSE(shared.getValue("next"));
        EXPECT_THROW(game.commitSharedState(), std::logic_error);
    });
    shared.update<SharedProgress>([](auto& value) { value.score = 4; });
    shared.setValue("SCORE", int64_t{4});
    game.commitSharedState();
    EXPECT_EQ(notifications, 1);
    shared.unsubscribe(id);
    game.commitSharedState();
    EXPECT_EQ(shared.getScriptStateValue("next"), 1.0);
}

TEST(GameState, CoalescesWritesAndSupportsRemovalAndUnsubscribeDuringDispatch) {
    Game game;
    auto& shared = game.getSharedState();
    int calls = 0;
    GameState::SubscriptionId id = 0;
    id = shared.subscribe([&](const auto&, const auto&, const auto&) {
        ++calls;
        shared.unsubscribe(id);
    });
    shared.setValue("x", int64_t{1});
    shared.setValue("x", int64_t{2});
    shared.setValue("y", true);
    game.commitSharedState();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(shared.getScriptStateValue("x"), 2.0);
    int removals = 0;
    shared.subscribe([&](const auto&, const auto& oldValue, const auto& newValue) {
        ++removals;
        EXPECT_TRUE(oldValue);
        EXPECT_FALSE(newValue);
    });
    shared.setValue("x", int64_t{2});
    shared.eraseValue("missing");
    game.commitSharedState();
    EXPECT_EQ(removals, 0);
    shared.eraseValue("x");
    game.commitSharedState();
    EXPECT_EQ(removals, 1);
}

TEST(GameState, ValidatesKeysValuesAndThreadOwnership) {
    Game game;
    auto& shared = game.getSharedState();
    EXPECT_THROW(shared.setValue("", true), std::invalid_argument);
    EXPECT_THROW(shared.setValue("x", std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
    EXPECT_THROW(shared.subscribe({}), std::invalid_argument);
    shared.setValue("name", std::string("Alice"));
    game.commitSharedState();
    EXPECT_FALSE(shared.getScriptStateValue("name"));
    EXPECT_FALSE(shared.getScriptStateValue("missing"));
    bool rejected = false;
    std::thread worker([&]() {
        try {
            shared.setValue("x", true);
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    worker.join();
    EXPECT_TRUE(rejected);
}

TEST(GameState, NotificationsPreserveMutableCaptureAcrossKeysAndCommits) {
    Game game;
    auto& shared = game.getSharedState();
    int observed = 0;
    shared.subscribe([count = 0, &observed](const auto&, const auto&, const auto&) mutable {
        observed = ++count;
    });
    shared.setValue("x", true);
    shared.setValue("y", true);
    game.commitSharedState();
    EXPECT_EQ(observed, 2);
    shared.setValue("x", false);
    game.commitSharedState();
    EXPECT_EQ(observed, 3);
}

}  // namespace vde::test
