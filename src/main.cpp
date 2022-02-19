#include "glfw_engine.h"
#include "game.h"
#include "scene/node.h"
#include "util.h"
#include "asset/asset_manager.h"
#include "rendering/shader.h"
#include "rendering/environment.h"
#include "rendering/texture.h"
#include "rendering/framebuffer_2d.h"
#include "rendering/framebuffer_cube.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/pssm_shadow_mapping.h"
#include "audio/audio_manager.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

#include "rendering/probe/envmap/envmap_probe_control.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"
#include "rendering/postprocess/filters/shadertoy_filter.h"
#include "rendering/postprocess/filters/default_filter.h"

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

/* Populators */
#include "terrain/populators/populator.h"

#include "util/noise_factory.h"
#include "util/img/write_bitmap.h"
#include "util/enum_options.h"
#include "util/mesh_factory.h"

#include "rendering/probe/gi/gi_probe_control.h"
#include "rendering/probe/spherical_harmonics/spherical_harmonics_control.h"
#include "rendering/shaders/gi/gi_voxel_debug_shader.h"

#include "asset/fbom/fbom.h"
#include "asset/byte_writer.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <math.h>

using namespace hyperion;

class SceneEditor : public Game {
public:
    PssmShadowMapping *shadows;

    std::vector<std::shared_ptr<Node>> m_raytested_entities;
    std::unordered_map<HashCode_t, std::shared_ptr<Node>> m_hit_to_entity;
    std::shared_ptr<Node> m_selected_node;
    bool m_dragging_node = false;
    float m_dragging_timer = 0.0f;
    bool m_left_click_left = false;
    RaytestHit m_ray_hit;
    bool m_is_ray_hit = false;
    std::vector<std::thread> m_threads;

    std::shared_ptr<ui::UIText> m_selected_node_text;
    std::shared_ptr<ui::UIButton> m_rotate_mode_btn;

    SceneEditor(const RenderWindow &window)
        : Game(window)
    {
        std::srand(std::time(nullptr));

        ShaderProperties defines;
    }

    ~SceneEditor()
    {
        for (auto &thread : m_threads) {
            thread.join();
        }

        delete shadows;

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

        auto particle_node = std::make_shared<Node>();
        particle_node->SetName("Particle node");
        // particle_node->SetRenderable(std::make_shared<ParticleRenderer>(particle_generator_info));
        particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/test_snowflake.png"));
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(GetCamera(), particle_generator_info));
        particle_node->SetLocalScale(Vector3(1.5));
        particle_node->AddControl(std::make_shared<BoundingBoxControl>());
        particle_node->SetLocalTranslation(Vector3(0, 165, 0));
        particle_node->AddControl(std::make_shared<CameraFollowControl>(GetCamera(), Vector3(0, 2, 0)));

