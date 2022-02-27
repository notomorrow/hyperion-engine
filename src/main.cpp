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

#include "terrain/terrain_shader.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"
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
#include "rendering/shadow/shadow_map_control.h"

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
        particle_node->GetMaterial().SetTexture(MATERIAL_TEXTURE_DIFFUSE_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/test_snowflake.png"));
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

        cubemap->SetFilter(Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);

        if (!ProbeManager::GetInstance()->EnvMapEnabled()) {
            Environment::GetInstance()->SetGlobalCubemap(cubemap);
        }

        return cubemap;
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
            .Define("SHADOW_MAP_RADIUS", 0.005f)
            .Define("SHADOW_PCF", false)
            .Define("SHADOWS_VARIANCE", true)
        );

        AssetManager *asset_manager = AssetManager::GetInstance();

        Environment::GetInstance()->SetShadowsEnabled(true);
        Environment::GetInstance()->SetNumCascades(1);
        Environment::GetInstance()->GetProbeManager()->SetEnvMapEnabled(false);
        Environment::GetInstance()->GetProbeManager()->SetSphericalHarmonicsEnabled(false);
        Environment::GetInstance()->GetProbeManager()->SetVCTEnabled(false);

        GetRenderer()->GetDeferredPipeline()->GetPreFilterStack()->AddFilter<SSAOFilter>("ssao", 5);
        GetRenderer()->GetDeferredPipeline()->GetPreFilterStack()->AddFilter<FXAAFilter>("fxaa", 6);
        //GetRenderer()->GetDeferredPipeline()->GetPostFilterStack()->AddFilter<DepthOfFieldFilter>("depth of field", 50);
        GetRenderer()->GetDeferredPipeline()->GetPostFilterStack()->AddFilter<BloomFilter>("bloom", 80);

        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(0.5, 1, 0.5).Normalize());
        Environment::GetInstance()->GetSun().SetIntensity(700000.0f);
        Environment::GetInstance()->GetSun().SetColor(Vector4(1.0, 0.8, 0.65, 1.0));


        GetCamera()->SetTranslation(Vector3(0, 8, 0));

        // Initialize particle system
        // InitParticleSystem();

        auto ui_text = std::make_shared<ui::UIText>("text_test", "Hyperion 0.1.0\n"
            "Press 1 to toggle shadows\n"
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

        //GetScene()->AddControl(std::make_shared<SkyboxControl>(GetCamera(), nullptr));
        GetScene()->AddControl(std::make_shared<SkydomeControl>(GetCamera()));


#if 0

        //m_threads.emplace_back(std::thread([scene = GetScene(), asset_manager]() {
            auto model = asset_manager->LoadFromFile<Node>("models/san-miguel/san-miguel-low-poly.obj");
            model->SetName("model");
            model->Scale(Vector3(0.5f));
            for (size_t i = 0; i < model->NumChildren(); i++) {
                if (model->GetChild(i) == nullptr) {
                    continue;
                }

                if (model->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }

                if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "grey_and_white_room:Branches")) {
                    model->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                    model->GetChild(i)->GetSpatial().GetMaterial().alpha_blended = true;
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "grey_and_white_room:Floor")) {
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.05f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "grey_and_white_room:Leaves")) {
                    model->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                    model->GetChild(i)->GetSpatial().GetMaterial().alpha_blended = true;
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "grey_and_white_room:TableWood")) {
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.05f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "grey_and_white_room:Transluscent")) {
                    model->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                    model->GetChild(i)->GetSpatial().GetMaterial().alpha_blended = true;
                    model->GetChild(i)->GetMaterial().diffuse_color = Vector4(1.0, 1.0, 1.0, 0.5f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.05f);
                }

                if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "Bin")) {

                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.9f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.7f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "Ceramic")) {

                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.1f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.1f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "Mirror")) {

                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.9f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.01f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "Plastic")) {

                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.9f);
                }

                if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "leaf")) {
                    model->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                    model->GetChild(i)->GetSpatial().GetMaterial().alpha_blended = true;
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "Material__57")) {
                    model->GetChild(i)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                    model->GetChild(i)->GetSpatial().GetMaterial().alpha_blended = true;
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "vase")) {

                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.6f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "chain")) {
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else if (StringUtil::StartsWith(model->GetChild(i)->GetName(), "flagpole")) {
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                } else {
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.04f);
                    model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 1.0f);
                }
            }

            model->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
            model->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 3.0f, 4.0f)));
            GetScene()->AddChild(model);
        //}));
