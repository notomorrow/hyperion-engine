#include "glfw_engine.h"
#include "gl_util.h"
#include "game.h"
#include "entity.h"
#include "util.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "rendering/mesh.h"
#include "rendering/shader.h"
#include "rendering/environment.h"
#include "rendering/camera/ortho_camera.h"
#include "rendering/camera/perspective_camera.h"
#include "rendering/camera/fps_camera.h"
#include "rendering/texture.h"
#include "rendering/framebuffer_2d.h"
#include "rendering/framebuffer_cube.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shaders/post_shader.h"
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

#include "rendering/shaders/cubemap_renderer_shader.h"

#include "rendering/probe/probe_renderer.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"

/* Extra */
#include "rendering/skydome/skydome.h"
#include "rendering/skybox/skybox.h"
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

/* Misc. Controls */
#include "controls/bounding_box_control.h"
#include "controls/camera_follow_control.h"
// #include "rendering/renderers/bounding_box_renderer.h"

/* UI */
#include "rendering/ui/ui_object.h"
#include "rendering/ui/ui_button.h"
#include "rendering/ui/ui_text.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <math.h>

using namespace apex;

class MyGame : public Game {
public:
    Camera *cam, *env_cam;
    Framebuffer *fbo, *env_fbo;
    PssmShadowMapping *shadows;

    std::vector<std::shared_ptr<Entity>> m_raytested_entities;

