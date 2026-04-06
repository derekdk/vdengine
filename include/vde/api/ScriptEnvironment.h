#pragma once

/**
 * @file ScriptEnvironment.h
 * @brief Narrow interface for InputScriptExecutor's engine interactions
 *
 * Decouples script execution from the Game class so that the executor
 * can be tested with a lightweight mock instead of a full engine instance.
 */

#include <string>
#include <utility>

namespace vde {

class InputHandler;
class Scene;
struct SceneGroup;

/**
 * @brief Narrow interface that InputScriptExecutor uses to interact with the engine.
 *
 * Game implements this directly (private inheritance). The executor never
 * receives a Game pointer — only this contract.
 */
class ScriptEnvironment {
  public:
    virtual ~ScriptEnvironment() = default;

    /// Resolve the input handler for the current frame (focused scene -> global).
    virtual InputHandler* resolveInputHandler() = 0;

    /// Capture the current framebuffer to a PNG file.
    virtual bool captureScreenshot(const std::string& outputPath) = 0;

    /// Look up a scene by name.
    virtual Scene* getScene(const std::string& name) = 0;

    /// Get the currently active scene group.
    virtual const SceneGroup& getActiveSceneGroup() const = 0;

    /// Get the swap chain extent (width, height) — for viewport assertions.
    virtual std::pair<uint32_t, uint32_t> getSwapChainExtent() const = 0;

    /// Set the process exit code.
    virtual void setExitCode(int code) = 0;

    /// Request engine shutdown.
    virtual void quit() = 0;
};

}  // namespace vde
