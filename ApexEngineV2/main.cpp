#include <iostream>
#include <string>

#include "glfw_engine.h"
#include "game.h"
#include "entity.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "rendering/renderer.h"
#include "rendering/mesh.h"
#include "rendering/shader.h"
#include "rendering/camera/ortho_camera.h"
#include "rendering/camera/perspective_camera.h"
#include "rendering/camera/fps_camera.h"
#include "rendering/texture.h"
#include "rendering/framebuffer.h"
#include "util/shader_preprocessor.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
using namespace apex;

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    std::shared_ptr<Entity> top;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;

    float timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        timer = 0;
        scene_fbo_rendered = false;

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 65, 0.5, 150);
        fbo = new Framebuffer(512, 512);
    }

    ~MyGame()
    {
        delete fbo;
        delete cam;
        delete renderer;
    }

    void Initialize()
    {
        AudioManager::GetInstance()->Initialize();

        std::string vs_shader_path("res/shaders/default.vert");
        std::string fs_shader_path("res/shaders/default.frag");

        std::map<std::string, float> defines;
        shader = std::make_shared<Shader>(
            ShaderPreprocessor::ProcessShader(
                AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_shader_path)->GetText(),
                defines,
                vs_shader_path),
            ShaderPreprocessor::ProcessShader(
                AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_shader_path)->GetText(),
                defines,
                fs_shader_path)
            );

        top = std::make_shared<Entity>("top");

        auto torus = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/torus.obj");
        torus->GetChild(0)->GetRenderable()->SetShader(shader);
        torus->GetChild(0)->GetRenderable()->GetMaterial().SetParameter("lighting", true);

        auto audio_ctrl = std::make_shared<AudioControl>(
            AssetManager::GetInstance()->LoadFromFile<AudioSource>("res/sounds/cartoon001.wav"));
        torus->AddControl(audio_ctrl);
        audio_ctrl->GetSource()->SetLoop(true);
        audio_ctrl->GetSource()->Play();
        top->AddChild(torus);

        auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(shader);
        cube->Move(Vector3(-3, 1, -3));
        cube->Scale(0.35f);
        cube->SetName("cube");
        top->AddChild(cube);

        pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/pbr_6.png");
    }

    void Logic(double dt)
    {
        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;
        cam->Update(dt);
        top->Update(dt);

        top->GetChild("cube")->Rotate(Quaternion(Vector3(1), dt));

        top->GetChild(0)->SetLocalRotation(Quaternion(Vector3(0, 1, 0), timer));
        top->GetChild(0)->SetLocalTranslation(Vector3(0, 0, (std::sin(timer) + 1.0) * 5));
    }

    void Render()
    {
        if (!scene_fbo_rendered) {
            fbo->Use();
            glClearColor(0, 1, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            glActiveTexture(GL_TEXTURE0);
            shader->SetUniform("u_diffuseTexture", 0);
            pbr_tex->Use();
            renderer->FindRenderables(top.get());
            renderer->RenderAll(cam);
            renderer->ClearRenderables();
            pbr_tex->End();

            fbo->End();

            scene_fbo_rendered = true;
        }
        glClearColor(1, 0, 0, 1);

        glActiveTexture(GL_TEXTURE0);
        shader->SetUniform("u_diffuseTexture", 0);
        fbo->GetColorTexture()->Use();
        
        renderer->FindRenderables(top.get());
        renderer->RenderAll(cam);
        renderer->ClearRenderables();
        fbo->GetColorTexture()->End();
    }
};

int main()
{
    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new MyGame(RenderWindow(1080, 720, "Apex Engine 5.0"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    system("pause");
    return 0;
}