        GetScene()->AddChild(particle_node);
    }

    std::shared_ptr<Cubemap> InitCubemap()
    {
        AssetManager *asset_manager = AssetManager::GetInstance();
        std::shared_ptr<Cubemap> cubemap(new Cubemap({
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/posx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/negx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/posy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/negy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/posz.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/chapel/negz.jpg")
        }));

        cubemap->SetFilter(CoreEngine::GLEnums::LINEAR, CoreEngine::GLEnums::LINEAR_MIPMAP_LINEAR);

        if (!ProbeManager::GetInstance()->EnvMapEnabled()) {
            Environment::GetInstance()->SetGlobalCubemap(cubemap);
        }

        return cubemap;
    }

    void InitTestArea()
    {
        AssetManager *asset_manager = AssetManager::GetInstance();
        bool voxel_debug = false;

        /*auto mitsuba = asset_manager->LoadFromFile<Entity>("models/mitsuba.obj");
        //living_room->Scale(0.05f);
        mitsuba->Move(Vector3(-4.5, 1.2, -4.5));
        mitsuba->Scale(2);
        mitsuba->GetChild(0)->GetMaterial().diffuse_color = Vector4(1.0);
        mitsuba->GetChild(0)->GetMaterial().SetParameter(MATERIAL_PARAMETER_EMISSIVENESS, 60.0f);
        for (size_t i = 0; i < mitsuba->NumChildren(); i++) {
            if (mitsuba->GetChild(i)->GetRenderable() == nullptr) {
                continue;
            }

            if (voxel_debug)
                mitsuba->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
        }
        GetScene()->AddChild(mitsuba);*/

        auto sponza = asset_manager->LoadFromFile<Node>("models/sponza/sponza.obj");
        sponza->SetName("sponza");
        sponza->Scale(Vector3(0.01f));
        //if (voxel_debug) {
            for (size_t i = 0; i < sponza->NumChildren(); i++) {
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                if (sponza->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }
                if (voxel_debug) {
                    sponza->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                }
            }
        //}
        GetScene()->AddChild(sponza);
        sponza->Update(1.0f);
        sponza->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        sponza->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        //GetScene()->AddControl(std::make_shared<LightVolumeGridControl>(Vector3(), BoundingBox(Vector3(-25), Vector3(25))));
        return;
        /*{

            auto street = asset_manager->LoadFromFile<Entity>("models/street/street.obj");
            street->SetName("street");
            // street->SetLocalTranslation(Vector3(3.0f, -0.5f, -1.5f));
            street->Scale(0.6f);

            for (size_t i = 0; i < street->NumChildren(); i++) {
                if (voxel_debug)
                    street->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
            }

            GetScene()->AddChild(street);
            street->UpdateTransform();
            street->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
            street->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        }*/
        /*{

            auto fireplace_room = asset_manager->LoadFromFile<Entity>("models/fireplace_room/fireplace_room.obj");
            fireplace_room->SetName("fireplace_room");
            fireplace_room->SetLocalTranslation(Vector3(6.0f, -0.5f, 5.5f));
            fireplace_room->Scale(7.0f);

                fireplace_room->GetChild("grey_and_white_room:Floor")->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.05f);
                fireplace_room->GetChild("grey_and_white_room:Floor")->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.2f);

            GetScene()->AddChild(fireplace_room);
            fireplace_room->Update(1.0f);
            fireplace_room->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 3.0f, 4.0f)));
            fireplace_room->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        }*/

        /*{
            auto sdb = asset_manager->LoadFromFile<Entity>("models/salle_de_bain/salle_de_bain.obj");
            sdb->SetName("salle_de_bain");
            GetScene()->AddChild(sdb);
            sdb->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 3.0f, 4.0f)));
            sdb->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        }*/

        {
            auto model = asset_manager->LoadFromFile<Node>("models/sibenik/sibenik.obj");
            model->SetName("sibenik");
            GetScene()->AddChild(model);
            model->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 3.0f, 4.0f)));
            model->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        }
    }

    void PerformRaytest()
    {
        m_raytested_entities.clear();

        Ray ray;
        ray.m_direction = GetCamera()->GetDirection();
        ray.m_direction.Normalize();
        ray.m_position = GetCamera()->GetTranslation();


        using Intersection_t = std::pair<std::shared_ptr<Node>, RaytestHit>;

        RaytestHit intersection;
        std::vector<Intersection_t> intersections = { { GetScene(), intersection } };

        while (true) {
            std::vector<Intersection_t> new_intersections;

            for (auto& it : intersections) {
                for (size_t i = 0; i < it.first->NumChildren(); i++) {
                    if (const auto &child = it.first->GetChild(i)) {
                        BoundingBox aabb = child->GetAABB();

                        if (aabb.IntersectRay(ray, intersection)) {
                            new_intersections.push_back({ child, intersection });
                        }
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

        // RAY HIT POSITION / NORMAL

        RaytestHitList_t mesh_intersections;
        m_hit_to_entity.clear();

        for (Intersection_t& it : intersections) {
            // it.first->AddControl(std::make_shared<BoundingBoxControl>());

            m_raytested_entities.push_back(it.first);

            if (auto renderable = it.first->GetRenderable()) {
                RaytestHitList_t entity_hits;
                if (renderable->IntersectRay(ray, it.first->GetGlobalTransform(), entity_hits)) {
                    mesh_intersections.insert(mesh_intersections.end(), entity_hits.begin(), entity_hits.end());

                    for (auto& hit : entity_hits) {
                        m_hit_to_entity[hit.GetHashCode().Value()] = it.first;
                    }
                }

            }
        }

        if (mesh_intersections.empty()) {
            m_is_ray_hit = false;
            return;
        }

        std::sort(mesh_intersections.begin(), mesh_intersections.end(), [=](const RaytestHit& a, const RaytestHit& b) {
            return a.hitpoint.Distance(GetCamera()->GetTranslation()) < b.hitpoint.Distance(GetCamera()->GetTranslation());
        });

        m_ray_hit = mesh_intersections[0];
        m_is_ray_hit = true;
    }

    void Initialize()
    {

        ShaderManager::GetInstance()->SetBaseShaderProperties(ShaderProperties()
            .Define("NORMAL_MAPPING", true)
            .Define("SHADOW_MAP_RADIUS", 0.02f)
            .Define("SHADOW_PCF", true)
        );

        AssetManager *asset_manager = AssetManager::GetInstance();

        shadows = new PssmShadowMapping(GetCamera(), 4, 30.0f);
        shadows->SetVarianceShadowMapping(false);

        Environment::GetInstance()->SetShadowsEnabled(false);
        Environment::GetInstance()->SetNumCascades(4);
        Environment::GetInstance()->GetProbeManager()->SetEnvMapEnabled(false);
        Environment::GetInstance()->GetProbeManager()->SetSphericalHarmonicsEnabled(false);
        Environment::GetInstance()->GetProbeManager()->SetVCTEnabled(false);

        GetRenderer()->GetPostProcessing()->AddFilter<SSAOFilter>("ssao", 5);
        //GetRenderer()->GetPostProcessing()->AddFilter<DepthOfFieldFilter>("depth of field", 50);
        //GetRenderer()->GetPostProcessing()->AddFilter<BloomFilter>("bloom", 80);
        //GetRenderer()->GetPostProcessing()->AddFilter<GammaCorrectionFilter>("gamma correction", 100);
        //GetRenderer()->GetPostProcessing()->AddFilter<FXAAFilter>("fxaa", 9999);
        GetRenderer()->SetDeferred(true);

        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(1, 1, 0).Normalize());
        Environment::GetInstance()->GetSun().SetIntensity(500000.0f);

        //auto gi_test_node = std::make_shared<Node>("gi_test_node");
        //gi_test_node->Move(Vector3(0, 5, 0));
        //gi_test_node->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f)));
        //gi_test_node->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 1.0f, 0.0f), BoundingBox(-25.0f, 25.0f)));
        //gi_test_node->AddControl(std::make_shared<SphericalHarmonicsControl>(Vector3(0.0f, 1.0f, 0.0f), BoundingBox(-25.0f, 25.0f)));
        //GetScene()->AddChild(gi_test_node);


        GetCamera()->SetTranslation(Vector3(0, 0, 0));





        // Initialize particle system
        // InitParticleSystem();

        auto ui_text = std::make_shared<ui::UIText>("text_test", "Hyperion 0.1.0\n"
            "Press 1 to toggle shadows\n"
            "Press 2 to toggle deferred rendering\n"
            "Press 3 to toggle environment cubemapping\n"
            "Press 4 to toggle spherical harmonics mapping\n"
            "Press 5 to toggle voxel cone tracing\n");
        ui_text->SetLocalTranslation2D(Vector2(-1.0, 1.0));
        ui_text->SetLocalScale2D(Vector2(20));
        GetUI()->AddChild(ui_text);
        GetUIManager()->RegisterUIObject(ui_text);

        auto fps_counter = std::make_shared<ui::UIText>("fps_coutner", "- FPS");
        fps_counter->SetLocalTranslation2D(Vector2(0.8, 1.0));
        fps_counter->SetLocalScale2D(Vector2(30));
        GetUI()->AddChild(fps_counter);
        GetUIManager()->RegisterUIObject(fps_counter);

        m_selected_node_text = std::make_shared<ui::UIText>("selected_node_text", "No object selected");
        m_selected_node_text->SetLocalTranslation2D(Vector2(-1.0, -0.8));
        m_selected_node_text->SetLocalScale2D(Vector2(15));
        GetUI()->AddChild(m_selected_node_text);
        GetUIManager()->RegisterUIObject(m_selected_node_text);

        m_rotate_mode_btn = std::make_shared<ui::UIButton>("rotate_mode");
        m_rotate_mode_btn->SetLocalTranslation2D(Vector2(-1.0, -0.6));
        m_rotate_mode_btn->SetLocalScale2D(Vector2(15));
        GetUI()->AddChild(m_rotate_mode_btn);
        GetUIManager()->RegisterUIObject(m_rotate_mode_btn);

        auto cm = InitCubemap();

        GetScene()->AddControl(std::make_shared<SkyboxControl>(GetCamera(), nullptr));
       // GetScene()->AddControl(std::make_shared<SkydomeControl>(GetCamera()));

        m_threads.emplace_back(std::thread([scene = GetScene(), asset_manager]() {
            auto sponza = asset_manager->LoadFromFile<Node>("models/sponza/sponza.obj");
            sponza->SetName("sponza");
            sponza->Scale(Vector3(0.01f));
            for (size_t i = 0; i < sponza->NumChildren(); i++) {
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                if (sponza->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }
            }
            scene->AddChildAsync(sponza, [](const std::shared_ptr<Node> &) {
                std::cout << "!!! ADDED\n";
            });
        }));

        bool write = false;
        bool read = false;

        if (!write && !read) {
            //InitTestArea();
            /*GetScene()->AddControl(std::make_shared<NoiseTerrainControl>(GetCamera()));
            auto tmp = std::make_shared<Entity>("dummy node for vct");
            tmp->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0, 0, 0), BoundingBox(Vector3(-15, 5, -15), Vector3(15, 5, 15))));
            tmp->AddControl(std::make_shared<GIProbeControl>(Vector3(0, 0, 0)));
            GetScene()->AddChild(tmp);*/
        } else {
            if (write) {
                InitTestArea();
                FileByteWriter fbw(AssetManager::GetInstance()->GetRootDir() + "models/sdb.fbom");
                fbom::FBOMWriter writer;
                writer.Append(GetScene()->GetChild("salle_de_bain").get());
                auto res = writer.Emit(&fbw);
                fbw.Close();

                if (res != fbom::FBOMResult::FBOM_OK) {
                    throw std::runtime_error(std::string("FBOM Error: ") + res.message);
                }
            }

            if (read) {

                std::shared_ptr<Loadable> result = fbom::FBOMLoader().LoadFromFile("models/scene.fbom");

                if (auto entity = std::dynamic_pointer_cast<Node>(result)) {
                    for (size_t i = 0; i < entity->NumChildren(); i++) {
                        if (auto child = entity->GetChild(i)) {
                            if (auto ren = child->GetRenderable()) {
                                ren->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
                                //ren->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                            }
                        }
                    }

                    GetScene()->AddChild(entity);
                    entity->GetChild("mesh0_SG")->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 3.0f, 4.0f)));
                    entity->GetChild("mesh0_SG")->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 2.0f, 0.0f)));
                }
            }
        }

        /*GetSceneManager()->GetOctree()->AddCallback([this](OctreeChangeEvent evt, const Octree *oct, int node_id, const Spatial *spatial) {
            std::cout << "event " << evt << ", node: " << node_id << "\n";
            if (evt == OCTREE_INSERT_OCTANT) {
                std::cout << "INSERT OCTANT " << oct->GetAABB() << "\n";
                auto bb_node = std::make_shared<Node>("oct_" + std::to_string(intptr_t(oct)));
                bb_node->SetRenderable(std::make_shared<BoundingBoxRenderer>(oct->GetAABB()));
                bb_node->SetAABBAffectsParent(false);
                //GetScene()->AddChild(bb_node);
                //std::cout << "<ADDED BB NODE>\n";
            } else if (evt == OCTREE_REMOVE_OCTANT) {
                std::cout << "REMOVE OCTANT " << oct->GetAABB() << "\n";
                auto node = GetScene()->GetChild("oct_" + std::to_string(intptr_t(oct)));
                if (node != nullptr) {
                    //GetScene()->RemoveChild(node);
                    //std::cout << "<REMOVE BB NODE>\n";
                }
            }
        });*/
        //m_octree->InsertNode(non_owning_ptr(GetScene().get()));
        //BuildOctreeAABB(m_octree);

        //dragger->SetGlobalTranslation({ 5, 2, 5 });
        //dragger->Update(1.0f);
        //m_octree->InsertNode(non_owning_ptr(dragger.get()));

        //auto d2 = std::dynamic_pointer_cast<Entity>(dragger->Clone());
        //d2->SetGlobalTranslation({ -5, 2, -5 });
        //d2->Update(1.0);
        //GetScene()->AddChild(d2);
        //m_octree->InsertNode(non_owning_ptr(d2.get()));

        /*auto octree_test = std::make_shared<Entity>("octreetest_root");
        octree_test->SetRenderable(MeshFactory::CreateCube());
        octree_test->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
        octree_test->GetMaterial().diffuse_color = Vector4(0, 0, 1, 1);
        octree_test->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.0f);
        octree_test->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
        auto octree_sub = std::make_shared<Entity>("octreetest_sub");
        octree_sub->SetRenderable(MeshFactory::CreateCube());
        octree_sub->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
        octree_sub->GetMaterial().diffuse_color = Vector4(1, 0, 0, 1);
        octree_sub->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.0f);
        octree_sub->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
        octree_sub->Move(Vector3(3, 5, 3));
        octree_sub->Scale(Vector3(0.25f));
        GetScene()->AddChild(octree_sub);
        octree_test->Move(Vector3(5, 2, 5));
        GetScene()->AddChild(octree_test);
        //octree_test->Update(1.0f);
        //m_octree->InsertNode(non_owning_ptr(octree_test.get()));

        auto octree_test2 = std::make_shared<Entity>("octreetest2_root");
        octree_test2->SetRenderable(MeshFactory::CreateCube());
        octree_test2->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
        octree_test2->GetMaterial().diffuse_color = Vector4(0, 1, 0, 1);
        
        octree_test2->Move(Vector3(-5, 2, -5));
        octree_test2->Scale(Vector3(0.25f));
        GetScene()->AddChild(octree_test2);*/
        //GetScene()->Update(1.0f);
        //m_octree->InsertNode(non_owning_ptr(GetScene().get()));


        /*m_octree->Divide();
        m_octree->GetOctants()[0]->Divide();
        m_octree->GetOctants()[0]->GetOctants()[0]->Divide();
        m_octree->GetOctants()[0]->GetOctants()[0]->GetOctants()[0]->Divide();
        m_octree->GetOctants()[0]->GetOctants()[0]->GetOctants()[0]->Undivide();*/

        /*auto house = asset_manager->LoadFromFile<Node>("models/house.obj");
        for (size_t i = 0; i < house->NumChildren(); i++) {
            if (auto &child = house->GetChild(i)) {
                child->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                child->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.05f);
            }
        }
        GetScene()->AddChild(house);*/


        auto tv = asset_manager->LoadFromFile<Node>("models/television/Television_01_4k.obj", true);
        tv->SetLocalScale(4.f);
        for (size_t i = 0; i < tv->NumChildren(); i++) {
            tv->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_FLIP_UV, Vector2(0, 1));
        }
            
        GetScene()->AddChild(tv);


        bool add_spheres = true;

        if (add_spheres) {

            for (int x = 0; x < 5; x++) {
                for (int z = 0; z < 5; z++) {
                    Vector3 box_position = Vector3(((float(x) - 3)), 2.0f, (float(z)));
                    Vector3 col = Vector3(
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f)
                    ).Normalize();
                    auto box = asset_manager->LoadFromFile<Node>("models/material_sphere/material_sphere.obj", true);
                    box->SetLocalScale(0.4f);


                    for (size_t i = 0; i < box->NumChildren(); i++) {

                        box->GetChild(i)->GetMaterial().diffuse_color = Vector4(
                            1.f,//col.x,
                            1.f,//1.0,//col.y,
                            1.f,//1.0,//col.z,
                            1.f
                        );
                       // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", asset_manager->LoadFromFile<Texture2D>("models/monkey/albedo.png"));
                       // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", asset_manager->LoadFromFile<Texture2D>("textures/columned-lava-rock-unity/columned-lava-rock_height.png"));
                       // box->GetChild(0)->GetMaterial().SetTexture("AoMap", asset_manager->LoadFromFile<Texture2D>("models/monkey/ao.png"));
                       // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", asset_manager->LoadFromFile<Texture2D>("models/monkey/normal.png"));
                       // box->GetChild(0)->GetMaterial().SetTexture("RoughnessMap", asset_manager->LoadFromFile<Texture2D>("models/monkey/roughness.png"));
                       // box->GetChild(0)->GetMaterial().SetTexture("MetalnessMap", asset_manager->LoadFromFile<Texture2D>("models/monkey/metallic.png"));


                        box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", asset_manager->LoadFromFile<Texture2D>("textures/bamboo_wood/bamboo-wood-semigloss-albedo.png"));
                        //box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", asset_manager->LoadFromFile<Texture2D>("textures/columned-lava-rock-unity/columned-lava-rock_height.png"));
                        box->GetChild(0)->GetMaterial().SetTexture("AoMap", asset_manager->LoadFromFile<Texture2D>("textures/bamboo_wood/bamboo-wood-semigloss-ao.png"));
                        box->GetChild(0)->GetMaterial().SetTexture("NormalMap", asset_manager->LoadFromFile<Texture2D>("textures/bamboo_wood/bamboo-wood-semigloss-normal.png"));
                        box->GetChild(0)->GetMaterial().SetTexture("RoughnessMap", asset_manager->LoadFromFile<Texture2D>("textures/bamboo_wood/bamboo-wood-semigloss-roughness.png"));
                        box->GetChild(0)->GetMaterial().SetTexture("MetalnessMap", asset_manager->LoadFromFile<Texture2D>("textures/bomboo_wood/bamboo_wood/bamboo-wood-semigloss-metal.png"));
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_FLIP_UV, Vector2(0, 1));

                        //box->GetChild(i)->GetMaterial().SetTexture("DiffuseMap", asset_manager->LoadFromFile<Texture2D>("textures/plastic/plasticpattern1-albedo.png"));
                        //box->GetChild(i)->GetMaterial().SetTexture("ParallaxMap", asset_manager->LoadFromFile<Texture2D>("textures/nylon-tent-fabric1-unity/nylon-tent-fabric_height.png"));
                        //box->GetChild(i)->GetMaterial().SetTexture("AoMap", asset_manager->LoadFromFile<Texture2D>("textures/nylon-tent-fabric1-unity/nylon-tent-ao.png"));
                        //box->GetChild(i)->GetMaterial().SetTexture("NormalMap", asset_manager->LoadFromFile<Texture2D>("textures/plastic/plasticpattern1-normal2-unity2b.png"));
                        //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.1f);
                        //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, x / 5.0f);
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, z / 5.0f);
                    }

                    //Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(box_position + Vector3(0, 1, 0), Vector4(col.x, col.y, col.z, 1.0f) * 1.0f, 2.0f));

                    box->SetLocalTranslation(box_position);
                    GetScene()->AddChild(box);
                }
            }
        }
        
        //Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 1, 0), Vector4(1.0f, 0.0f, 0.0f, 1.0f) * 3.0f, 1.5f));

        auto ui_crosshair = std::make_shared<ui::UIObject>("crosshair");
        ui_crosshair->GetMaterial().SetTexture("ColorMap", asset_manager->LoadFromFile<Texture2D>("textures/crosshair.png"));
        ui_crosshair->SetLocalTranslation2D(Vector2(0));
        ui_crosshair->SetLocalScale2D(Vector2(128));
        GetUI()->AddChild(ui_crosshair);
        GetUIManager()->RegisterUIObject(ui_crosshair);


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

            GetRenderer()->SetDeferred(!GetRenderer()->IsDeferred());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_3, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetEnvMapEnabled(!ProbeManager::GetInstance()->EnvMapEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_4, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetSphericalHarmonicsEnabled(!ProbeManager::GetInstance()->SphericalHarmonicsEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_5, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetVCTEnabled(!ProbeManager::GetInstance()->VCTEnabled());
        }));

        InputEvent raytest_event([=](bool pressed)
            {
                if (!pressed) {
                    return;
                }

                if (m_selected_node != nullptr) {
                    m_selected_node->RemoveControl(m_selected_node->GetControl<BoundingBoxControl>(0));
                }

                PerformRaytest();

                if (!m_is_ray_hit) {
                    return;
                }

                ex_assert(m_hit_to_entity.find(m_ray_hit.GetHashCode().Value()) != m_hit_to_entity.end());

                auto node = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];

                m_selected_node = node;
                m_selected_node->AddControl(std::make_shared<BoundingBoxControl>());

                std::stringstream ss;
                ss << "Selected object: ";
                ss << m_selected_node->GetName();
                ss << " ";
                ss << m_selected_node->GetGlobalTranslation();
                ss << " " << m_selected_node->GetAABB();
                
                m_selected_node_text->SetText(ss.str());

                /*auto cube = GetScene()->GetChild("cube");
                cube->SetGlobalTranslation(mesh_intersections[0].hitpoint);

                Matrix4 look_at;
                MatrixUtil::ToLookAt(look_at, mesh_intersections[0].normal, Vector3::UnitY());
                cube->SetGlobalRotation(Quaternion(look_at));*/
            });

        GetInputManager()->RegisterClickEvent(MOUSE_BTN_LEFT, raytest_event);
    }

    void Logic(double dt)
    {
        if (GetInputManager()->IsButtonDown(MouseButton::MOUSE_BTN_LEFT) && m_selected_node != nullptr) {
            //std::cout << "Left button down\n";
            if (!m_dragging_node) {
                m_dragging_timer += dt;

                if (m_dragging_timer >= 0.5f) {
                    m_dragging_node = true;
                }
            } else {
                PerformRaytest();

                if (m_is_ray_hit) {
                    // check what it is intersecting with
                    auto intersected_with = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];

                    ex_assert(intersected_with != nullptr);

                    if (intersected_with != m_selected_node) {
                        m_selected_node->SetGlobalTranslation(m_ray_hit.hitpoint);
                        m_selected_node->UpdateTransform();

                        std::stringstream ss;
                        ss << "Selected object: ";
                        ss << m_selected_node->GetName();
                        ss << " ";
                        ss << m_selected_node->GetGlobalTranslation();

                        m_selected_node_text->SetText(ss.str());
                    }
                }
            }
        } else {
            m_dragging_node = false;
            m_dragging_timer = 0.0f;
        }

        AudioManager::GetInstance()->SetListenerPosition(GetCamera()->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(GetCamera()->GetDirection(), GetCamera()->GetUpVector());

        PhysicsManager::GetInstance()->RunPhysics(dt);

    }

    void OnRender()
    {
        if (Environment::GetInstance()->ShadowsEnabled()) {
            Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
            // shadow_dir.SetY(-1.0f);
            shadows->SetOrigin(GetCamera()->GetTranslation());
            shadows->SetLightDirection(shadow_dir);
            shadows->Render(GetRenderer());
        }

        Environment::GetInstance()->CollectVisiblePointLights(GetCamera());
    }
};

