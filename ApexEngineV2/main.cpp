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
#include "rendering/shaders/post_shader.h"
#include "rendering/shaders/post/gamma_correct.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/shadow_mapping.h"
#include "util/shader_preprocessor.h"
#include "util/mesh_factory.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
#include "animation/bone.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

/* Extra */
#include "rendering/skydome/skydome.h"

using namespace apex;

void DrawBoundingBox(const BoundingBox &bb)
{
    glBegin(GL_LINES);
    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMin().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMin().z);

    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMin().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMin().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMax().z);

    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMin().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMin().y, bb.GetMax().z);
    glVertex3f(bb.GetMax().x, bb.GetMax().y, bb.GetMax().z);
    glVertex3f(bb.GetMin().x, bb.GetMax().y, bb.GetMax().z);
    glEnd();
}

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    ShadowMapping *shadows;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;
    std::shared_ptr<Mesh> debug_quad;

    double timer;
    double shadow_timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        shadow_timer = 0.0f;
        timer = 0.15;
        scene_fbo_rendered = false;

        ShaderProperties defines;
        debug_quad = MeshFactory::CreateQuad();
        debug_quad->GetMaterial().alpha_blended = true;
        debug_quad->SetShader(ShaderManager::GetInstance()->GetShader<GammaCorrectShader>(defines));

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 65, 0.5, 150);
        fbo = new Framebuffer(window.width, window.height);
        shadows = new ShadowMapping(cam);
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
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        Environment::GetInstance()->GetSun().SetDirection(Vector3(-0.3f, -0.4f, 1.0f).Normalize());

        Environment::GetInstance()->SetShadowsEnabled(true);
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

        /*auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.xml");
        dragger->Move(Vector3(3, -1.8f, 3));
        dragger->Scale(0.5);
        dragger->GetControl<SkeletonControl>(0)->SetLoop(true);
        dragger->GetControl<SkeletonControl>(0)->PlayAnimation(1, 3.0);
        top->AddChild(dragger);*/

        /*auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(shader);
        cube->Scale(0.5);
        cube->SetName("cube");
        dragger->GetControl<SkeletonControl>(0)->GetBone("head")->AddChild(cube);
        */

        pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/pbr_6.png");

        auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
        cube->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({ {"DIFFUSE_MAP", true} }));
        cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = pbr_tex;
        cube->Scale(0.5);
        cube->SetName("cube");
        top->AddChild(cube);

        auto monkey = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/monkeyhq.obj");
        monkey->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({ { "DIFFUSE_MAP", true } }));
        monkey->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = pbr_tex;
        monkey->Move(Vector3(-3, 0, -3));
        monkey->SetName("monkey");
        top->AddChild(monkey);

        auto quad_node = std::make_shared<Entity>("quad");
        auto quad_mesh = MeshFactory::CreateQuad();
        quad_mesh->SetShader(shader);
        quad_node->SetRenderable(quad_mesh);
        quad_node->Scale(15);
        quad_node->Move(Vector3::UnitY() * -2);
        quad_node->Rotate(Quaternion(Vector3::UnitX(), MathUtil::PI / 2));
        top->AddChild(quad_node);

        top->AddControl(std::make_shared<SkydomeControl>(cam));
    }

    void Logic(double dt)
    {
        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;
        shadow_timer += dt;

        Environment::GetInstance()->GetSun().SetDirection(Vector3(sinf(timer * 0.005), cosf(timer * 0.005), 0.0f).Normalize());

        cam->Update(dt);

        top->Update(dt);

        top->GetChild(0)->SetLocalRotation(Quaternion(Vector3(1, 0, 0), timer));
        top->GetChild(0)->SetLocalTranslation(Vector3(0, 0, (std::sin(timer) + 1.0f) * 5));
    }

    void Render()
    {
        renderer->FindRenderables(top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            if (shadow_timer >= 0.04) {
                Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
                shadow_dir.y = -1.0f;
                shadows->SetLightDirection(shadow_dir.Normalize());
                shadows->Begin();

                Environment::GetInstance()->SetShadowMatrix(0,
                    shadows->GetShadowCamera()->GetViewProjectionMatrix());

                glClear(GL_DEPTH_BUFFER_BIT);
                renderer->RenderBucket(shadows->GetShadowCamera(), renderer->opaque_bucket);
                shadows->End();

                shadow_timer = 0.0;
            }
        }

        glDisable(GL_CULL_FACE);

        fbo->Use();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer->RenderAll(cam);
        renderer->ClearRenderables();

        /*debug_quad->GetMaterial().diffuse_texture = shadows->GetShadowMap();
        */

        fbo->End();

        debug_quad->GetMaterial().diffuse_texture = fbo->GetColorTexture();
        debug_quad->GetShader()->ApplyMaterial(debug_quad->GetMaterial());
        debug_quad->GetShader()->Use();
        debug_quad->Render();
        debug_quad->GetShader()->End();
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

    return 0;
}