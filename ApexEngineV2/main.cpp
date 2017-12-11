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
#include "physics/rigid_body.h"
#include "physics/box_physics_shape.h"
#include "physics/sphere_physics_shape.h"
#include "physics/plane_physics_shape.h"

/* Particles */
#include "particles/particle_renderer.h"
#include "particles/particle_emitter_control.h"

#include "rendering/bounding_box_renderer.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

using namespace apex;

class MyGame : public Game {
public:
    Renderer *renderer;
    Camera *cam;
    Framebuffer *fbo;
    PssmShadowMapping *shadows;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Entity> test_object_0, test_object_1, test_object_2;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Texture> tex;
    std::shared_ptr<Mesh> debug_quad;

    std::shared_ptr<physics::RigidBody> rb1, rb2, rb3, rb4;

    double timer;
    double shadow_timer;
    double physics_update_timer;
    bool scene_fbo_rendered;

    MyGame(const RenderWindow &window)
        : Game(window)
    {
        shadow_timer = 0.0f;
        physics_update_timer = 0.0;
        timer = 0.2;
        scene_fbo_rendered = false;

        std::srand(std::time(nullptr));

        ShaderProperties defines;
        debug_quad = MeshFactory::CreateQuad();
        debug_quad->SetShader(ShaderManager::GetInstance()->GetShader<GammaCorrectShader>(defines));

        renderer = new Renderer();
        cam = new FpsCamera(inputmgr, &this->window, 60.0f, 0.3f, 500.0f);
        fbo = new Framebuffer(window.width, window.height);
        shadows = new PssmShadowMapping(cam, 1, 50);
    }

    ~MyGame()
    {
        delete shadows;
        delete fbo;
        delete cam;
        delete renderer;
    }

    void InitParticleSystem()
    {
        ParticleConstructionInfo particle_generator_info(
            // the lambda function for setting a particle's origin
            [](const Particle &particle) {
                return Vector3(0, 2, 0);
            },
                // the lambda function for setting a particle's velocity
            [](const Particle &particle) {
                static int counter = 0;
                counter++;

                float radius = 1.0f;
                const Vector3 rotation(std::sinf(counter * 0.2f) * radius, 0.0f, std::cosf(counter * 0.2f) * radius);
                Vector3 random(MathUtil::Random(-0.3f, 0.3f), 0.0f, MathUtil::Random(-0.3f, 0.3f));
                return rotation + random;
            }
        );

        particle_generator_info.m_gravity = Vector3(0, 5, 0);
        particle_generator_info.m_max_particles = 200;
        particle_generator_info.m_lifespan = 1.0;
        particle_generator_info.m_lifespan_randomness = 1.0;

        auto particle_node = std::make_shared<Entity>();
        particle_node->SetName("particles");
        particle_node->SetRenderable(std::make_shared<ParticleRenderer>(particle_generator_info));
        particle_node->GetMaterial().texture0 =
            AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/smoke.png");
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(cam));

