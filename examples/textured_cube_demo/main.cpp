/**
 * @file main.cpp
 * @brief Textured Cube Demo
 *
 * Demonstrates a rotating cube with different textures on each face.
 * Uses a texture atlas approach where a single texture contains 6 regions
 * arranged in a 3x2 grid, with custom UV mapping for each face.
 *
 * Texture atlas layout (3 columns x 2 rows):
 * +-------+-------+-------+
 * | Face1 | Face2 | Face3 |  <- Top row (Y+, Z+, Y-)
 * +-------+-------+-------+
 | Face4 | Face5 | Face6 |  <- Bottom row (Z-, X+, X-)
 * +-------+-------+-------+
 */

#include <vde/Texture.h>
#include <vde/api/GameAPI.h>

#include <cmath>
#include <iostream>
#include <vector>

#include "../ExampleBase.h"

/**
 * @brief Input handler for the textured cube demo.
 */
class TexturedCubeInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onKeyPress(int key) override {
        // Call base class first for ESC and F keys
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_SPACE) {
            m_spacePressed = true;
        }
        if (key == vde::KEY_R) {
            m_resetRotation = true;
        }
    }

    bool isSpacePressed() {
        bool val = m_spacePressed;
        m_spacePressed = false;
        return val;
    }

    bool isResetRotation() {
        bool val = m_resetRotation;
        m_resetRotation = false;
        return val;
    }

  private:
    bool m_spacePressed = false;
    bool m_resetRotation = false;
};

/**
 * @brief Rotating cube entity with texture atlas mapping.
 */
class TexturedCube : public vde::MeshEntity {
  public:
    TexturedCube() = default;

    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }
    void toggleRotation() { m_rotating = !m_rotating; }
    bool isRotating() const { return m_rotating; }

    void update(float deltaTime) override {
        if (m_rotating) {
            auto rot = getRotation();
            rot.yaw += m_rotationSpeed * deltaTime;
            rot.pitch += m_rotationSpeed * 0.5f * deltaTime;
            if (rot.yaw > 360.0f)
                rot.yaw -= 360.0f;
            if (rot.pitch > 360.0f)
                rot.pitch -= 360.0f;
            setRotation(rot);
        }
    }

  private:
    float m_rotationSpeed = 30.0f;
    bool m_rotating = true;
};

/**
 * @brief Create a cube mesh with custom UV coordinates for texture atlas.
 *
 * The texture atlas is 3x2 (3 columns, 2 rows).
 * Face layout in atlas:
 * - (0,0) Top face    (Y+)
 * - (1,0) Front face  (Z+)
 * - (2,0) Bottom face (Y-)
 * - (0,1) Back face   (Z-)
 * - (1,1) Right face  (X+)
 * - (2,1) Left face   (X-)
 */
vde::ResourcePtr<vde::Mesh> createTexturedCube(float size) {
    auto mesh = std::make_shared<vde::Mesh>();
    float halfSize = size * 0.5f;

    // UV helper: maps a face to its region in the 3x2 texture atlas
    auto uvRegion = [](int col, int row, float u, float v) -> glm::vec2 {
        float regionWidth = 1.0f / 3.0f;
        float regionHeight = 1.0f / 2.0f;
        return glm::vec2(col * regionWidth + u * regionWidth,
                         row * regionHeight + v * regionHeight);
    };

    std::vector<vde::Vertex> vertices = {
        // Front face (Z+) - Atlas position (1, 0) - middle top
        {{-halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 0, 0.0f, 1.0f)},
        {{halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 0, 1.0f, 1.0f)},
        {{halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 0, 1.0f, 0.0f)},
        {{-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 0, 0.0f, 0.0f)},

        // Back face (Z-) - Atlas position (0, 1) - left bottom
        {{halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 1, 0.0f, 1.0f)},
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 1, 1.0f, 1.0f)},
        {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 1, 1.0f, 0.0f)},
        {{halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 1, 0.0f, 0.0f)},

        // Top face (Y+) - Atlas position (0, 0) - left top
        {{-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 0, 0.0f, 1.0f)},
        {{halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 0, 1.0f, 1.0f)},
        {{halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 0, 1.0f, 0.0f)},
        {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(0, 0, 0.0f, 0.0f)},

        // Bottom face (Y-) - Atlas position (2, 0) - right top
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 0, 0.0f, 1.0f)},
        {{halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 0, 1.0f, 1.0f)},
        {{halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 0, 1.0f, 0.0f)},
        {{-halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 0, 0.0f, 0.0f)},

        // Right face (X+) - Atlas position (1, 1) - middle bottom
        {{halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 1, 0.0f, 1.0f)},
        {{halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 1, 1.0f, 1.0f)},
        {{halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 1, 1.0f, 0.0f)},
        {{halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(1, 1, 0.0f, 0.0f)},

        // Left face (X-) - Atlas position (2, 1) - right bottom
        {{-halfSize, -halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 1, 0.0f, 1.0f)},
        {{-halfSize, -halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 1, 1.0f, 1.0f)},
        {{-halfSize, halfSize, halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 1, 1.0f, 0.0f)},
        {{-halfSize, halfSize, -halfSize}, {1.0f, 1.0f, 1.0f}, uvRegion(2, 1, 0.0f, 0.0f)},
    };

    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,   // Front
        4,  5,  6,  6,  7,  4,   // Back
        8,  9,  10, 10, 11, 8,   // Top
        12, 13, 14, 14, 15, 12,  // Bottom
        16, 17, 18, 18, 19, 16,  // Right
        20, 21, 22, 22, 23, 20   // Left
    };

    mesh->setData(vertices, indices);
    return mesh;
}