#else
        /*auto terrain = asset_manager->LoadFromFile<Node>("models/tmp_terrain.obj");
        terrain->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));
        terrain->Scale(20.0f);
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("DiffuseMap", asset_manager->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor_albedo.png"));
        ///terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("ParallaxMap", asset_manager->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor_Height.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("NormalMap", asset_manager->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor_Normal-ogl.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("AoMap", asset_manager->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor-ao.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetParameter(MATERIAL_PARAMETER_UV_SCALE, Vector2(50.0f));

        // GetMaterial().SetTexture("BaseTerrainColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Base_Color.png"));
        // GetMaterial().SetTexture("BaseTerrainNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Normal-ogl.png"));
        // GetMaterial().SetTexture("BaseTerrainParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Height.png"));
        // GetMaterial().SetTexture("BaseTerrainAoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Ambient_Occlusion.png"));

        // GetMaterial().SetTexture("SlopeColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Base_Color.png"));
        // GetMaterial().SetTexture("SlopeNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Normal-ogl.png"));
        // GetMaterial().SetTexture("SlopeParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Height.png"));
        // GetMaterial().SetTexture("SlopeAoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_AmbientOcculusion.png"));

        //GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/grass3.jpg"));
        //GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/grass_nrm.jpg"));
        terrain->GetChild(0)->GetSpatial().GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<TerrainShader>(ShaderProperties()));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_DIFFUSE_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_albedo.png")); // for vct
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_albedo.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_normal-dx.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_ao.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("BaseTerrainColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-albedo.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("BaseTerrainNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-normal-dx.png"));
        //terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture("BaseTerrainAoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-ao.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/damp-rocky-ground-ue/damp-rocky-ground1-albedo.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/damp-rocky-ground-ue/damp-rocky-ground1-Normal-dx.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/damp-rocky-ground-ue/damp-rocky-ground1-ao.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor_albedo.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor_Normal-ogl.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/forest-floor-unity/forest_floor-ao.png"));
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetTexture(MATERIAL_TEXTURE_SPLAT_MAP, AssetManager::GetInstance()->LoadFromFile<Texture>("textures/splat.png"));

        terrain->GetChild(0)->GetSpatial().GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.95f);
        terrain->GetChild(0)->GetSpatial().GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.0f);
        terrain->Scale({ 2, 2.5, 2 });
        //terrain->GetChild(0)->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);

        //tmp->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0, 0, 0), BoundingBox(Vector3(-15, 5, -15), Vector3(15, 5, 15))));
        terrain->AddControl(std::make_shared<GIProbeControl>(Vector3(0, 0, 0)));
        GetScene()->AddChild(terrain);
        terrain->Move({ 5, 0, -5 });


        auto tree = asset_manager->LoadFromFile<Node>("models/conifer/Conifer_Low.obj");
        for (size_t i = 0; i < tree->NumChildren(); i++) {
            if (auto child = tree->GetChild(i)) {
                //child->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                child->GetSpatial().GetMaterial().alpha_blended = true;
                child->GetSpatial().GetMaterial().cull_faces = MaterialFace_None;
            }
        }
        tree->SetLocalTranslation({ 21.f, 6.4f, -4.f });
        tree->Scale(0.05f);
        GetScene()->AddChild(tree);

        auto tree2 = asset_manager->LoadFromFile<Node>("models/conifer/Conifer_Low.obj");
        for (size_t i = 0; i < tree2->NumChildren(); i++) {
            if (auto child = tree2->GetChild(i)) {
                //child->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                child->GetSpatial().GetMaterial().alpha_blended = true;
                child->GetSpatial().GetMaterial().cull_faces = MaterialFace_None;
            }
        }
        tree2->SetLocalTranslation({ 22.f, 6.4f, -5.7f });
        tree2->SetLocalScale(0.059f);
        tree2->Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(35.0f)));
        GetScene()->AddChild(tree2);

        auto tree3 = asset_manager->LoadFromFile<Node>("models/conifer/Conifer_Low.obj");
        for (size_t i = 0; i < tree3->NumChildren(); i++) {
            if (auto child = tree3->GetChild(i)) {
                //child->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                child->GetSpatial().GetMaterial().alpha_blended = true;
                child->GetSpatial().GetMaterial().cull_faces = MaterialFace_None;
            }
        }
        tree3->SetLocalTranslation({ 24.f, 6.3f, -3.7f });
        tree3->SetLocalScale(0.045f);
        tree3->Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(-35.0f)));
        GetScene()->AddChild(tree3);

        auto tree4 = asset_manager->LoadFromFile<Node>("models/conifer/fir.obj");
        for (size_t i = 0; i < tree4->NumChildren(); i++) {
            if (auto child = tree4->GetChild(i)) {
                //child->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                child->GetSpatial().GetMaterial().alpha_blended = true;
                child->GetSpatial().GetMaterial().cull_faces = MaterialFace_None;
            }
        }
        tree4->SetLocalTranslation({ 24.f, 6.3f, -1.7f });
        tree4->SetLocalScale(0.05f);
        tree4->Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(-35.0f)));
        GetScene()->AddChild(tree4);

        auto tree5 = asset_manager->LoadFromFile<Node>("models/conifer/fir.obj");
        for (size_t i = 0; i < tree5->NumChildren(); i++) {
            if (auto child = tree5->GetChild(i)) {
                //child->GetSpatial().SetBucket(Spatial::Bucket::RB_TRANSPARENT);
                child->GetSpatial().GetMaterial().alpha_blended = true;
                child->GetSpatial().GetMaterial().cull_faces = MaterialFace_None;
            }
        }
        tree5->SetLocalTranslation({ 27.f, 7.f, -1.f });
        tree5->SetLocalScale(0.045f);
        tree5->Rotate(Quaternion(Vector3::UnitY(), MathUtil::DegToRad(-76.0f)));
        GetScene()->AddChild(tree5);*/
