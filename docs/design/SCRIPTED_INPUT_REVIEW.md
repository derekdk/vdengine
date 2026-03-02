# Scripted Input API — Review & Recommendations

## Executive Summary

The SCRIPTED_INPUT_API.md document proposes a scripted input replay and visual regression testing system for VDE. The design is sound in intent — enabling automated smoke tests, AI-driven certification, and reproducible demos. However, the document conflates a design specification with marketing prose, duplicates content extensively, and proposes building several capabilities that are better delegated to external tooling. This review identifies what to keep, what to simplify, and what to cut.

**Nothing from this API is implemented yet.** The document is purely aspirational. The existing codebase has no `setInputScriptFile`, no `processInputScript`, no `InputScriptState`, no pixel extraction, and no `--input-script` CLI parsing. The `runExample()` and `runTool()` helpers do not accept `argc`/`argv`.

> **Note:** A repository memory previously stored that `runExample`/`runTool` accept `argc`/`argv` and support `--input-script`. This is incorrect — that code does not exist.

---

## What Fits Well

### 1. Scripted Keyboard Input Replay

The core idea — parse a text file of timed input commands and replay them through the existing `InputHandler` dispatch — fits VDE perfectly.

- The `Game::run()` loop already has a `processInput()` call where a `processInputScript()` step naturally inserts.
- The `InputHandler` interface already has `onKeyPress(int key)`, `onKeyRelease(int key)` methods.
- GLFW key codes in `KeyCodes.h` match the proposed key name mapping.
- The scene-based focus system (`getFocusedScene()`) provides the routing logic the doc describes.

### 2. Frame Timing and Wait Commands

`<wait for startup>`, `<wait N>`, and `<exit>` are simple, deterministic, and essential. They use `deltaTime` accumulation, which is already tracked.

### 3. `--input-script` CLI Argument and Environment Variable

Multiple input sources (CLI flag, env var, direct API call) with a clear priority order is a reasonable pattern. The `configureInputScriptFromArgs` helper keeps `main()` boilerplate minimal.

### 4. Exit Code Propagation

Using process exit code 0/1 for pass/fail is the correct way to integrate with CI and automation tooling. The existing `ExampleBase.h` already has `m_exitCode` and `getExitCode()` on `BaseExampleGame`.

---

## What Needs Rethinking

### 1. Pixel Extraction and Hash Verification — Scope Creep

The document spends ~60% of its length on `extractpixels`, `hashpixels`, and `comparepixels`. These are fundamentally **visual regression testing tools**, not input scripting. Building GPU-to-CPU pixel readback, SHA-256 hashing, labeled comparison engines, and `.pixels.txt` file output is a large Vulkan infrastructure project that should be evaluated separately.

**Recommendations:**
- **Phase 1: Ship input replay only.** This is the 80/20 — it enables smoke testing ("did it crash?"), scripted demos, and automated feature exercising.
- **Phase 2 (separate): Evaluate pixel verification.** If needed, consider lightweight alternatives:
  - **Screenshot capture to PNG** (via stb_image_write, already a dependency) + external image diff tools (ImageMagick `compare`, perceptual diff tools).
  - **Frame hash at the swapchain level** — a single whole-frame hash after presenting is far simpler than arbitrary-region extraction with labeled comparison logic.
  - **Use existing tools** for visual regression (e.g., `reg-suit`, `BackstopJS` patterns, or even simple `fc` on captured PNGs).

**Rationale:** The goal is *"make sure our development process can be tested in an automated way."* Input replay + exit code + optional screenshot capture achieves this without building a visual regression framework from scratch.

### 2. Document Redundancy

The document repeats the same workflows 5–6 times (baseline capture → verification → regression detection) with slightly different framing:
- Quick Start section
- AI-Driven Testing section
- Advanced Features section
- Best Practices section
- Summary section

Each repeats the same `hashpixels` → `comparepixels` flow. The document should be refactored to describe each concept once with clear cross-references.

### 3. Hash Algorithm Confusion

The document inconsistently refers to both "64-bit hash" and "SHA-256." SHA-256 produces a 256-bit (64-hex-char) hash, while the output examples show 16-hex-char values (64-bit). The verification scripts then have 64-hex-char values. This needs to be resolved:

**Recommendation:** If implementing pixel hashing, use a simple non-cryptographic hash (e.g., `xxHash64` or `FNV-1a 64-bit`). SHA-256 is overkill for non-adversarial visual regression. Use a consistent size throughout the docs.

### 4. Environment Variable Priority

The doc says the discovery order is: explicit API → command-line → env var. But the troubleshooting section says *"VDE_INPUT_SCRIPT environment variable takes precedence."* These contradict each other. The command-line should always win over env vars.

**Recommendation:** Priority order should be: explicit API call > CLI argument > environment variable. This is the standard convention.

### 5. The Tool Batch Processing Model Is Unrealistic

Scripting tool operations via keyboard shortcuts (`O` for Open, `S` for Save) is fragile and tied to UI layout. Tools that need batch operations should expose proper CLI arguments (e.g., `--input file.obj --output file.vde`), not simulate keyboard input.

