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
#include "rendering/shadow/pssm_shadow_mapping.h"
#include "util/shader_preprocessor.h"
#include "util/mesh_factory.h"
#include "util/aabb_factory.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
#include "animation/bone.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

/* Extra */
#include "rendering/skydome/skydome.h"
#include "terrain/noise_terrain/noise_terrain_control.h"


/* Physics */
#include "physics/physics_manager.h"
#include "physics/physics2/rigid_body.h"
#include "physics/physics2/box_physics_shape.h"
#include "physics/physics2/sphere_physics_shape.h"
#include "physics/physics2/plane_physics_shape.h"
#include "physics/rigid_body_control.h"

/* Particles */
#include "particles/particle_generator.h"

using namespace apex;

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    PssmShadowMapping *shadows;
    ParticleGenerator *particle_generator;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Entity> test_object, test_object_1, torus3;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> pbr_tex;
    std::shared_ptr<Mesh> debug_quad;

    std::shared_ptr<physics::Rigidbody> rb1, rb2, rb3, rb4;

    double timer;
    double shadow_timer;
    double physics_update_timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        shadow_timer = 0.0f;
        physics_update_timer = 0.0;
        timer = 0.15;
        scene_fbo_rendered = false;

        ShaderProperties defines;
        debug_quad = MeshFactory::CreateQuad();
        debug_quad->GetMaterial().alpha_blended = true;
        debug_quad->SetShader(ShaderManager::GetInstance()->GetShader<GammaCorrectShader>(defines));

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 70.0f, 0.3f, 250.0f);
        fbo = new Framebuffer(window.width, window.height);
        shadows = new PssmShadowMapping(cam, 1, 20);

        particle_generator = new ParticleGenerator(ParticleConstructionInfo(
            [](const Particle &particle)
        {
            const Vector3 base(0, 8, 0);
            const Vector3 random(MathUtil::Random(-15.0f, 15.0f), MathUtil::Random(-15.0f, 15.0f), MathUtil::Random(-15.0f, 15.0f));
            return base;// +random;
        },
            [](const Particle &particle)
        {
            static int counter = 0;
            counter++;
            float radius = 3.0f;
            const Vector3 gravity = Environment::GetInstance()->GetGravity() * 0.2f;
            const Vector3 rotation(std::sinf(counter * 0.2f) * radius, 0.0f, std::cosf(counter * 0.2f) * radius);
            const float random = MathUtil::Random(0.3f, 1.3f);
            return (gravity * random) + rotation;
        }
        ));
    }

    ~MyGame()
    {
        delete particle_generator;
        delete shadows;
        delete fbo;
        delete cam;
        delete renderer;
    }

    void Initialize()
    {
        Environment::GetInstance()->SetShadowsEnabled(true);
        AudioManager::GetInstance()->Initialize();

        ShaderProperties defines = {
            { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
            { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
        };
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

        top = std::make_shared<Entity>("top");

        test_object_1 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/box.obj");
        //torus->Scale(1.5f);
        //torus->Move({ 0, 3, 5 });
        test_object_1->GetChild(0)->GetRenderable()->SetShader(shader);
        test_object_1->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 1.0f, 0.0f, 0.0f, 1.0f };

        auto audio_ctrl = std::make_shared<AudioControl>(
            AssetManager::GetInstance()->LoadFromFile<AudioSource>("res/sounds/cartoon001.wav"));
        //test_object_1->AddControl(audio_ctrl);
        audio_ctrl->GetSource()->SetLoop(true);
        audio_ctrl->GetSource()->Play();
        top->AddChild(test_object_1);

        auto test_object_2 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/box.obj");
        //torus2->Scale(1.5f);
        test_object_2->Move({ 0, 4, 5 });
        test_object_2->GetChild(0)->GetRenderable()->SetShader(shader);
        test_object_2->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.0f, 1.0f, 0.0f, 1.0f };
        top->AddChild(test_object_2);

        torus3 = std::make_shared<Entity>();
        torus3->SetRenderable(test_object_2->GetChild(0)->GetRenderable());
        torus3->Move({ 0, 25, 15 });
        top->AddChild(torus3);



       // PhysicsManager::GetInstance()->AddPhysicsCollider(shape1);
       // PhysicsManager::GetInstance()->AddPhysicsCollider(shape2);

        /*auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.GetX()ml");
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

        //pbr_tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/grass2.jpg");

        test_object = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sphere.obj");
        test_object->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.15f, 0.3f, 1.0f, 1.0f };
        test_object->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
           // {"DIFFUSE_MAP", true},
            {"SHADOWS", Environment::GetInstance()->ShadowsEnabled()},
            {"NUM_SPLITS", Environment::GetInstance()->NumCascades()}
        }));
        //cube->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = pbr_tex;
       // cube->Scale({ 5, 1, 5 });
       // cube->Move({ 0, 0, 6 });
        test_object->SetName("sphere");
        top->AddChild(test_object);

        rb1 = std::make_shared<physics::Rigidbody>(std::make_shared<physics::SpherePhysicsShape>(0.5), 1.0);
        rb1->SetPosition(Vector3(2, 40, 0));
        rb1->SetLinearVelocity(Vector3(-1, 10, 0));
        rb1->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(0.5), 1.0));
        test_object->AddControl(std::make_shared<RigidBodyControl>(rb1));

        rb2 = std::make_shared<physics::Rigidbody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1)), 1.0);
        rb2->SetPosition(Vector3(0, 5, 0));
       // rb2->SetLinearVelocity(Vector3(2, -1, 0.4));
        rb2->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        test_object_1->AddControl(std::make_shared<RigidBodyControl>(rb2));

        rb3 = std::make_shared<physics::Rigidbody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1)), 1.0);
        rb3->SetPosition(Vector3(0, 10, 0));
        rb3->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        torus3->AddControl(std::make_shared<RigidBodyControl>(rb3));

        rb4 = std::make_shared<physics::Rigidbody>(std::make_shared<physics::PlanePhysicsShape>(Vector3(0, 1, 0), 0.0), 0.0);
        rb4->SetAwake(false);

        PhysicsManager::GetInstance()->RegisterBody(rb1);
        PhysicsManager::GetInstance()->RegisterBody(rb2);
        PhysicsManager::GetInstance()->RegisterBody(rb3);
        PhysicsManager::GetInstance()->RegisterBody(rb4);

        /*auto monkey = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/monkeyhq.obj");
        monkey->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = Vector4(0.0f, 0.9f, 0.2f, 1.0f);
        monkey->GetChild(0)->GetRenderable()->SetShader(shader);
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
        top->AddChild(quad_node);*/
        
        top->AddControl(std::make_shared<SkydomeControl>(cam));
        //top->AddControl(std::make_shared<NoiseTerrainControl>(cam, 1332));
    }

    void Logic(double dt)
    {
        // offset root node if camera is out of bounds
        const float bounds = 15.0f;
        Vector3 campos(cam->GetTranslation());

        if (campos.GetX() >= bounds) {
            campos.SetX(0);
            top->SetLocalTranslation(top->GetLocalTranslation() - Vector3(bounds, 0, 0));
            cam->SetTranslation(campos);
        } else if (campos.GetX() <= -bounds) {
            campos.SetX(0);
            top->SetLocalTranslation(top->GetLocalTranslation() + Vector3(bounds, 0, 0));
            cam->SetTranslation(campos);
        }
        if (campos.GetZ() >= bounds) {
            campos.SetZ(0);
            top->SetLocalTranslation(top->GetLocalTranslation() - Vector3(0, 0, bounds));
            cam->SetTranslation(campos);
        } else if (campos.GetZ() <= -bounds) {
            campos.SetZ(0);
            top->SetLocalTranslation(top->GetLocalTranslation() + Vector3(0, 0, bounds));
            cam->SetTranslation(campos);
        }

        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;
        shadow_timer += dt;

        Environment::GetInstance()->GetSun().SetDirection(Vector3(sinf(timer * 0.005f), cosf(timer * 0.005f), 0.0f).Normalize());

        cam->Update(dt);


        const double theta = 0.02;
        if (physics_update_timer >= theta) {
            PhysicsManager::GetInstance()->RunPhysics(theta);
            physics_update_timer = 0.0;
        }
        physics_update_timer += dt;

        particle_generator->UpdateParticles(dt);

        top->Update(dt);
    }

    void Render()
    {
        renderer->FindRenderables(top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            if (shadow_timer >= 0.04) {
                Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
                shadow_dir.SetY(-1.0f);
                shadows->SetLightDirection(shadow_dir.Normalize());
                shadows->Render(renderer);
                shadow_timer = 0.0;
            }

            glDisable(GL_CULL_FACE);
        }

        //fbo->Use();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer->RenderAll(cam);
        renderer->ClearRenderables();

        particle_generator->DrawParticles(cam);

        /*debug_quad->GetMaterial().diffuse_texture = shadows->GetShadowMap();
        */

        /*fbo->End();

        debug_quad->GetMaterial().diffuse_texture = fbo->GetColorTexture();
        debug_quad->GetShader()->ApplyMaterial(debug_quad->GetMaterial());
        debug_quad->GetShader()->Use();
        debug_quad->Render();
        debug_quad->GetShader()->End();*/
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