/**
 * @brief Create a procedural texture atlas with 6 different patterns.
 *
 * Creates a 3x2 texture atlas with distinct visual patterns for each face.
 */
std::shared_ptr<vde::Texture> createTextureAtlas() {
    constexpr int atlasWidth = 512 * 3;   // 3 columns
    constexpr int atlasHeight = 512 * 2;  // 2 rows
    constexpr int regionSize = 512;
    constexpr int channels = 4;

    std::vector<uint8_t> pixels(atlasWidth * atlasHeight * channels);

    // Define 6 different patterns/colors for each face
    struct FacePattern {
        int col, row;
        uint8_t r, g, b;
        enum PatternType { SOLID, CHECKER, STRIPES, DOTS, GRADIENT, GRID } pattern;
    };

    const FacePattern faces[6] = {
        {0, 0, 255, 100, 100, FacePattern::CHECKER},   // Top: Red checker
        {1, 0, 100, 255, 100, FacePattern::STRIPES},   // Front: Green stripes
        {2, 0, 100, 100, 255, FacePattern::DOTS},      // Bottom: Blue dots
        {0, 1, 255, 255, 100, FacePattern::GRADIENT},  // Back: Yellow gradient
        {1, 1, 255, 100, 255, FacePattern::GRID},      // Right: Magenta grid
        {2, 1, 100, 255, 255, FacePattern::SOLID}      // Left: Cyan solid
    };

    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
        const auto& face = faces[faceIdx];
        int startX = face.col * regionSize;
        int startY = face.row * regionSize;

        for (int y = 0; y < regionSize; ++y) {
            for (int x = 0; x < regionSize; ++x) {
                int pixelX = startX + x;
                int pixelY = startY + y;
                int idx = (pixelY * atlasWidth + pixelX) * channels;

                uint8_t r = face.r;
                uint8_t g = face.g;
                uint8_t b = face.b;

                // Apply pattern
                switch (face.pattern) {
                case FacePattern::CHECKER: {
                    bool checker = ((x / 64) + (y / 64)) % 2 == 0;
                    float factor = checker ? 1.0f : 0.5f;
                    r = static_cast<uint8_t>(r * factor);
                    g = static_cast<uint8_t>(g * factor);
                    b = static_cast<uint8_t>(b * factor);
                    break;
                }
                case FacePattern::STRIPES: {
                    float factor = (y / 32) % 2 == 0 ? 1.0f : 0.6f;
                    r = static_cast<uint8_t>(r * factor);
                    g = static_cast<uint8_t>(g * factor);
                    b = static_cast<uint8_t>(b * factor);
                    break;
                }
                case FacePattern::DOTS: {
                    int dotX = x % 64;
                    int dotY = y % 64;
                    int centerX = 32;
                    int centerY = 32;
                    int dist =
                        (dotX - centerX) * (dotX - centerX) + (dotY - centerY) * (dotY - centerY);
                    if (dist < 256) {
                        r = 255;
                        g = 255;
                        b = 255;
                    }
                    break;
                }
                case FacePattern::GRADIENT: {
                    float factor = static_cast<float>(y) / regionSize;
                    r = static_cast<uint8_t>(r * (0.5f + 0.5f * factor));
                    g = static_cast<uint8_t>(g * (0.5f + 0.5f * factor));
                    b = static_cast<uint8_t>(b * (0.5f + 0.5f * factor));
                    break;
                }
                case FacePattern::GRID: {
                    bool gridLine = (x % 64 < 4) || (y % 64 < 4);
                    if (gridLine) {
                        r = 50;
                        g = 50;
                        b = 50;
                    }
                    break;
                }
                case FacePattern::SOLID:
                default:
                    // Keep original colors
                    break;
                }

                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = 255;
            }
        }
    }

    auto texture = std::make_shared<vde::Texture>();
    texture->loadFromData(pixels.data(), atlasWidth, atlasHeight);
    return texture;
}