int main()
{
    // timing test
    /*{ // fbom
        using namespace std;
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
    
        // Call the function, here sort()
        std::shared_ptr<Loadable> result;
        for (int i = 0; i < 100; i++) {
            result = fbom::FBOMLoader().LoadFromFile("./test.fbom");
        }
    
        // Get ending timepoint
        auto stop = high_resolution_clock::now();
    
        // Get duration. Substart timepoints to 
        // get durarion. To cast it to proper unit
        // use duration cast method
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

        std::cout << "FBOM time: " << duration << "\n";
    }
    // timing test
    { // obj
        using namespace std;
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
    
        // Call the function, here sort()
        std::shared_ptr<Loadable> result;
        for (int i = 0; i < 100; i++) {
            result = asset_manager->LoadFromFile<Entity>("models/sphere_hq.obj", false);
        }
    
        // Get ending timepoint
        auto stop = high_resolution_clock::now();
    
        // Get duration. Substart timepoints to 
        // get durarion. To cast it to proper unit
        // use duration cast method
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

        std::cout << "OBJ time: " << duration << "\n";
    }*/


    // std::shared_ptr<Entity> my_entity = std::make_shared<Entity>("FOO BAR");
    // my_entity->AddControl(std::make_shared<NoiseTerrainControl>(nullptr, 12345));

    // auto my_entity = asset_manager->LoadFromFile<Entity>("models/sphere_hq.obj", true);
    // my_entity->Scale(Vector3(0.2f));
    // my_entity->Move(Vector3(0, 2, 0));

    // FileByteWriter fbw("test.fbom");
    // fbom::FBOMWriter writer;
    // writer.Append(my_entity.get());
    // auto res = writer.Emit(&fbw);
    // fbw.Close();

    // if (res != fbom::FBOMResult::FBOM_OK) {
    //     throw std::runtime_error(std::string("FBOM Error: ") + res.message);
    // }

    // return 0;

    /*std::shared_ptr<Loadable> result = fbom::FBOMLoader().LoadFromFile("./test.fbom");

    if (auto entity = std::dynamic_pointer_cast<Entity>(result)) {
        std::cout << "Loaded entity name: " << entity->GetName() << "\n";
    }

    return 0;*/

    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    std::string base_path = HYP_ROOT_DIR;
    AssetManager::GetInstance()->SetRootDir(base_path+"/res/");

    auto *game = new SceneEditor(RenderWindow(1480, 1200, "Hyperion Demo"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}