        top->AddChild(particle_node);
    }

    void InitPhysicsTests()
    {
        /*rb1 = std::make_shared<physics::RigidBody>(std::make_shared<physics::SpherePhysicsShape>(0.5), 1.0);
        rb1->SetPosition(Vector3(2, 8, 0));
        //rb1->SetLinearVelocity(Vector3(-1, 10, 0));
        rb1->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(0.5), 1.0));
        test_object_0->AddControl(rb1);

        rb2 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1)), 1.0);
        rb2->SetPosition(Vector3(0, 7, 0));
        // rb2->SetLinearVelocity(Vector3(2, -1, 0.4));
        rb2->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        test_object_1->AddControl(rb2);

        rb3 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1)), 1.0);
        rb3->SetPosition(Vector3(0, 4, 0));
        rb3->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        test_object_2->AddControl(rb3);*/

        rb4 = std::make_shared<physics::RigidBody>(std::make_shared<physics::PlanePhysicsShape>(Vector3(0, 1, 0), 0.0), 0.0);
        rb4->SetAwake(false);

        /*PhysicsManager::GetInstance()->RegisterBody(rb1);
        PhysicsManager::GetInstance()->RegisterBody(rb2);
        PhysicsManager::GetInstance()->RegisterBody(rb3);*/
        PhysicsManager::GetInstance()->RegisterBody(rb4);
    }

    void InitTestObjects()
    {
        for (int x = -2; x < 2; x++) {
            for (int z = -2; z < 2; z++) {

                auto box = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj", true);
                box->GetChild(0)->GetRenderable()->SetShader(shader);
                box->GetChild(0)->GetMaterial().diffuse_color = { 0.0f, 0.0f, 1.0f, 1.0f };
                Mesh *mesh = dynamic_cast<Mesh*>(box->GetChild(0)->GetRenderable().get());

                /*auto bb_renderer = std::make_shared<BoundingBoxRenderer>(&box->GetChild(0)->GetAABB());
                auto bb_renderer_node = std::make_shared<Entity>();
                bb_renderer_node->SetRenderable(bb_renderer);
                box->AddChild(bb_renderer_node);*/
                
                box->SetLocalTranslation(Vector3(x * 3, 12, z * 3));

                auto rb = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(2.0)), 1.0);
                rb->SetPosition(box->GetLocalTranslation());
                rb->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(0.5), 1.0));
                box->AddControl(rb);
                PhysicsManager::GetInstance()->RegisterBody(rb);

                top->AddChild(box);
            }
        }

        /*test_object_0 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sphere.obj");
        test_object_0->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 0.15f, 0.3f, 1.0f, 1.0f };
        test_object_0->GetChild(0)->GetRenderable()->SetShader(shader);
        top->AddChild(test_object_0);

        test_object_1 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/box.obj");
        test_object_1->GetChild(0)->GetRenderable()->SetShader(shader);
        test_object_1->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 1.0f, 0.0f, 0.0f, 1.0f };

        auto audio_ctrl = std::make_shared<AudioControl>(
            AssetManager::GetInstance()->LoadFromFile<AudioSource>("res/sounds/cartoon001.wav"));
        //test_object_1->AddControl(audio_ctrl);
        audio_ctrl->GetSource()->SetLoop(true);
        audio_ctrl->GetSource()->Play();
        top->AddChild(test_object_1);

        test_object_2 = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/round_cube.obj");
        test_object_2->GetChild(0)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>({
            { "DIFFUSE_MAP", true },
            { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
            { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
        }));
        test_object_2->GetChild(0)->GetRenderable()->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        test_object_2->GetChild(0)->GetRenderable()->GetMaterial().diffuse_texture = tex;
        top->AddChild(test_object_2);*/


    }

    void Initialize()
    {
        Environment::GetInstance()->SetShadowsEnabled(true);
        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(0.9, 0.9, 0.9).Normalize());

        // Initialize root node
        top = std::make_shared<Entity>("top");

        // Initialize particle system
        InitParticleSystem();

        ShaderProperties defines = {
            { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
            { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
        };
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

        tex = AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/grass.jpg");

        InitTestObjects();

        InputEvent raytest_event([=]()
            {
                Ray ray;
                ray.m_direction = cam->GetDirection();
                ray.m_position = cam->GetTranslation();

                Vector3 intersection;

                std::vector<Entity*> intersections = { top.get() };

                while (true) {
                    std::vector<Entity*> new_intersections;
                    for (Entity *ent : intersections) {
                        for (size_t i = 0; i < ent->NumChildren(); i++) {
                            BoundingBox aabb = ent->GetChild(i)->GetAABB();
                            if (aabb.IntersectRay(ray, intersection)) {
                                std::cout << "intersection: { name: " << ent->GetChild(i)->GetName() << 
                                             ", point: " << intersection << " }\n";

                                new_intersections.push_back(ent->GetChild(i).get());
                            }
                        }
                    }

                    intersections = new_intersections;

                    if (intersections.empty()) {
                        break;
                    }
                }

                std::cout << std::endl;

            }, true);

        inputmgr->RegisterKeyEvent(KeyboardKey::KEY_6, raytest_event);

        auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.xml");
        dragger->Move(Vector3(3, -1.8f, 3));
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

        InitPhysicsTests();

        auto house = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/house.obj");
        for (size_t i = 0; i < house->NumChildren(); i++) {
            //monkey->GetChild(i)->GetRenderable()->GetMaterial().diffuse_color = Vector4(0.0f, 0.9f, 0.2f, 1.0f);
            house->GetChild(i)->GetRenderable()->SetShader(shader);
        }
        
        house->Move(Vector3(-3, 0, -3));
        house->SetName("house");
        top->AddChild(house);

        /*auto quad_node = std::make_shared<Entity>("quad");
        auto quad_mesh = MeshFactory::CreateQuad();
        quad_mesh->SetShader(shader);
        quad_node->SetRenderable(quad_mesh);
        quad_node->Scale(15);
        quad_node->Move(Vector3::UnitY() * -2);
        quad_node->Rotate(Quaternion(Vector3::UnitX(), MathUtil::PI / 2));
        top->AddChild(quad_node);*/

        top->AddControl(std::make_shared<SkydomeControl>(cam));
        top->AddControl(std::make_shared<NoiseTerrainControl>(cam, 1234));
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

        Environment::GetInstance()->GetSun().SetDirection(Vector3(sin(timer * 0.05), cos(timer * 0.05), 0.0f).Normalize());

        Vector4 sun_color = Vector4(1.0, 0.95, 0.9, 1.0);
        const float sun_to_horizon = std::max(Environment::GetInstance()->GetSun().GetDirection().y, 0.0f);
        
        sun_color.Lerp(Vector4(0.9, 0.8, 0.7, 1.0), 1.0 - sun_to_horizon);
        const float sun_to_end = std::min(std::max(-Environment::GetInstance()->GetSun().GetDirection().y * 5.0f, 0.0f), 1.0f);
        sun_color.Lerp(Vector4(0.2, 0.2, 0.2, 1.0), sun_to_end);


        Environment::GetInstance()->GetSun().SetColor(sun_color);

        cam->Update(dt);

        const double theta = 0.02;
        if (physics_update_timer >= theta) {
            PhysicsManager::GetInstance()->RunPhysics(theta);
            physics_update_timer = 0.0;
        }
        physics_update_timer += dt;

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

    auto *game = new MyGame(RenderWindow(1480, 1200, "Apex Engine 5.0"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}