    std::shared_ptr<Entity> top;
    std::shared_ptr<Entity> test_object_0, test_object_1, test_object_2;
    std::shared_ptr<Shader> shader;
    std::shared_ptr<Shader> cubemap_renderer_shader;
    std::shared_ptr<Entity> debug_quad;

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

        

    }

    ~MyGame()
    {
        delete shadows;
        delete fbo;
        // delete env_fbo;
        delete cam;
        // delete env_cam;

        AudioManager::Deinitialize();
    }

    void InitParticleSystem()
    {
        ParticleConstructionInfo particle_generator_info;
        particle_generator_info.m_origin_randomness = Vector3(5.0f, 0.5f, 5.0f);
        particle_generator_info.m_velocity = Vector3(0.5f, -3.0f, 0.5f);
        particle_generator_info.m_velocity_randomness = Vector3(1.3f, 0.0f, 1.3f);
        particle_generator_info.m_scale = Vector3(0.05);
        particle_generator_info.m_gravity = Vector3(0, -9, 0);
        particle_generator_info.m_max_particles = 300;
        particle_generator_info.m_lifespan = 1.0;
        particle_generator_info.m_lifespan_randomness = 1.5;

        auto particle_node = std::make_shared<Entity>();
        particle_node->SetName("Particle node");
        // particle_node->SetRenderable(std::make_shared<ParticleRenderer>(particle_generator_info));
        particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/test_snowflake.png"));
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(cam, particle_generator_info));
        particle_node->SetLocalScale(Vector3(1.5));
        particle_node->AddControl(std::make_shared<BoundingBoxControl>());
        particle_node->SetLocalTranslation(Vector3(0, 165, 0));
        particle_node->AddControl(std::make_shared<CameraFollowControl>(cam, Vector3(0, 2, 0)));

        top->AddChild(particle_node);
    }

    void InitPhysicsTests()
    {

        /*rb4 = std::make_shared<physics::RigidBody>(std::make_shared<physics::PlanePhysicsShape>(Vector3(0, 1, 0), 0.0), 0.0);
        rb4->SetAwake(false);
        PhysicsManager::GetInstance()->RegisterBody(rb4);*/

        /*rb1 = std::make_shared<physics::RigidBody>(std::make_shared<physics::SpherePhysicsShape>(0.5), 1.0);
        rb1->SetPosition(Vector3(2, 8, 0));
        //rb1->SetLinearVelocity(Vector3(-1, 10, 0));
        rb1->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(0.5), 1.0));
        test_object_0->AddControl(rb1);

        rb2 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1)), 1.0);
        rb2->SetPosition(Vector3(0, 7, 0));
        // rb2->SetLinearVelocity(Vector3(2, -1, 0.4));
        rb2->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        test_object_1->AddControl(rb2);*/

        


        const auto brdf_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/brdfLUT.png");

        { // test object
            auto object = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/mitsuba.obj");
            for (size_t i = 0; i < object->NumChildren(); i++) {
                object->GetChild(i)->GetRenderable()->SetShader(shader);

                object->GetChild(i)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                object->GetChild(i)->GetMaterial().SetTexture("BrdfMap", brdf_map);
            }

            auto tex = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/dummy.jpg");

            for (int x = 0; x < 5; x++) {
                for (int z = 0; z < 5; z++) {
                    Vector3 position = Vector3(x * 3, 24.0f, z * 3);
                    auto clone = std::dynamic_pointer_cast<Entity>(object->Clone());
                    clone->SetLocalTranslation(position);
                    clone->SetName("object_" + std::to_string(x) + "_" + std::to_string(z));
                    for (size_t i = 0; i < clone->NumChildren(); i++) {
                        clone->GetChild(i)->GetMaterial().SetTexture("DiffuseMap", tex);
                        clone->GetChild(i)->GetMaterial().SetParameter("roughness", MathUtil::Max(float(x) / 5.0f, 0.001f));
                        clone->GetChild(i)->GetMaterial().SetParameter("shininess", MathUtil::Max(float(z) / 5.0f, 0.001f));
                    }

                    auto rigid_body = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(2.0)), 4.0);
                    rigid_body->SetPosition(position);
                    rigid_body->SetLinearVelocity(Vector3(0, -9, 0));
                    rigid_body->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
                    //clone->AddControl(rigid_body);
                    //PhysicsManager::GetInstance()->RegisterBody(rigid_body);


                    //auto bb_renderer = std::make_shared<BoundingBoxRenderer>(&rigid_body->GetBoundingBox());
                   // auto bb_renderer_node = std::make_shared<Entity>();
                   // bb_renderer_node->SetRenderable(bb_renderer);
                   // clone->AddChild(bb_renderer_node);

//                    if (x == 0 && z == 0) {
//                        auto audio_ctrl = std::make_shared<AudioControl>(
//                        AssetManager::GetInstance()->LoadFromFile<AudioSource>("res/sounds/cartoon001.wav"));
//                        clone->AddControl(audio_ctrl);
//                        audio_ctrl->GetSource()->SetLoop(true);
//                        audio_ctrl->GetSource()->Play();
//                    }

                    top->AddChild(clone);
                }
            }
        }

        //rb4->SetAwake(false);

        /*PhysicsManager::GetInstance()->RegisterBody(rb1);
        PhysicsManager::GetInstance()->RegisterBody(rb2);
        PhysicsManager::GetInstance()->RegisterBody(rb3);*/
    }

    void Initialize()
    {
        ShaderManager::GetInstance()->SetBaseShaderProperties(ShaderProperties()
            .Define("SHADOW_MAP_RADIUS", 0.055f)
        );
        
        cam = new FpsCamera(
            GetInputManager(),
            &m_renderer->GetRenderWindow(),
            m_renderer->GetRenderWindow().GetScaledWidth(),
            m_renderer->GetRenderWindow().GetScaledHeight(),
            55.0f,
            0.5f,
            750.0f
        );

        shadows = new PssmShadowMapping(cam, 4, 100);
        shadows->SetVarianceShadowMapping(true);

        Environment::GetInstance()->SetShadowsEnabled(true);
        Environment::GetInstance()->SetNumCascades(4);
        Environment::GetInstance()->SetProbeEnabled(true);
        Environment::GetInstance()->GetProbeRenderer()->SetRenderShading(true);
        Environment::GetInstance()->GetProbeRenderer()->SetRenderTextures(true);
        Environment::GetInstance()->GetProbeRenderer()->GetProbe()->SetOrigin(Vector3(0, 10, 0));

        m_renderer->GetPostProcessing()->AddFilter<SSAOFilter>("ssao", 20);
        m_renderer->GetPostProcessing()->AddFilter<BloomFilter>("bloom", 40);
        // m_renderer->GetPostProcessing()->AddFilter<DepthOfFieldFilter>("depth of field", 50);
        m_renderer->GetPostProcessing()->AddFilter<GammaCorrectionFilter>("gamma correction", 999);
        m_renderer->GetPostProcessing()->AddFilter<FXAAFilter>("fxaa", 9999);
        m_renderer->SetDeferred(true);

        env_cam = new PerspectiveCamera(45, 256, 256, 0.3f, 100.0f);
        env_cam->SetTranslation(Vector3(0, 10, 0));
        fbo = new Framebuffer2D(cam->GetWidth(), cam->GetHeight());
        env_fbo = new FramebufferCube(256, 256);
        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(1.9f, 0.35f, 1.9f).Normalize());

        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, 0), Vector4(1.0f, 0.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(6.0f, 15, 0), Vector4(0.0f, 1.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, 6.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, -6.0f), Vector4(1.0f, 0.4f, 0.7f, 1.0f), 10.0f));
        // Initialize root node
        top = std::make_shared<Entity>("top");

        cam->SetTranslation(Vector3(4, 0, 0));

        // Initialize particle system
        // InitParticleSystem();

        // UI test
        /*auto test_ui = std::make_shared<ui::UIButton>("my_ui_object");
        test_ui->SetLocalTranslation2D(Vector2(-0.5, 0.0));
        test_ui->SetLocalScale2D(Vector2(260, 80));
        top->AddChild(test_ui);
        GetUIManager()->RegisterUIObject(test_ui);*/

        auto ui_text = std::make_shared<ui::UIText>("text_test", "AEngine 1.2.0\n"
            "Press 1 to toggle shadows\n"
            "Press 2 to toggle deferred rendering");
        ui_text->SetLocalTranslation2D(Vector2(-1.0, 1.0));
        ui_text->SetLocalScale2D(Vector2(30));
        top->AddChild(ui_text);
        GetUIManager()->RegisterUIObject(ui_text);

        auto fps_counter = std::make_shared<ui::UIText>("fps_coutner", "- FPS");
        fps_counter->SetLocalTranslation2D(Vector2(0.8, 1.0));
        fps_counter->SetLocalScale2D(Vector2(30));
        top->AddChild(fps_counter);
        GetUIManager()->RegisterUIObject(fps_counter);

        auto ui_fbo_view = std::make_shared<ui::UIObject>("fbo_preview");
        ui_fbo_view->GetMaterial().SetTexture("ColorMap", Environment::GetInstance()->GetShadowMap(0));
        ui_fbo_view->SetLocalTranslation2D(Vector2(0.8, -0.8));
        ui_fbo_view->SetLocalScale2D(Vector2(256));
        top->AddChild(ui_fbo_view);
        GetUIManager()->RegisterUIObject(ui_fbo_view);

        auto ui_crosshair = std::make_shared<ui::UIObject>("crosshair");
        ui_crosshair->GetMaterial().SetTexture("ColorMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/crosshair.png"));
        ui_crosshair->SetLocalTranslation2D(Vector2(0));
        ui_crosshair->SetLocalScale2D(Vector2(128));
        top->AddChild(ui_crosshair);
        GetUIManager()->RegisterUIObject(ui_crosshair);


        ShaderProperties defines;
        // defines.Define("DEFERRED", m_renderer->IsDeferred());
        // defines.Define("SHADOWS", Environment::GetInstance()->ShadowsEnabled());
        defines.Define("NORMAL_MAPPING", 1);
        defines.Define("ROUGHNESS_MAPPING", 1);
        defines.Define("METALNESS_MAPPING", 1);
        // defines.Define("NUM_SPLITS", Environment::GetInstance()->NumCascades());
    
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

        cubemap_renderer_shader = ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties {});
        // TODO: terrain raycasting
        // Ray top_ray;
        // top_ray.m_direction = Vector3(0, -1, 0);
        // top_ray.m_position = Vector3(0, 15000, 0);


        GetInputManager()->RegisterKeyEvent(KEY_1, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            Environment::GetInstance()->SetShadowsEnabled(!Environment::GetInstance()->ShadowsEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_2, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            m_renderer->SetDeferred(!m_renderer->IsDeferred());
        }));


        InputEvent raytest_event([=](bool pressed)
            {
                if (!pressed) {
                    return;
                }

                // for (auto &it : m_raytested_entities) {
                //     it->RemoveControl(it->GetControl<BoundingBoxControl>(0));
                // }

                m_raytested_entities.clear();

                Ray ray;
                ray.m_direction = cam->GetDirection();
                ray.m_direction.Normalize();
                ray.m_position = cam->GetTranslation();


                using Intersection_t = std::pair<std::shared_ptr<Entity>, RaytestHit>;

                RaytestHit intersection;
                std::vector<Intersection_t> intersections = { { top, intersection } };

                while (true) {
                    std::vector<Intersection_t> new_intersections;

                    for (auto &it : intersections) {
                        for (size_t i = 0; i < it.first->NumChildren(); i++) {
                            const auto &child = it.first->GetChild(i);

                            BoundingBox aabb = child->GetAABB();

                            if (aabb.IntersectRay(ray, intersection)) {
                                new_intersections.push_back({ child, intersection });
                            }
                        }
                    }

                    if (new_intersections.empty()) {
                        break;
                    }

                    intersections = new_intersections;
                }

                if (intersections.empty()) {
                    return;
                }
    
                RaytestHitList_t mesh_intersections;

                for (Intersection_t &it : intersections) {
                    // it.first->AddControl(std::make_shared<BoundingBoxControl>());
                    m_raytested_entities.push_back(it.first);

                    if (auto renderable = it.first->GetRenderable()) {
                        renderable->IntersectRay(ray, it.first->GetGlobalTransform(), mesh_intersections);
                    }
                }

                if (mesh_intersections.empty()) {
                    return;
                }

                std::sort(mesh_intersections.begin(), mesh_intersections.end(), [=](const RaytestHit &a, const RaytestHit &b) {
                    return a.hitpoint.Distance(cam->GetTranslation()) < b.hitpoint.Distance(cam->GetTranslation());
                });

                auto cube = top->GetChild("cube");
                cube->SetGlobalTranslation(mesh_intersections[0].hitpoint);

                Matrix4 look_at;
                MatrixUtil::ToLookAt(look_at, mesh_intersections[0].normal, Vector3::UnitY());
                cube->SetGlobalRotation(Quaternion(look_at));

                std::cout << std::endl;
            });

        GetInputManager()->RegisterClickEvent(MOUSE_BTN_LEFT, raytest_event);

        /*auto box_node = std::make_shared<Entity>("box_node");
        box_node->SetLocalTranslation(Vector3(6, 110, 6));
        box_node->SetRenderable(MeshFactory::CreateCube());
        box_node->SetLocalScale(20);
        auto rb4 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1.0)), physics::PhysicsMaterial(0.0));
        rb4->SetPosition(Vector3(6, 110, 6));
        rb4->SetRenderDebugBoundingBox(true);
        // rb4->SetAwake(false);
        PhysicsManager::GetInstance()->RegisterBody(rb4);
        box_node->AddControl(rb4);
        top->AddChild(box_node);*/


        auto dragger = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/ogrexml/dragger_Body.mesh.xml");
        dragger->SetName("dragger");
        dragger->Move(Vector3(7, -10, 6));
        dragger->Scale(0.25);
        dragger->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 0.7f, 0.6f, 1.0f };
        dragger->GetChild(0)->GetMaterial().SetParameter("shininess", 0.1f);
        dragger->GetChild(0)->GetMaterial().SetParameter("roughness", 0.9f);
        dragger->GetControl<SkeletonControl>(0)->SetLoop(true);
        dragger->GetControl<SkeletonControl>(0)->PlayAnimation(0, 12.0);
        top->AddChild(dragger);
        dragger->UpdateTransform();


        auto superdan = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/superdan/superdan.obj");
        superdan->SetName("superdan");
        superdan->Move(Vector3(-7, 5, -6));
        superdan->Scale(0.25);
        for (size_t i = 0; i < superdan->NumChildren(); i++) {
            superdan->GetChild(i)->GetMaterial().SetParameter("FlipUV", Vector2(0, 1));
        }
        top->AddChild(superdan);
        superdan->UpdateTransform();

        {

            auto street = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/street/street.obj");
            street->SetName("street");
            street->Move(Vector3(0, 0, 0));
            street->Scale(1);
            top->AddChild(street);
            street->UpdateTransform();
        }

        /*{
            auto sundaysbest = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/durdle/sundays_best.obj");
            sundaysbest->SetName("sundaysbest");
            sundaysbest->Move(Vector3(2, 4, 8));
            sundaysbest->Scale(0.3);
            for (size_t i = 0; i < sundaysbest->NumChildren(); i++) {
                sundaysbest->GetChild(i)->GetMaterial().SetParameter("FlipUV", Vector2(0, 1));
            }
            top->AddChild(sundaysbest);
            sundaysbest->UpdateTransform();

            ParticleConstructionInfo particle_generator_info;
            particle_generator_info.m_origin_randomness = Vector3(0.1, 0.0, 0.1);
            particle_generator_info.m_velocity = Vector3(0.5f, 1.0f, -5.9f);
            particle_generator_info.m_velocity_randomness = Vector3(0.1f, 0.25f, 1.0f);
            // particle_generator_info.m_scale = Vector3(0.5);
            particle_generator_info.m_gravity = Vector3(0, -9, 0);
            particle_generator_info.m_max_particles = 150;
            particle_generator_info.m_lifespan = 0.1;
            particle_generator_info.m_lifespan_randomness = 0.25;

            auto particle_node = std::make_shared<Entity>();
            particle_node->SetName("Particle node");
            // particle_node->SetRenderable(std::make_shared<ParticleRenderer>(particle_generator_info));
            particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/particle.png"));
            particle_node->AddControl(std::make_shared<ParticleEmitterControl>(cam, particle_generator_info));
            particle_node->SetLocalScale(Vector3(1.5));
            particle_node->SetLocalTranslation(Vector3(-0.1, 1.9, -0.4));
            sundaysbest->AddChild(particle_node);
        }*/

        // auto rb3 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(0.25)), physics::PhysicsMaterial(0.01));
        // rb3->SetPosition(dragger->GetGlobalTransform().GetTranslation());
        // rb3->SetLinearVelocity(Vector3(0, -9, 0));
        // rb3->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
        // rb3->SetAwake(true);
        // dragger->AddControl(rb3);
        // PhysicsManager::GetInstance()->RegisterBody(rb3);

        /*for (auto bone : dragger->GetControl<SkeletonControl>(0)->GetBones()) {
            if (bone->GetName() != "head") {
                continue;
            }
            // bone->SetLocalRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(30.0f)));
            std::vector<Entity*> parents;
            Entity *object = bone;
            while (object != dragger.get()) {
                object = object->GetParent();
                parents.push_back(object);
            }
            for (int i = parents.size() - 1; i >= 0; i--) {
                parents[i]->UpdateTransform();
            }
            bone->UpdateTransform();
            auto rb3 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(1.0)), physics::PhysicsMaterial(0.5));
            rb3->SetPosition(bone->GetGlobalTranslation());
            rb3->SetOrientation(bone->GetGlobalTransform().GetRotation());
            // std::cout << "head trans : " << bone->GetGlobalTransform().GetTranslation() << "\n";
            // rb3->SetLinearVelocity(Vector3(0, -9, 0));
            rb3->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
            rb3->SetRenderDebugBoundingBox(true);
            // rb3->SetOrientation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(30.0f)));
            rb3->SetAwake(true);
            bone->AddControl(rb3);
            PhysicsManager::GetInstance()->RegisterBody(rb3);
        }*/

        // dragger->GetControl<SkeletonControl>(0)->GetBone("head")->SetOffsetTranslation(Vector3(5.0));


        //dragger->GetControl<SkeletonControl>(0)->GetBone("head")->AddChild(cube);
         

        auto plane_rigid_body = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(6.0)), 0.0);
        // plane_rigid_body->SetAwake(false);
        auto plane_entity = std::make_shared<Entity>("static plane");
        plane_entity->SetRenderable(MeshFactory::CreateCube());
        plane_entity->Scale(Vector3(5.0));
        plane_entity->GetRenderable()->SetShader(shader);
        plane_entity->GetMaterial().diffuse_color = Vector4(1.0, 0.0, 0.0, 1.0);
        plane_entity->GetMaterial().SetParameter("shininess", 0.6f);
        plane_entity->GetMaterial().SetParameter("roughness", 0.0f);
        plane_entity->AddControl(plane_rigid_body);
        top->AddChild(plane_entity);

        for (int x = 0; x < 5; x++) {
            for (int z = 0; z < 5; z++) {
                Vector3 box_position = Vector3(((float(x) - 2.5) * 6), 4, (float(z) - 2.5) * 6);
                auto box = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sphere_hq.obj", true);
                box->Scale(0.25);
                for (size_t i = 0; i < box->NumChildren(); i++) {
                    box->GetChild(i)->GetRenderable()->SetShader(shader);

                    // box->GetChild(0)->GetMaterial().alpha_blended = true;
                    box->GetChild(i)->GetMaterial().diffuse_color = Vector4(
                        1.0f,//MathUtil::Random(0.0f, 1.0f),
                        1.0f,//MathUtil::Random(0.0f, 1.0f),
                        1.0f,//MathUtil::Random(0.0f, 1.0f),
                        1.0f
                    );
                    // box->GetChild(i)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Base_Color.png"));
                    // box->GetChild(i)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Height.png"));
                    // box->GetChild(i)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Ambient_Occlusion.png"));
                    // box->GetChild(i)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Normal-ogl.png"));

                    // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-albedo.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-normal-ogl.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("MetalnessMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-metalness.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("RoughnessMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-roughness.png"));

                    // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_albedo.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_height.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_ao.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_normal-ogl.png"));
                    // box->GetChild(i)->GetMaterial().SetParameter("shininess", float(x) / 5.0f);
                    // box->GetChild(i)->GetMaterial().SetParameter("roughness", float(z) / 5.0f);
                    box->GetChild(i)->GetMaterial().SetParameter("shininess", 0.8f);
                    box->GetChild(i)->GetMaterial().SetParameter("roughness", 0.0f);
                }
                box->SetLocalTranslation(box_position);
                top->AddChild(box);
                box->UpdateTransform();

                auto rigid_body = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(0.25)), 0.1);
                rigid_body->SetPosition(box_position);
                // rigid_body->SetLinearVelocity(Vector3(0, -5, 0));
                rigid_body->SetInertiaTensor(MatrixUtil::CreateInertiaTensor(Vector3(1.0) / 2, 1.0));
                rigid_body->SetAwake(true);
                // box->AddControl(rigid_body);
                // box->AddControl(std::make_shared<BoundingBoxControl>());
            }
        }
        // box->GetChild(0)->GetMaterial().SetTexture("BrdfMap", brdf_map);
        // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_albedo.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_height.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_ao.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_normal-ogl.png"));



        /*{
            auto building = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/building2/building2.obj", true);
            building->SetLocalTranslation(Vector3(0, 0, 15));
            building->SetLocalScale(Vector3(1));
            for (int i = 0; i < building->NumChildren(); i++) {
                building->GetChild(i)->GetRenderable()->SetShader(shader);
                // building->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                building->GetChild(0)->GetMaterial().SetParameter("shininess", 0.5f);
                building->GetChild(0)->GetMaterial().SetParameter("roughness", 0.5f);
            }
            top->AddChild(building);
        }*/

        {
            auto cube = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
            cube->GetChild(0)->GetRenderable()->SetShader(shader);
            cube->GetChild(0)->GetMaterial().diffuse_color = { 0.0, 0.0, 1.0, 1.0 };
            cube->Scale(0.1);
            cube->SetName("cube");
            top->AddChild(cube);
        }

        /*{
            auto tree = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/pine/LoblollyPine.obj", true);
            tree->SetLocalTranslation(Vector3(0, 0, 0));
            tree->SetLocalScale(Vector3(1.0));
            for (int i = 0; i < tree->NumChildren(); i++) {
                tree->GetChild(i)->GetMaterial().SetParameter("shininess", 0.0f);
                tree->GetChild(i)->GetMaterial().SetParameter("roughness", 0.9f);
                // dynamic_cast<Mesh*>(tree->GetChild(i)->GetRenderable().get())->CalculateNormals();
            }

            // tree->GetChild("Branches_08")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            // tree->GetChild("Branches_08")->GetMaterial().alpha_blended = true;
            // tree->GetChild("Branches_08")->GetMaterial().SetParameter("RimShading", 0.0f);
            // tree->GetChild("Branches_08")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().alpha_blended = true;
            tree->GetChild("LoblollyPineNeedles_1")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            tree->GetChild("LoblollyPineNeedles_1")->GetMaterial().SetParameter("RimShading", 0.0f);
            // tree->GetChild("BlackGumLeaves_2")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            // tree->GetChild("BlackGumLeaves_2")->GetMaterial().alpha_blended = true;
            // tree->GetChild("BlackGumLeaves_2")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            // tree->GetChild("BlackGumLeaves_2")->GetMaterial().SetParameter("RimShading", 0.0f);
            
            // tree->AddControl(std::make_shared<BoundingBoxControl>());
            
            top->AddChild(tree);



            // auto bb_renderer = std::make_shared<BoundingBoxRenderer>(&tree->GetAABB());
            // auto bb_renderer_node = std::make_shared<Entity>();
            // bb_renderer_node->SetRenderable(bb_renderer);
            // tree->AddChild(bb_renderer_node);
        }*/

        /*{
            auto tree = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/tree02/Tree.obj", true);
            tree->SetLocalTranslation(Vector3(0, 0, 5));
            tree->SetLocalScale(Vector3(5));
            for (size_t i = 0; i < tree->NumChildren(); i++) {
                tree->GetChild(i)->GetMaterial().SetParameter("shininess", 0.1f);
                tree->GetChild(i)->GetMaterial().SetParameter("roughness", 0.9f);
            }
            top->AddChild(tree);
        }*/


        /*rb3 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(2.0)), 0.0);
        rb3->SetPosition(box_position);
        // rb3->SetOrientation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(30.0f)));
        rb3->SetAwake(false);
        box->AddControl(rb3);
        PhysicsManager::GetInstance()->RegisterBody(rb3);



        /*{ // house
            auto house = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/house.obj");
            for (size_t i = 0; i < house->NumChildren(); i++) {
                //monkey->GetChild(i)->GetRenderable()->GetMaterial().diffuse_color = Vector4(0.0f, 0.9f, 0.2f, 1.0f);
                house->GetChild(i)->GetRenderable()->SetShader(shader);
            }

            house->Move(Vector3(-3, 0, -3));
            house->SetName("house");
            top->AddChild(house);
        }*/

        std::shared_ptr<Cubemap> cubemap(new Cubemap({
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posz.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negz.jpg")
         }));
        // Environment::GetInstance()->SetGlobalCubemap(cubemap);

        Environment::GetInstance()->SetGlobalIrradianceCubemap(std::shared_ptr<Cubemap>(new Cubemap({
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posz.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negz.jpg")
        })));

        /*{ // sponza
            auto sponza = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sponza/sponza_dualcolor.obj");
            // std::function<void(Entity*)> scan_nodes;
            // scan_nodes = [this, &scan_nodes](Entity *root) {

            //     for (size_t i = 0; i < root->NumChildren(); i++) {
            //         root->GetChild(i)->GetRenderable()->SetShader(shader);

            //         scan_nodes(root->GetChild(i).get());
            //     }
            // };
            // scan_nodes(sponza.get());
            // for (size_t i = 0; i < sponza->NumChildren(); i++) {
            //     std::cout << " child [" << i << "] material shininess = " << sponza->GetChild(i)->GetMaterial().GetParameter("shininess")[0] << "\n";
            //     sponza->GetChild(i)->GetRenderable()->SetShader(shader);
            // }

            sponza->Move(Vector3(, 5, 0));
            sponza->SetName("sponza");
            top->AddChild(sponza);

            //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
            sponza->Scale(2.0f);
        }*/

        /*{
            auto breakfast_room = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/breakfast_room/breakfast_room.obj");
           

            breakfast_room->Move(Vector3(0, 0, 0));
            breakfast_room->SetName("breakfast_room");
            breakfast_room->Scale(Vector3(0.25));
            top->AddChild(breakfast_room);

            //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
            breakfast_room->Scale(5.0f);
        }*/

        // { //
        //     auto obj = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/mitsuba.obj");
        //     std::function<void(Entity*)> scan_nodes;
        //     scan_nodes = [this, &scan_nodes](Entity *root) {

        //         for (size_t i = 0; i < root->NumChildren(); i++) {
        //             root->GetChild(i)->GetRenderable()->SetShader(shader);

        //             scan_nodes(root->GetChild(i).get());
        //         }
        //     };
        //     scan_nodes(obj.get());

        //     obj->Move(Vector3(0, 0, 0));
        //     obj->SetName("obj");
        //     top->AddChild(obj);

        //     //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
        //     obj->Scale(2.0f);
        // }

        // top->AddControl(std::make_shared<SkydomeControl>(cam));
        top->AddControl(std::make_shared<SkyboxControl>(cam, cubemap));
        // top->AddControl(std::make_shared<NoiseTerrainControl>(cam, 223));
    }

    void Logic(double dt)
    {
        // offset root node if camera is out of bounds
        /*const float bounds = 15.0f;
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
        }*/

        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        timer += dt;
        shadow_timer += dt;

        // top->GetChild("dragger")->GetControl<SkeletonControl>(0)->GetBone("head")->SetLocalRotation(Quaternion(
        //     Vector3(sin(timer * 1.2), cos(timer * 1.2), -sin(timer * 1.2)).Normalize()
        // ));
        // top->GetChild("dragger")->GetControl<SkeletonControl>(0)->GetBone("spine")->SetLocalRotation(Quaternion(
        //     Vector3(sin(timer * 1.2) * 0.2, 0, cos(timer * 1.2) * 0.2).Normalize()
        // ));
        // top->GetChild("sundaysbest")->SetLocalRotation(Quaternion(
        //     Vector3(sin(timer * 1.2) * 0.2, 0, cos(timer * 1.2) * 0.2).Normalize()
        // ));
        // top->GetChild("dragger")->GetControl<SkeletonControl>(0)->GetBone("thigh.L")->SetLocalRotation(Quaternion(
        //     Vector3(sin(timer * 1.2) * 0.2, 0, -sin(timer * 1.2) * 0.2).Normalize()
        // ));

        // Environment::GetInstance()->GetSun().SetDirection(Vector3(sin(timer * 0.2), 1, -cos(timer * 0.2)).Normalize());

        Vector4 sun_color = Vector4(1.0, 0.95, 0.9, 1.0);
        const float sun_to_horizon = std::max(Environment::GetInstance()->GetSun().GetDirection().y, 0.0f);

        sun_color.Lerp(Vector4(0.9, 0.8, 0.7, 1.0), 1.0 - sun_to_horizon);
        const float sun_to_end = std::min(std::max(-Environment::GetInstance()->GetSun().GetDirection().y * 5.0f, 0.0f), 1.0f);
        sun_color.Lerp(Vector4(0.2, 0.2, 0.2, 1.0), sun_to_end);


        // Environment::GetInstance()->GetSun().SetColor(sun_color);

        cam->Update(dt);

        //const double theta = 0.01;
        //if (physics_update_timer >= theta) {
            PhysicsManager::GetInstance()->RunPhysics(dt);
       //     physics_update_timer = 0.0;
       // }
       // physics_update_timer += dt;

        // if (auto fps_counter = std::static_pointer_cast<ui::UIText>(top->GetChild("fps_counter"))) {
        //     fps_counter->SetText(std::to_string(CoreEngine::GetInstance()->stats.fps) + " FPS");
        // }

        top->Update(dt);
    }

    void Render()
    {
        m_renderer->Begin(cam, top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
            shadow_dir.SetY(-1.0f);
            shadows->SetOrigin(cam->GetTranslation());
            shadows->SetLightDirection(shadow_dir.Normalize());
            shadows->Render(m_renderer);
        }

        Environment::GetInstance()->GetProbeRenderer()->Render(m_renderer, cam);

        if (!Environment::GetInstance()->GetGlobalCubemap()) {
            Environment::GetInstance()->SetGlobalCubemap(
                Environment::GetInstance()->GetProbeRenderer()->GetColorTexture()
            );
        }

        m_renderer->Render(cam);
        m_renderer->End(cam, top.get());

        top->ClearPendingRemoval();
    }
};

int main()
{
    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new MyGame(RenderWindow(1480, 1200, "AEngine Demo"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}