#endif
        /*auto house = asset_manager->LoadFromFile<Node>("models/house.obj");
        for (size_t i = 0; i < house->NumChildren(); i++) {
            if (auto &child = house->GetChild(i)) {
                child->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                child->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.05f);
            }
        }
        GetScene()->AddChild(house);*/

        auto shadow_node = std::make_shared<Node>("shadow_node");
        shadow_node->AddControl(std::make_shared<ShadowMapControl>(GetRenderer()->GetEnvironment()->GetSun().GetDirection() * -1.0f, 8.0f));
        shadow_node->AddControl(std::make_shared<CameraFollowControl>(GetCamera()));
        GetScene()->AddChild(shadow_node);
        
        //Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(0, 1, 0), Vector4(1.0f, 0.0f, 0.0f, 1.0f) * 3.0f, 1.5f));

        auto ui_crosshair = std::make_shared<ui::UIObject>("crosshair");
        ui_crosshair->GetMaterial().SetTexture(MATERIAL_TEXTURE_COLOR_MAP, asset_manager->LoadFromFile<Texture2D>("textures/crosshair.png"));
        ui_crosshair->SetLocalTranslation2D(Vector2(0));
        ui_crosshair->SetLocalScale2D(Vector2(128));
        GetUI()->AddChild(ui_crosshair);
        GetUIManager()->RegisterUIObject(ui_crosshair);


        bool add_spheres = true;

        if (add_spheres) {

            for (int x = 0; x < 5; x++) {
                for (int z = 0; z < 5; z++) {

                    Vector3 box_position = Vector3(((float(x))), 6.4f, (float(z)));

                    //m_threads.emplace_back(std::thread([=, scene = GetScene()]() {
                    auto box = asset_manager->LoadFromFile<Node>("models/cube.obj", true);
                    box->SetLocalScale(0.35f);

                    //box->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));

                    for (size_t i = 0; i < box->NumChildren(); i++) {
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_DIFFUSE_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-albedo.png"));
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_AO_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-ao.png"));
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_NORMAL_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-normal-dx.png"));
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_ROUGHNESS_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-roughness.png"));
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_METALNESS_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-metallic.png"));
                        box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_PARALLAX_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-height.png"));
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_FLIP_UV, Vector2(0, 1));
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_PARALLAX_HEIGHT, 0.25f);

                         //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_PARALLAX_HEIGHT, 5.0f);
                         //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.1f);
                         //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                         //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, x / 5.0f);
                         //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.2f);
                    }
                    //Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(box_position + Vector3(0, 1, 0), Vector4(col.x, col.y, col.z, 1.0f) * 1.0f, 2.0f));
                    box->SetLocalTranslation(box_position);
                    GetScene()->AddChildAsync(box, [](auto) {});
                    // }));
                }
            }
        }

        GetInputManager()->RegisterKeyEvent(KEY_1, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            Environment::GetInstance()->SetShadowsEnabled(!Environment::GetInstance()->ShadowsEnabled());
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
