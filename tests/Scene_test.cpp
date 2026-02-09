/**
 * @file Scene_test.cpp
 * @brief Unit tests for Scene class (Phase 1)
 *
 * Tests Scene entity management, camera, lighting, and world bounds.
 */

#include <vde/api/CameraBounds.h>
#include <vde/api/Entity.h>
#include <vde/api/GameCamera.h>
#include <vde/api/LightBox.h>
#include <vde/api/Scene.h>
#include <vde/api/ViewportRect.h>
#include <vde/api/WorldBounds.h>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// Scene Basic Tests
// ============================================================================

class SceneTest : public ::testing::Test {
  protected:
    std::unique_ptr<Scene> scene;

    void SetUp() override { scene = std::make_unique<Scene>(); }
};

TEST_F(SceneTest, DefaultConstructor) {
    EXPECT_NE(scene.get(), nullptr);
}

TEST_F(SceneTest, GetEntitiesEmptyByDefault) {
    EXPECT_TRUE(scene->getEntities().empty());
}

// ============================================================================
// Entity Management Tests
// ============================================================================

TEST_F(SceneTest, AddEntityReturnsSharedPtr) {
    auto entity = scene->addEntity<MeshEntity>();
    EXPECT_NE(entity, nullptr);
}

TEST_F(SceneTest, AddEntityIncrementsCount) {
    EXPECT_EQ(scene->getEntities().size(), 0);

    scene->addEntity<MeshEntity>();
    EXPECT_EQ(scene->getEntities().size(), 1);

    scene->addEntity<MeshEntity>();
    EXPECT_EQ(scene->getEntities().size(), 2);
}

TEST_F(SceneTest, AddEntityRef) {
    auto entity = std::make_shared<MeshEntity>();
    EntityId id = scene->addEntity(entity);

    EXPECT_GT(id, 0);
    EXPECT_EQ(scene->getEntities().size(), 1);
}

TEST_F(SceneTest, GetEntityById) {
    auto entity = scene->addEntity<MeshEntity>();
    EntityId id = entity->getId();

    Entity* found = scene->getEntity(id);
    EXPECT_EQ(found, entity.get());
}

TEST_F(SceneTest, GetEntityByIdNotFound) {
    Entity* found = scene->getEntity(99999);
    EXPECT_EQ(found, nullptr);
}

TEST_F(SceneTest, GetEntityByName) {
    auto entity = scene->addEntity<MeshEntity>();
    entity->setName("TestEntity");

    Entity* found = scene->getEntityByName("TestEntity");
    EXPECT_EQ(found, entity.get());
}

