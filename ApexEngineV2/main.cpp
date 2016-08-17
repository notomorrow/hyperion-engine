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
#include "rendering/environment.h"
#include "rendering/camera/ortho_camera.h"
#include "rendering/camera/perspective_camera.h"
#include "rendering/camera/fps_camera.h"
#include "rendering/texture.h"
#include "rendering/framebuffer.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/shadow_mapping.h"
#include "util/shader_preprocessor.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
#include "animation/bone.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

/* Extra */
#include "rendering/skydome/skydome.h"

using namespace apex;

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    ShadowMapping *shadows;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;

    float timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        timer = 10;
        scene_fbo_rendered = false;

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 65, 0.5, 150);
        fbo = new Framebuffer(512, 512);
        shadows = new ShadowMapping();
    }

    ~MyGame()
    {
        delete shadows;
        delete fbo;
        delete cam;
        delete renderer;
    }

    void Initialize()
    {
        Environment::GetInstance()->GetSun().SetDirection(Vector3(-0.3, -0.4, 1).Normalize());

        Environment::GetInstance()->SetShadowsEnabled(false);
        Environment::GetInstance()->SetShadowMap(0, shadows->GetShadowMap());

        AudioManager::GetInstance()->Initialize();

        std::string vs_shader_path("res/shaders/default.vert");
        std::string fs_shader_path("res/shaders/default.frag");

        ShaderProperties defines;
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

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

        auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.xml");
        dragger->Move(Vector3(3, 0, 3));
        dragger->Scale(0.5);
        dragger->GetControl<SkeletonControl>(0)->SetLoop(true);
        dragger->GetControl<SkeletonControl>(0)->PlayAnimation(1, 3.0);
        top->AddChild(dragger);


        /*auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(shader);
        cube->Scale(0.5);
        cube->SetName("cube");
        dragger->GetControl<SkeletonControl>(0)->GetBone("head")->AddChild(cube);
        */

        pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/noise.png");

        auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(shader);
        cube->Scale(0.5);
        cube->SetName("cube");
        top->AddChild(cube);

        top->AddControl(std::make_shared<SkydomeControl>(cam));
    }

    void Logic(double dt)
    {
        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;

        Environment::GetInstance()->GetSun().SetDirection(Vector3(sin(timer*0.05), cos(timer*0.05), 0).Normalize());

        cam->Update(dt);

        if (Environment::GetInstance()->ShadowsEnabled()) {
            shadows->GetShadowCamera()->Update(dt);
            Environment::GetInstance()->SetShadowMatrix(0,
                shadows->GetShadowCamera()->GetViewProjectionMatrix());
        }

        top->Update(dt);

        top->GetChild(0)->SetLocalRotation(Quaternion(Vector3(1, 0, 0), timer));
        top->GetChild(0)->SetLocalTranslation(Vector3(0, 0, (std::sin(timer) + 1.0) * 5));
    }

    void Render()
    {
       /* if (!scene_fbo_rendered) {
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
        glClearColor(1, 0, 0, 1);*/

        renderer->FindRenderables(top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            shadows->Begin();
            glClear(GL_DEPTH_BUFFER_BIT);
            renderer->RenderAll(shadows->GetShadowCamera());
            shadows->End();
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        shader->SetUniform("u_diffuseTexture", 0);
        pbr_tex->Use();
       // shadows->GetShadowMap()->Use();
        renderer->RenderAll(cam);
       // shadows->GetShadowMap()->End();
        pbr_tex->End();

        renderer->ClearRenderables();
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