/**
 * @brief Scene demonstrating textured cube with different textures per face.
 */
class TexturedCubeScene : public vde::examples::BaseExampleScene {
  public:
    TexturedCubeScene() : BaseExampleScene(10.0f) {}

    void onEnter() override {
        // Print standard header
        printExampleHeader();

        // Set up orbit camera
        auto* camera = new vde::OrbitCamera(vde::Position(0, 0, 0), 5.0f, 25.0f, 45.0f);
        setCamera(camera);

        // Basic lighting
        auto* lightBox = new vde::ThreePointLightBox(vde::Color::white(), 1.0f);
        lightBox->setAmbientIntensity(0.4f);
        setLightBox(lightBox);

        // Dark background
        setBackgroundColor(vde::Color::fromHex(0x1a1a2e));

        // Create texture atlas
        auto texture = createTextureAtlas();
        texture->uploadToGPU(getGame()->getVulkanContext());

        // Create textured cube
        m_cube = addEntity<TexturedCube>();
        m_cube->setName("TexturedCube");
        m_cube->setMesh(createTexturedCube(2.0f));
        m_cube->setTexture(texture);
        m_cube->setColor(vde::Color::white());
        m_cube->setRotationSpeed(30.0f);

        std::cout << "\nCube created with texture atlas (3x2 grid)." << std::endl;
        std::cout << "Each face displays a different pattern." << std::endl;
    }

    void update(float deltaTime) override {
        // Call base class first (handles ESC, F, auto-terminate)
        BaseExampleScene::update(deltaTime);

        auto* input = dynamic_cast<TexturedCubeInputHandler*>(getInputHandler());
        if (!input)
            return;

        if (input->isSpacePressed()) {
            m_cube->toggleRotation();
            std::cout << "Rotation " << (m_cube->isRotating() ? "enabled" : "paused") << std::endl;
        }

        if (input->isResetRotation()) {
            m_cube->setRotation(0.0f, 0.0f, 0.0f);
            std::cout << "Rotation reset to origin" << std::endl;
        }
    }

  protected:
    std::string getExampleName() const override { return "Textured Cube with Atlas Mapping"; }

    std::vector<std::string> getFeatures() const override {
        return {"Texture atlas with 6 distinct regions (3x2 grid)",
                "Custom UV mapping for each cube face",
                "Procedurally generated textures with different patterns",
                "Rotating cube with interactive controls"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Rotating cube with different patterns on each face:",
                "  - Red checkered pattern (top)",
                "  - Green horizontal stripes (front)",
                "  - Blue with white dots (bottom)",
                "  - Yellow gradient (back)",
                "  - Magenta with grid lines (right)",
                "  - Cyan solid color (left)"};
    }

    std::vector<std::string> getControls() const override {
        return {"SPACE - Toggle rotation on/off", "R - Reset rotation to default orientation"};
    }

  private:
    std::shared_ptr<TexturedCube> m_cube;
};

/**
 * @brief Game class for textured cube demo.
 */
class TexturedCubeGame
    : public vde::examples::BaseExampleGame<TexturedCubeInputHandler, TexturedCubeScene> {
  public:
    TexturedCubeGame() = default;
};

/**
 * @brief Main entry point.
 */
int main(int argc, char** argv) {
    TexturedCubeGame demo;
    return vde::examples::runExample(demo, "VDE Textured Cube Demo", 1280, 720, argc, argv);
}