TEST_F(SceneTest, GetEntityByNameNotFound) {
    Entity* found = scene->getEntityByName("NonExistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(SceneTest, RemoveEntityById) {
    auto entity = scene->addEntity<MeshEntity>();
    EntityId id = entity->getId();

    EXPECT_EQ(scene->getEntities().size(), 1);
    scene->removeEntity(id);
    EXPECT_EQ(scene->getEntities().size(), 0);
}

TEST_F(SceneTest, ClearEntitiesRemovesAll) {
    scene->addEntity<MeshEntity>();
    scene->addEntity<MeshEntity>();
    scene->addEntity<MeshEntity>();

    EXPECT_EQ(scene->getEntities().size(), 3);
    scene->clearEntities();
    EXPECT_EQ(scene->getEntities().size(), 0);
}

TEST_F(SceneTest, GetEntitiesOfType) {
    scene->addEntity<MeshEntity>();
    scene->addEntity<SpriteEntity>();
    scene->addEntity<MeshEntity>();

    auto meshEntities = scene->getEntitiesOfType<MeshEntity>();
    EXPECT_EQ(meshEntities.size(), 2);

    auto spriteEntities = scene->getEntitiesOfType<SpriteEntity>();
    EXPECT_EQ(spriteEntities.size(), 1);
}

// ============================================================================
// Camera Tests
// ============================================================================

TEST_F(SceneTest, SetCameraUniquePtr) {
    auto camera = std::make_unique<OrbitCamera>();
    OrbitCamera* rawPtr = camera.get();
    scene->setCamera(std::move(camera));

    EXPECT_EQ(scene->getCamera(), rawPtr);
}

TEST_F(SceneTest, SetCameraRawPointer) {
    OrbitCamera* camera = new OrbitCamera();
    scene->setCamera(camera);

    EXPECT_EQ(scene->getCamera(), camera);
}

TEST_F(SceneTest, GetCameraDefaultNull) {
    // Before any camera is set
    EXPECT_EQ(scene->getCamera(), nullptr);
}

// ============================================================================
// Lighting Tests
// ============================================================================

TEST_F(SceneTest, SetLightBoxUniquePtr) {
    auto lightBox = std::make_unique<SimpleColorLightBox>(Color::white());
    LightBox* rawPtr = lightBox.get();
    scene->setLightBox(std::move(lightBox));

    EXPECT_EQ(scene->getLightBox(), rawPtr);
}

TEST_F(SceneTest, SetLightBoxRawPointer) {
    SimpleColorLightBox* lightBox = new SimpleColorLightBox(Color::red());
    scene->setLightBox(lightBox);

    EXPECT_EQ(scene->getLightBox(), lightBox);
}

TEST_F(SceneTest, GetLightBoxDefaultNull) {
    EXPECT_EQ(scene->getLightBox(), nullptr);
}

TEST_F(SceneTest, GetEffectiveLightingReturnsDefault) {
    // When no LightBox is set, should return a default
    const LightBox& lighting = scene->getEffectiveLighting();
    // Just verify it doesn't crash and returns something
    EXPECT_GE(lighting.getAmbientIntensity(), 0.0f);
}

TEST_F(SceneTest, GetEffectiveLightingReturnsCustom) {
    auto lightBox = std::make_unique<SimpleColorLightBox>(Color(0.5f, 0.5f, 0.5f));
    lightBox->setAmbientIntensity(0.75f);
    scene->setLightBox(std::move(lightBox));

    const LightBox& lighting = scene->getEffectiveLighting();
    EXPECT_FLOAT_EQ(lighting.getAmbientIntensity(), 0.75f);
}

// ============================================================================
// Background Color Tests
// ============================================================================

TEST_F(SceneTest, SetBackgroundColor) {
    scene->setBackgroundColor(Color::red());

    const Color& bg = scene->getBackgroundColor();
    EXPECT_FLOAT_EQ(bg.r, 1.0f);
    EXPECT_FLOAT_EQ(bg.g, 0.0f);
    EXPECT_FLOAT_EQ(bg.b, 0.0f);
}

// ============================================================================
// World Bounds Tests (Phase 2.5)
// ============================================================================

TEST_F(SceneTest, SetWorldBounds) {
    WorldBounds bounds = WorldBounds::fromDirectionalLimits(100_m, WorldBounds::south(100_m),
                                                            WorldBounds::west(100_m), 100_m, 20_m,
                                                            WorldBounds::down(10_m));

    scene->setWorldBounds(bounds);

    const WorldBounds& result = scene->getWorldBounds();
    EXPECT_FLOAT_EQ(result.northLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(result.southLimit().value, -100.0f);
}

TEST_F(SceneTest, GetWorldBoundsMutable) {
    WorldBounds& bounds = scene->getWorldBounds();
    bounds = WorldBounds::flat(50_m, -50_m, -50_m, 50_m);

    EXPECT_TRUE(scene->is2D());
}

TEST_F(SceneTest, Is2DWithFlatBounds) {
    scene->setWorldBounds(WorldBounds::flat(100_m, -100_m, -100_m, 100_m));
    EXPECT_TRUE(scene->is2D());
}

TEST_F(SceneTest, Is2DWith3DBounds) {
    scene->setWorldBounds(
        WorldBounds::fromDirectionalLimits(100_m, -100_m, -100_m, 100_m, 50_m, -50_m));
    EXPECT_FALSE(scene->is2D());
}

// ============================================================================
// Input Handler Tests
// ============================================================================

class TestInputHandler : public InputHandler {
  public:
    void onKeyPress(int key) override { lastKey = key; }
    int lastKey = 0;
};

TEST_F(SceneTest, SetInputHandler) {
    TestInputHandler handler;
    scene->setInputHandler(&handler);

    EXPECT_EQ(scene->getInputHandler(), &handler);
}

// ============================================================================
// Update Tests
// ============================================================================

class CountingEntity : public MeshEntity {
  public:
    void update(float deltaTime) override {
        updateCount++;
        lastDeltaTime = deltaTime;
    }

    int updateCount = 0;
    float lastDeltaTime = 0.0f;
};

TEST_F(SceneTest, UpdateCallsEntityUpdate) {
    auto entity = scene->addEntity<CountingEntity>();

    scene->update(0.016f);  // ~60 FPS

    EXPECT_EQ(entity->updateCount, 1);
    EXPECT_FLOAT_EQ(entity->lastDeltaTime, 0.016f);
}

TEST_F(SceneTest, UpdateCallsAllEntities) {
    auto entity1 = scene->addEntity<CountingEntity>();
    auto entity2 = scene->addEntity<CountingEntity>();
    auto entity3 = scene->addEntity<CountingEntity>();

    scene->update(0.016f);

    EXPECT_EQ(entity1->updateCount, 1);
    EXPECT_EQ(entity2->updateCount, 1);
    EXPECT_EQ(entity3->updateCount, 1);
}

// ============================================================================
// Background / Priority Tests (Phase 2)
// ============================================================================

TEST_F(SceneTest, ContinueInBackgroundDefaultFalse) {
    EXPECT_FALSE(scene->getContinueInBackground());
}

TEST_F(SceneTest, SetContinueInBackground) {
    scene->setContinueInBackground(true);
    EXPECT_TRUE(scene->getContinueInBackground());

    scene->setContinueInBackground(false);
    EXPECT_FALSE(scene->getContinueInBackground());
}

TEST_F(SceneTest, UpdatePriorityDefaultZero) {
    EXPECT_EQ(scene->getUpdatePriority(), 0);
}

TEST_F(SceneTest, SetUpdatePriority) {
    scene->setUpdatePriority(5);
    EXPECT_EQ(scene->getUpdatePriority(), 5);

    scene->setUpdatePriority(-3);
    EXPECT_EQ(scene->getUpdatePriority(), -3);
}

// ============================================================================
// Viewport Tests (Phase 3)
// ============================================================================

TEST_F(SceneTest, DefaultViewportIsFullWindow) {
    auto vp = scene->getViewportRect();
    EXPECT_FLOAT_EQ(vp.x, 0.0f);
    EXPECT_FLOAT_EQ(vp.y, 0.0f);
    EXPECT_FLOAT_EQ(vp.width, 1.0f);
    EXPECT_FLOAT_EQ(vp.height, 1.0f);
    EXPECT_EQ(vp, ViewportRect::fullWindow());
}

TEST_F(SceneTest, SetViewportRect) {
    auto topRight = ViewportRect::topRight();
    scene->setViewportRect(topRight);

    auto vp = scene->getViewportRect();
    EXPECT_FLOAT_EQ(vp.x, 0.5f);
    EXPECT_FLOAT_EQ(vp.y, 0.0f);
    EXPECT_FLOAT_EQ(vp.width, 0.5f);
    EXPECT_FLOAT_EQ(vp.height, 0.5f);
}

TEST_F(SceneTest, SetViewportRectCustom) {
    ViewportRect custom{0.1f, 0.2f, 0.3f, 0.4f};
    scene->setViewportRect(custom);

    auto vp = scene->getViewportRect();
    EXPECT_FLOAT_EQ(vp.x, 0.1f);
    EXPECT_FLOAT_EQ(vp.y, 0.2f);
    EXPECT_FLOAT_EQ(vp.width, 0.3f);
    EXPECT_FLOAT_EQ(vp.height, 0.4f);
}

TEST_F(SceneTest, ViewportRectCanBeResetToFullWindow) {
    scene->setViewportRect(ViewportRect::bottomLeft());
    scene->setViewportRect(ViewportRect::fullWindow());

    EXPECT_EQ(scene->getViewportRect(), ViewportRect::fullWindow());
}

// ============================================================================
// Phase Callback Tests (Phase 4)
// ============================================================================

TEST_F(SceneTest, PhaseCallbacksDisabledByDefault) {
    EXPECT_FALSE(scene->usesPhaseCallbacks());
}

TEST_F(SceneTest, EnablePhaseCallbacks) {
    scene->enablePhaseCallbacks();
    EXPECT_TRUE(scene->usesPhaseCallbacks());
}

class PhaseTrackingScene : public Scene {
  public:
    void updateGameLogic(float deltaTime) override {
        gameLogicCalled = true;
        gameLogicDt = deltaTime;
        ++callOrder;
        gameLogicOrder = callOrder;
    }
    void updateAudio(float deltaTime) override {
        // Call base to drain queue
        Scene::updateAudio(deltaTime);
        audioCalled = true;
        audioDt = deltaTime;
        ++callOrder;
        audioOrder = callOrder;
    }
    void updateVisuals(float deltaTime) override {
        visualsCalled = true;
        visualsDt = deltaTime;
        ++callOrder;
        visualsOrder = callOrder;
    }

    bool gameLogicCalled = false;
    bool audioCalled = false;
    bool visualsCalled = false;
    float gameLogicDt = 0.0f;
    float audioDt = 0.0f;
    float visualsDt = 0.0f;
    int callOrder = 0;
    int gameLogicOrder = 0;
    int audioOrder = 0;
    int visualsOrder = 0;
};

TEST_F(SceneTest, PhaseCallbacksCanBeCalledDirectly) {
    PhaseTrackingScene trackScene;
    trackScene.enablePhaseCallbacks();

    trackScene.updateGameLogic(0.016f);
    trackScene.updateAudio(0.016f);
    trackScene.updateVisuals(0.016f);

    EXPECT_TRUE(trackScene.gameLogicCalled);
    EXPECT_TRUE(trackScene.audioCalled);
    EXPECT_TRUE(trackScene.visualsCalled);
    EXPECT_FLOAT_EQ(trackScene.gameLogicDt, 0.016f);
    EXPECT_FLOAT_EQ(trackScene.audioDt, 0.016f);
    EXPECT_FLOAT_EQ(trackScene.visualsDt, 0.016f);
}

TEST_F(SceneTest, PhaseCallbackOrder) {
    PhaseTrackingScene trackScene;
    trackScene.enablePhaseCallbacks();

    trackScene.updateGameLogic(0.016f);
    trackScene.updateAudio(0.016f);
    trackScene.updateVisuals(0.016f);

    // gameLogic -> audio -> visuals
    EXPECT_LT(trackScene.gameLogicOrder, trackScene.audioOrder);
    EXPECT_LT(trackScene.audioOrder, trackScene.visualsOrder);
}

TEST_F(SceneTest, DefaultPhaseCallbacksAreNoOps) {
    // A plain Scene's callbacks should not crash
    scene->updateGameLogic(0.016f);
    scene->updateVisuals(0.016f);
    // updateAudio drains an empty queue — also safe
    scene->updateAudio(0.016f);
}

TEST_F(SceneTest, PhaseCallbackUpdateAudioDrainsQueue) {
    scene->playSFX(nullptr);
    scene->playSFX(nullptr);
    EXPECT_EQ(scene->getAudioEventQueueSize(), 2u);

    scene->updateAudio(0.016f);
    EXPECT_EQ(scene->getAudioEventQueueSize(), 0u);
}

// ============================================================================
// Deferred Command Tests
// ============================================================================

TEST_F(SceneTest, DeferCommandBasicExecution) {
    bool executed = false;
    scene->deferCommand([&executed]() { executed = true; });

    EXPECT_FALSE(executed);
    EXPECT_EQ(scene->getDeferredCommandCount(), 1u);

    scene->update(0.016f);
    EXPECT_TRUE(executed);
    EXPECT_EQ(scene->getDeferredCommandCount(), 0u);
}

TEST_F(SceneTest, DeferCommandFIFOOrder) {
    std::vector<int> order;
    scene->deferCommand([&order]() { order.push_back(1); });
    scene->deferCommand([&order]() { order.push_back(2); });
    scene->deferCommand([&order]() { order.push_back(3); });

    scene->update(0.016f);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(SceneTest, DeferCommandMultipleFlushCycles) {
    int counter = 0;
    scene->deferCommand([&counter]() { counter++; });
    scene->update(0.016f);
    EXPECT_EQ(counter, 1);

    // Second update with no pending commands does nothing
    scene->update(0.016f);
    EXPECT_EQ(counter, 1);

    // Queue more and flush again
    scene->deferCommand([&counter]() { counter++; });
    scene->update(0.016f);
    EXPECT_EQ(counter, 2);
}

TEST_F(SceneTest, DeferCommandReentrant) {
    // A deferred command queues another deferred command — must execute next update
    int step = 0;
    scene->deferCommand([this, &step]() {
        step = 1;
        scene->deferCommand([&step]() { step = 2; });
    });

    scene->update(0.016f);
    // First command ran; second was queued during execution
    EXPECT_EQ(step, 1);
    EXPECT_EQ(scene->getDeferredCommandCount(), 1u);

    scene->update(0.016f);
    EXPECT_EQ(step, 2);
    EXPECT_EQ(scene->getDeferredCommandCount(), 0u);
}

TEST_F(SceneTest, DeferCommandEntityAddRemove) {
    scene->deferCommand([this]() {
        auto entity = scene->addEntity<MeshEntity>();
        entity->setName("Deferred");
    });
    EXPECT_EQ(scene->getEntities().size(), 0u);

    scene->update(0.016f);
    EXPECT_EQ(scene->getEntities().size(), 1u);
    EXPECT_NE(scene->getEntityByName("Deferred"), nullptr);
}

TEST_F(SceneTest, RetireResourceKeepsAlive) {
    auto resource = std::make_shared<int>(42);
    std::weak_ptr<int> weak = resource;

    scene->retireResource(std::move(resource));
    // resource moved — but still alive in the retire queue
    EXPECT_FALSE(weak.expired());

    scene->update(0.016f);
    // flushDeferredCommands clears retired resources
    EXPECT_TRUE(weak.expired());
}

TEST_F(SceneTest, DeferCommandCount) {
    EXPECT_EQ(scene->getDeferredCommandCount(), 0u);
    scene->deferCommand([]() {});
    EXPECT_EQ(scene->getDeferredCommandCount(), 1u);
    scene->deferCommand([]() {});
    EXPECT_EQ(scene->getDeferredCommandCount(), 2u);

    scene->update(0.016f);
    EXPECT_EQ(scene->getDeferredCommandCount(), 0u);
}

TEST_F(SceneTest, DeferCommandFlushedByUpdateGameLogic) {
    bool executed = false;
    scene->deferCommand([&executed]() { executed = true; });

    scene->updateGameLogic(0.016f);
    EXPECT_TRUE(executed);
    EXPECT_EQ(scene->getDeferredCommandCount(), 0u);
}