**Recommendation:** Keep input scripting for tools limited to smoke testing ("it launches and doesn't crash"). Batch tool operations should use a proper command-line interface, not keyboard replay.

### 6. Future Enhancements Are Overscoped

The "Future Enhancements" section proposes conditionals, variables, loops, includes, mouse/gamepad scripting, YAML format, visual editor, and diff visualization. This turns a simple replay script into a full programming language and IDE.

**Recommendation:** Cut all future enhancements from the spec. If they become needed, design them as separate proposals. The strength of this system is its simplicity — `<wait>`, `<press>`, `<exit>`.

---

## Convention Alignment

### Fits VDE Conventions Well

| Convention | Alignment |
|---|---|
| C++20 / RAII | Script state as `unique_ptr<InputScriptState>` member ✓ |
| `m_` prefix members | `m_inputScriptFile`, `m_inputScriptState` ✓ |
| camelCase methods | `setInputScriptFile()`, `processInputScript()` ✓ |
| PascalCase types | `InputScriptState`, `InputScriptCommand` ✓ |
| `include/vde/api/` for public API | New header location ✓ |
| `src/api/` for implementation | New source file location ✓ |
| Doxygen documentation | Required for all public methods ✓ |
| GoogleTest unit tests | Script parser is testable without GPU ✓ |

### Suggestions for Better Alignment

1. **Naming:** Call it `InputScript` (not "Scripted Input") to match VDE's noun-first naming (`InputHandler`, `ResourceManager`, `AudioManager`).

2. **Header location:** The parser/state machine should live in `include/vde/api/InputScript.h` and `src/api/InputScript.cpp`. It's a Game API concept, not a core Vulkan component.

3. **Script file extension:** Use `.vdescript` or `.inputscript` instead of `.txt` to make file association clear and allow tooling to recognize them.

4. **Namespace:** Helper functions should go in `vde::` directly (not `vde::examples::` or `vde::tools::`). Both examples and tools share the same Game class — the helpers should be alongside Game, with `ExampleBase.h` and `ToolBase.h` simply forwarding. Alternatively, keep them in their respective namespaces but with a shared implementation in the engine.

5. **Logging prefix:** Use `[VDE:InputScript]` to match other engine subsystem logging (if any logging convention exists), rather than the bare `[InputScript]`.

---

## API Surface Recommendations

### Minimal Public API on `Game`

```cpp
class Game {
public:
    // Set path to input script file (called before initialize)
    void setInputScriptFile(const std::string& scriptPath);
    const std::string& getInputScriptFile() const;

private:
    void loadInputScript();      // Called during initialize()
    void processInputScript();   // Called each frame in run()

    std::string m_inputScriptFile;
    std::unique_ptr<InputScriptState> m_inputScriptState;
};
```

### Standalone Helper (free function, in InputScript.h)

```cpp
namespace vde {

// Parse --input-script from command-line arguments
std::string getInputScriptArg(int argc, char** argv);

// Convenience: parse and configure game in one call
void configureInputScriptFromArgs(Game& game, int argc, char** argv);

} // namespace vde
```

Use raw `argc`/`argv` instead of `std::span<const char* const>` — simpler, matches C++ convention, and avoids the `makeArgSpan` wrapper.

### Updated ExampleBase.h / ToolBase.h

```cpp
// Add argc/argv to runExample:
template <typename TGame>
int runExample(TGame& game, const std::string& name,
               uint32_t width, uint32_t height,
               int argc = 0, char** argv = nullptr);

// Implementation calls vde::configureInputScriptFromArgs(game, argc, argv)
```

---

## Risk Assessment

| Risk | Severity | Mitigation |
|---|---|---|
| Pixel readback is GPU-dependent and non-trivial | High | Defer to Phase 2; use screenshot-to-file as simpler alternative |
| Hash values may not be deterministic across GPU vendors | High | Defer; focus on input replay + crash detection first |
| Timing-dependent tests are fragile | Medium | Use generous waits; document that frame timing is approximate |
| Script syntax may need extension later | Low | Keep parser modular; command dispatch via function table |
| Env variable interference between test runs | Low | Document clearing; prefer CLI args |

---

## Summary of Recommendations

1. **Implement the input replay core** (`<wait>`, `<press>`, `<keydown>`, `<keyup>`, `<exit>`) as Phase 1.
2. **Defer all pixel extraction/hashing** to a separate Phase 2 evaluation.
3. **Add `--input-script` CLI support** to `Game` + helpers in ExampleBase/ToolBase.
4. **Update all example/tool `main()` functions** to pass `argc`/`argv`.
5. **Add a `<screenshot>` command** that saves the current frame to PNG — cheap to implement (stb_image_write is already available) and useful for manual verification.
6. **Refactor the design doc** to remove redundancy and separate specification from workflow guides.
7. **Don't build batch tooling through keyboard simulation** — tools should use proper CLI for batch operations.
8. **Create unit tests** for the script parser (no GPU needed).
9. **Create simple smoke test scripts** for each example (just `<wait for startup>`, `<wait 3000>`, `<exit>`).
10. **Update `scripts/tmp_run_examples_check.ps1`** to use `--input-script` with smoke test scripts.
