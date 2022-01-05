#include "glfw_engine.h"
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

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"

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

#include "rendering/renderers/bounding_box_renderer.h"

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

        m_renderer->GetPostProcessing()->AddFilter<SSAOFilter>("ssao", 0);
        // m_renderer->GetPostProcessing()->AddFilter<GammaCorrectionFilter>("gamma correction", 1);

        cam = new FpsCamera(
            inputmgr,
            &m_renderer->GetRenderWindow(),
            m_renderer->GetScaledWindowWidth(),
            m_renderer->GetScaledWindowHeight(),
            75.0f,
            0.1f,
            2600.0f
        );
        //env_cam = new PerspectiveCamera(45, 256, 256, 0.3f, 100.0f);
        //env_cam->SetTranslation(Vector3(0, 10, 0));
        fbo = new Framebuffer2D(cam->GetWidth(), cam->GetHeight());
        //env_fbo = new FramebufferCube(256, 256);
        shadows = new PssmShadowMapping(cam, 4, 200);

    }

    ~MyGame()
    {
        delete shadows;
        delete fbo;
        //delete env_fbo;
        delete cam;
        //delete env_cam;

        AudioManager::Deinitialize();
    }

    void InitParticleSystem()
    {
        ParticleConstructionInfo particle_generator_info(
            // the lambda function for setting a particle's origin
            [](const Particle &particle) {
                return Vector3(0, 20, 0);
            },
                // the lambda function for setting a particle's velocity
            [](const Particle &particle) {
                static int counter = 0;
                counter++;

                float radius = 1.0f;
                const Vector3 rotation(sinf(counter * 0.2f) * radius, 0.0f, cosf(counter * 0.2f) * radius);
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
        particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/smoke.png"));
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(cam));

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

        Environment::GetInstance()->SetShadowsEnabled(true);
        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(1.0).Normalize());

        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, 0), Vector4(1.0f, 0.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(6.0f, 15, 0), Vector4(0.0f, 1.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, 6.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f), 10.0f));
        Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 15, -6.0f), Vector4(1.0f, 0.4f, 0.7f, 1.0f), 10.0f));
        // Initialize root node
        top = std::make_shared<Entity>("top");

        cam->SetTranslation(Vector3(0, 150, 0));

        // Initialize particle system
        // InitParticleSystem();

        ShaderProperties defines = {
            { "SHADOWS", Environment::GetInstance()->ShadowsEnabled() },
            { "NORMAL_MAPPING", 1 },
            { "ROUGHNESS_MAPPING", 1 },
            { "METALNESS_MAPPING", 1 },
            { "NUM_SPLITS", Environment::GetInstance()->NumCascades() }
        };
        shader = ShaderManager::GetInstance()->GetShader<LightingShader>(defines);

        //cubemap_renderer_shader = ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties {});
        // TODO: terrain raycasting
        // Ray top_ray;
        // top_ray.m_direction = Vector3(0, -1, 0);
        // top_ray.m_position = Vector3(0, 15000, 0);

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

        // InitPhysicsTests();

        const auto brdf_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/brdfLUT.png");

        for (int x = 0; x < 5; x++) {
            for (int z = 0; z < 5; z++) {
                Vector3 box_position = Vector3((x * 6) + 6, 160, z * 6);
                auto box = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sphere_hq.obj", true);
                box->GetChild(0)->GetRenderable()->SetShader(shader);

                // box->GetChild(0)->GetMaterial().alpha_blended = true;
                box->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Base_Color.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Height.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Ambient_Occlusion.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/dirtwithrocks-ogl/dirtwithrocks_Normal-ogl.png"));

                // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-albedo.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-normal-ogl.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("MetalnessMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-metalness.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("RoughnessMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/grimy/grimy-metal-roughness.png"));

                // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_albedo.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_height.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_ao.png"));
                // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_normal-ogl.png"));
                box->GetChild(0)->GetMaterial().SetParameter("shininess", float(x) / 5.0f);
                box->GetChild(0)->GetMaterial().SetParameter("roughness", float(z) / 5.0f);
                box->SetLocalTranslation(box_position);
                top->AddChild(box);
            }
        }
        // box->GetChild(0)->GetMaterial().SetTexture("BrdfMap", brdf_map);
        // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_albedo.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_height.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_ao.png"));
        // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_normal-ogl.png"));



        /*{
            auto building = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/building/building.obj", true);
            building->SetLocalTranslation(Vector3(0, 150, 0));
            building->SetLocalScale(Vector3(20));
            for (int i = 0; i < building->NumChildren(); i++) {
                building->GetChild(i)->GetRenderable()->SetShader(shader);
                building->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                building->GetChild(0)->GetMaterial().SetParameter("shininess", 0.5f);
                building->GetChild(0)->GetMaterial().SetParameter("roughness", 0.5f);
            }
            top->AddChild(building);
        }*/

        {
            auto tree = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/blackgum/blackgum.obj", true);
            tree->SetLocalTranslation(Vector3(0, 150, 5));
            tree->SetLocalScale(Vector3(0.8));
            for (int i = 0; i < tree->NumChildren(); i++) {
                tree->GetChild(i)->GetMaterial().SetTexture("BrdfMap", brdf_map);
                // tree->GetChild(i)->GetRenderable()->SetShader(shader);
                // // tree->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                tree->GetChild(0)->GetMaterial().SetParameter("shininess", 0.0f);
                tree->GetChild(0)->GetMaterial().SetParameter("roughness", 0.5f);
            }

            tree->GetChild("Branches_08")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            tree->GetChild("Branches_08")->GetMaterial().alpha_blended = true;
            tree->GetChild("Branches_08")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            tree->GetChild("BlackGumLeaves_1")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            tree->GetChild("BlackGumLeaves_1")->GetMaterial().alpha_blended = true;
            tree->GetChild("BlackGumLeaves_1")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            tree->GetChild("BlackGumLeaves_2")->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
            tree->GetChild("BlackGumLeaves_2")->GetMaterial().alpha_blended = true;
            tree->GetChild("BlackGumLeaves_2")->GetRenderable()->SetRenderBucket(Renderable::RB_TRANSPARENT);
            top->AddChild(tree);
        }


        /*rb3 = std::make_shared<physics::RigidBody>(std::make_shared<physics::BoxPhysicsShape>(Vector3(2.0)), 0.0);
        rb3->SetPosition(box_position);
        // rb3->SetOrientation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(30.0f)));
        rb3->SetAwake(false);
        box->AddControl(rb3);
        PhysicsManager::GetInstance()->RegisterBody(rb3);

        auto bb_renderer = std::make_shared<BoundingBoxRenderer>(&rb3->GetBoundingBox());
        auto bb_renderer_node = std::make_shared<Entity>();
        bb_renderer_node->SetRenderable(bb_renderer);*/

        // box->AddChild(bb_renderer_node);


        { // house
            auto house = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/house.obj");
            for (size_t i = 0; i < house->NumChildren(); i++) {
                //monkey->GetChild(i)->GetRenderable()->GetMaterial().diffuse_color = Vector4(0.0f, 0.9f, 0.2f, 1.0f);
                house->GetChild(i)->GetRenderable()->SetShader(shader);
            }

            house->Move(Vector3(-3, 50, -3));
            house->SetName("house");
            top->AddChild(house);
        }

        std::shared_ptr<Cubemap> cubemap(new Cubemap({
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negx.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negy.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posz.jpg"),
             AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negz.jpg")
         }));
        Environment::GetInstance()->SetGlobalCubemap(cubemap);

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
            for (size_t i = 0; i < sponza->NumChildren(); i++) {
                std::cout << " child [" << i << "] material shininess = " << sponza->GetChild(i)->GetMaterial().GetParameter("shininess")[0] << "\n";
                sponza->GetChild(i)->GetRenderable()->SetShader(shader);
            }

            sponza->Move(Vector3(60, 5, 0));
            sponza->SetName("sponza");
            top->AddChild(sponza);

            //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
            sponza->Scale(2.0f);
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



       /* { // apartment
            auto apartment = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/apartment/flat01.obj");
            // std::function<void(Entity*)> scan_nodes;
            // scan_nodes = [this, &scan_nodes](Entity *root) {

            //     for (size_t i = 0; i < root->NumChildren(); i++) {
            //         root->GetChild(i)->GetRenderable()->SetShader(shader);

            //         scan_nodes(root->GetChild(i).get());
            //     }
            // };
            // scan_nodes(apartment.get());
            for (size_t i = 0; i < apartment->NumChildren(); i++) {
                apartment->GetChild(i)->GetRenderable()->SetShader(shader);
            }

            apartment->Move(Vector3(5, 18, 0));
            apartment->SetName("apartment");
            top->AddChild(apartment);

            //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
            apartment->Scale(4.0f);
        }*/

        // { // sewers
        //     auto sewers = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sewers/sewers/sewer.obj");
        //     // std::function<void(Entity*)> scan_nodes;
        //     // scan_nodes = [this, &scan_nodes](Entity *root) {

        //     //     for (size_t i = 0; i < root->NumChildren(); i++) {
        //     //         root->GetChild(i)->GetRenderable()->SetShader(shader);

        //     //         scan_nodes(root->GetChild(i).get());
        //     //     }
        //     // };
        //     // scan_nodes(sewers.get());
        //     for (size_t i = 0; i < sewers->NumChildren(); i++) {
        //         sewers->GetChild(i)->GetRenderable()->SetShader(shader);
        //     }

        //     sewers->Move(Vector3(5, 18, 0));
        //     sewers->SetName("sewers");
        //     top->AddChild(sewers);

        //     //room->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
        //     sewers->Scale(5.0f);
        // }

        // top->AddControl(std::make_shared<SkydomeControl>(cam));
        top->AddControl(std::make_shared<SkyboxControl>(cam, cubemap));
        top->AddControl(std::make_shared<NoiseTerrainControl>(cam, 3434));
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

        // Environment::GetInstance()->GetSun().SetDirection(Vector3(sin(timer * 0.2), cos(timer * 0.2), 0.0f).Normalize());

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

        top->Update(dt);
    }

    void Render()
    {
        m_renderer->FindRenderables(top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
            shadow_dir.SetY(-1.0f);
            shadows->SetLightDirection(shadow_dir.Normalize());
            shadows->Render(m_renderer);
        }

        //env_fbo->Use();
        //m_renderer->RenderBucket(env_cam, m_renderer->opaque_bucket, cubemap_renderer_shader.get());
        //env_fbo->End();

        /*glBindFramebuffer(GL_FRAMEBUFFER, env_fbo->GetId());
        env_cam->SetDirection(Vector3(1, 0, 0));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(cam, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X, env_fbo->GetId(), 0);
        env_cam->SetDirection(Vector3(-1, 0, 0));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(env_cam, env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X, env_fbo->GetId(), 0);
        env_cam->SetDirection(Vector3(0, 1, 0));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(env_cam, env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y, env_fbo->GetId(), 0);
        env_cam->SetDirection(Vector3(0, -1, 0));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(env_cam, env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, env_fbo->GetId(), 0);
        env_cam->SetDirection(Vector3(0, 0, 1));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(env_cam, env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z, env_fbo->GetId(), 0);
        env_cam->SetDirection(Vector3(0, 0, -1));
        env_cam->UpdateMatrices();
        m_renderer->RenderAll(env_cam, env_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, env_fbo->GetId(), 0);*/

        m_renderer->RenderAll(cam, fbo);
        m_renderer->ClearRenderables();
        m_renderer->RenderPost(cam, fbo);
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
