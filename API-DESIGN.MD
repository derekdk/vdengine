API Features


game has access to this libraries API headers

we should have a class called vde::Game in the vde namespace
it should be used to initialize and run the game loop
for example:
```cpp
#include <vde/Game.h>

vde::Game myGame;
vde::GameSettings settings;

int main() {
    settings.windowWidth = 800;
    settings.windowHeight = 600;
    settings.fullscreen = false;
    myGame.initialize(settings);
    myGame.setInputHandler(new MyInputHandler());
    myGame.addScene("MainMenu", new MainMenuScene());
    myGame.addScene("Level1", new Level1Scene());
    myGame.addScene("GameOver", new GameOverScene());
    myGame.setActiveScene("MainMenu");
    myGame.run();
    return 0;
}

```

A basic example that shows a rotatable mesh on the screen
```cpp
#include <vde/Game.h>
#include <vde/Scene.h>
#include <vde/Mesh.h>
#include <vde/InputHandler.h>
class MyInputHandler : public vde::InputHandler {
public:
    void onKeyPress(int key) override {
        if (key == vde::KEY_LEFT) {
            rotationY -= 5.0f;
        } else if (key == vde::KEY_RIGHT) {
            rotationY += 5.0f;
        }
    }
    float rotationY = 0.0f;
};
class MyScene : public vde::Scene {
public:
    MyScene() {
        auto mesh_id = this->AddResource<vde::Mesh>("path/to/mesh.obj");
        auto texture_id = this->AddResource<vde::Texture>("path/to/texture.png");
        meshEnt = this->addEntity<vde::MeshEntity>(mesh_id);
        meshEnt->setTexture(texture_id);
        meshEnt->setPosition(0.0f, 0.0f, 0.0f);
        this->setLightBox(new vde::SimpleColorLightBox(vde::Color(1.0f, 1.0f, 1.0f)));
        this->setCamera(new vde::SimpleCamera(vde::Position(0.0f, 0.0f, 5.0f), vde::Direction(0.0f, 0.0f, -1.0f)));
    }
    void update(float deltaTime) override {
        // Update logic if needed
        auto rotation = static_cast<MyInputHandler*>(getInputHandler())->rotationY;
        meshEnt->setRotation(0.0f, rotation, 0.0f);
        vde::Scene::update(deltaTime);
    }
    void render() override {
        vde::Scene::render();
    }
private:
    vde::Entity::Ref meshEnt;
};

vde::Game myGame;
vde::GameSettings settings;
MyInputHandler inputHandler;
MyScene myScene;
int main() {
    settings.windowWidth = 800;
    settings.windowHeight = 600;
    settings.fullscreen = false;
    myGame.initialize(settings);
    myGame.setInputHandler(&inputHandler);
    myGame.addScene("MyScene", &myScene);
    myGame.setActiveScene("MyScene");
    myScene.setInputHandler(&inputHandler);
    myGame.run();
    return 0;
}