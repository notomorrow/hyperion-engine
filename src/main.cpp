#include "glfw_engine.h"
#include "game.h"
#include "scene/node.h"
#include "util.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "asset/byte_reader.h"
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

#include "rendering/camera/fps_camera.h"

#include "util/mesh_factory.h"

#include "system/sdl_system.h"
#include "system/debug.h"
#include "rendering/backend/renderer_instance.h"
#include "rendering/backend/renderer_descriptor_pool.h"
#include "rendering/backend/renderer_descriptor_set.h"
#include "rendering/backend/renderer_descriptor.h"
#include "rendering/backend/renderer_image.h"
#include "rendering/backend/renderer_fbo.h"
#include "rendering/backend/renderer_render_pass.h"

#include <rendering/v2/engine.h>

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
#include <cmath>
#include <thread>

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

    SceneEditor(SystemWindow *window)
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
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/posx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/negx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/posy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/negy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/posz.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/Lycksele3/negz.jpg")
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
            it.first->AddControl(std::make_shared<BoundingBoxControl>());

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
        Environment::GetInstance()->GetProbeManager()->SetSphericalHarmonicsEnabled(true);
        Environment::GetInstance()->GetProbeManager()->SetVCTEnabled(false);

        GetRenderer()->GetDeferredPipeline()->GetPreFilterStack()->AddFilter<SSAOFilter>("ssao", 5);
        GetRenderer()->GetDeferredPipeline()->GetPreFilterStack()->AddFilter<FXAAFilter>("fxaa", 6);
        GetRenderer()->GetDeferredPipeline()->GetPostFilterStack()->AddFilter<BloomFilter>("bloom", 80);

        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(0.2, 1, 0.2).Normalize());
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

        GetScene()->AddControl(std::make_shared<SphericalHarmonicsControl>(Vector3(0.0f), BoundingBox(-25.0f, 25.0f)));


#if 1

        auto model = asset_manager->LoadFromFile<Node>("models/sponza/sponza.obj");
        model->SetName("model");
        model->Scale(Vector3(0.01f));
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

            if (auto result = fbom::FBOMLoader().LoadFromFile("models/scene.fbom")) {
                if (auto entity = std::dynamic_pointer_cast<Node>(result.loadable)) {
                    for (size_t i = 0; i < entity->NumChildren(); i++) {
                        if (auto child = entity->GetChild(i)) {
                            if (auto ren = child->GetRenderable()) {
                                ren->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
                            }
                        }
                    }

                    GetScene()->AddChild(entity);
                    entity->GetChild("mesh0_SG")->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 2.0f, 0.0f)));
                }
            }
        }
#endif

        auto shadow_node = std::make_shared<Node>("shadow_node");
        shadow_node->AddControl(std::make_shared<ShadowMapControl>(GetRenderer()->GetEnvironment()->GetSun().GetDirection() * -1.0f, 15.0f));
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
                        //box->GetChild(0)->GetMaterial().SetTexture(MATERIAL_TEXTURE_PARALLAX_MAP, asset_manager->LoadFromFile<Texture2D>("textures/rough-wet-cobble-ue/rough-wet-cobble-height.png"));
                        box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_FLIP_UV, Vector2(0, 1));
                        //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_PARALLAX_HEIGHT, 0.25f);

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

                AssertThrow(m_hit_to_entity.find(m_ray_hit.GetHashCode().Value()) != m_hit_to_entity.end());

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

        GetInputManager()->RegisterClickEvent(MouseButton::MOUSE_BUTTON_LEFT, raytest_event);
    }

    void Logic(double dt)
    {
        if (GetInputManager()->IsButtonDown(MouseButton::MOUSE_BUTTON_LEFT) && m_selected_node != nullptr) {
            //std::cout << "Left button down\n";
            if (!m_dragging_node) {
                m_dragging_timer += float(dt);

                if (m_dragging_timer >= 0.5f) {
                    m_dragging_node = true;
                }
            } else {
                PerformRaytest();

                if (m_is_ray_hit) {
                    // check what it is intersecting with
                    auto intersected_with = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];

                    AssertThrow(intersected_with != nullptr);

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


#define HYPERION_VK_TEST_CUBEMAP 1
#define HYPERION_VK_TEST_MIPMAP 0
#define HYPERION_VK_TEST_IMAGE_STORE 1

int main()
{
    using namespace hyperion::renderer;
    std::string base_path = HYP_ROOT_DIR;
    AssetManager::GetInstance()->SetRootDir(base_path + "/res/");

    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    auto my_node = AssetManager::GetInstance()->LoadFromFile<Node>("models/monkey/monkey.obj");
    auto monkey_mesh = std::dynamic_pointer_cast<Mesh>(my_node->GetChild(0)->GetRenderable());
    auto cube_mesh = MeshFactory::CreateCube();
    auto full_screen_quad = MeshFactory::CreateQuad();
    Material my_material;


    v2::Engine engine(system, "My app");



    /* Descriptor sets */
    GPUBuffer matrices_descriptor_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
              scene_data_descriptor_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    // test data for descriptors
    struct MatricesBlock {
        alignas(16) Matrix4 model;
        alignas(16) Matrix4 view;
        alignas(16) Matrix4 projection;
    } matrices_block;


    struct SceneDataBlock {
        alignas(16) Vector3 camera_position;
        alignas(16) Vector3 light_direction;
    } scene_data_block;

#if HYPERION_VK_TEST_CUBEMAP
    std::vector<std::shared_ptr<Texture2D>> cubemap_faces;
    cubemap_faces.resize(6);

    cubemap_faces[0] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posx.jpg");
    cubemap_faces[1] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negx.jpg");
    cubemap_faces[2] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posy.jpg");
    cubemap_faces[3] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negy.jpg");
    cubemap_faces[4] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posz.jpg");
    cubemap_faces[5] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negz.jpg");

    size_t cubemap_face_bytesize = cubemap_faces[0]->GetWidth() * cubemap_faces[0]->GetHeight() * Texture::NumComponents(cubemap_faces[0]->GetFormat());

    unsigned char *bytes = new unsigned char[cubemap_face_bytesize * 6];

    for (int i = 0; i < cubemap_faces.size(); i++) {
        std::memcpy(&bytes[i * cubemap_face_bytesize], cubemap_faces[i]->GetBytes(), cubemap_face_bytesize);
    }


    Image *image = new TextureImageCubemap(
        cubemap_faces[0]->GetWidth(),
        cubemap_faces[0]->GetHeight(),
        cubemap_faces[0]->GetInternalFormat(),
        Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR,
        bytes
    );

    ImageView test_image_view(VK_IMAGE_ASPECT_COLOR_BIT);
    Sampler test_sampler(
        Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR,
        Texture::TextureWrapMode::TEXTURE_WRAP_REPEAT
    );


    delete[] bytes;
#elif HYPERION_VK_TEST_MIPMAP
    // test image
    auto texture = AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dummy.jpg");
    Image *image = new TextureImage2D(
        texture->GetWidth(),
        texture->GetHeight(),
        texture->GetInternalFormat(),
        Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        texture->GetBytes()
    );

    ImageView test_image_view(VK_IMAGE_ASPECT_COLOR_BIT);
    Sampler test_sampler(
        Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Texture::TextureWrapMode::TEXTURE_WRAP_REPEAT
    );
#endif

#if HYPERION_VK_TEST_IMAGE_STORE
    renderer::Image *image_storage = new renderer::StorageImage(
        512,
        512,
        1,
        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Texture::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView image_storage_view(VK_IMAGE_ASPECT_COLOR_BIT);
#endif


    engine.Initialize();

    v2::RenderPass::ID render_pass_id{};
    {
        auto render_pass = std::make_unique<v2::RenderPass>(v2::RenderPass::RENDER_PASS_STAGE_SHADER, v2::RenderPass::RENDER_PASS_INLINE);

        /* For our color attachment */
        render_pass->AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
        });
        /* For our normals attachment */
        render_pass->AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });
        /* For our positions attachment */
        render_pass->AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });

        render_pass->AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)
        });



        render_pass_id = engine.AddRenderPass(std::move(render_pass));
    }

    v2::Framebuffer::ID my_fbo_id = engine.AddFramebuffer(engine.GetInstance()->swapchain->extent.width, engine.GetInstance()->swapchain->extent.height, render_pass_id);

    {
        auto *descriptor_set_globals = engine.GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBALS);

        descriptor_set_globals
            ->AddDescriptor(std::make_unique<BufferDescriptor>(0, non_owning_ptr(&matrices_descriptor_buffer), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT));

        descriptor_set_globals
            ->AddDescriptor(std::make_unique<BufferDescriptor>(1, non_owning_ptr(&scene_data_descriptor_buffer), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT));

        descriptor_set_globals
            ->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(2, non_owning_ptr(&test_image_view), non_owning_ptr(&test_sampler), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT));

#if HYPERION_VK_TEST_IMAGE_STORE
        descriptor_set_globals
            ->AddDescriptor(std::make_unique<ImageStorageDescriptor>(3, non_owning_ptr(&image_storage_view), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT));
#endif
    }

    {
        auto *descriptor_set_pass = engine.GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_PASS);

        /* Albedo texture */
        descriptor_set_pass
            ->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(
                0,
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[0].image_view.get()),
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[0].sampler.get()),
                VK_SHADER_STAGE_FRAGMENT_BIT
            ));

        /* Normals texture*/
        descriptor_set_pass
            ->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(
                1,
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[1].image_view.get()),
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[1].sampler.get()),
                VK_SHADER_STAGE_FRAGMENT_BIT
            ));

        /* Position texture */
        descriptor_set_pass
            ->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(
                2,
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[2].image_view.get()),
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[2].sampler.get()),
                VK_SHADER_STAGE_FRAGMENT_BIT
            ));

        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(
                3,
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[3].image_view.get()),
                non_owning_ptr(engine.GetFramebuffer(my_fbo_id)->GetWrappedObject()->GetAttachmentImageInfos()[3].sampler.get()),
                VK_SHADER_STAGE_FRAGMENT_BIT
            ));
    }

    Device *device = engine.GetInstance()->GetDevice();

    /* Initialize descriptor pool, has to be before any pipelines are created */
    {
        auto image_create_result = image->Create(
            device,
            engine.GetInstance(),
            Image::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>{},
            Image::LayoutTransferState<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>{}
        );
        AssertThrowMsg(image_create_result, "%s", image_create_result.message);

        auto image_view_result = test_image_view.Create(device, image);
        AssertThrowMsg(image_view_result, "%s", image_view_result.message);

        auto sampler_result = test_sampler.Create(device, &test_image_view);
        AssertThrowMsg(sampler_result, "%s", sampler_result.message);
    }
    {
        auto image_create_result = image_storage->Create(
            device,
            engine.GetInstance(),
            Image::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL>{}
        );
        AssertThrowMsg(image_create_result, "%s", image_create_result.message);

        auto image_view_result = image_storage_view.Create(device, image_storage);
        AssertThrowMsg(image_view_result, "%s", image_view_result.message);
    }

    matrices_descriptor_buffer.Create(device, sizeof(MatricesBlock));
    scene_data_descriptor_buffer.Create(device, sizeof(SceneDataBlock));

    engine.PrepareSwapchain();


    v2::Shader::ID mirror_shader_id{};
    {
        auto mirror_shader = std::make_unique<v2::Shader>(std::vector{
            SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read() },
            SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/forward_frag.spv").Read() }
        });

        mirror_shader_id = engine.AddShader(std::move(mirror_shader));
    }


    GraphicsPipeline::Builder scene_pass_pipeline_builder;

    scene_pass_pipeline_builder
        .Shader<v2::Shader>(mirror_shader_id)
        .VertexAttributes(MeshInputAttributeSet({
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_POSITIONS).GetAttributeDescription(0),
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_NORMALS).GetAttributeDescription(1),
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS0).GetAttributeDescription(2),
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS1).GetAttributeDescription(3),
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TANGENTS).GetAttributeDescription(4),
            monkey_mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_BITANGENTS).GetAttributeDescription(5)
        }))
        .RenderPass<v2::RenderPass>(render_pass_id)
        .Framebuffer<v2::Framebuffer>(my_fbo_id);

    v2::Pipeline::ID scene_pass_pipeline_id = engine.AddPipeline(std::move(scene_pass_pipeline_builder));

    

    //pipelines[1]->GetConstructionInfo().fbos[0]->GetAttachmentImageInfos()[0]->image->GetGPUImage()->Map();

    float timer = 0.0;

    //float data[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    //set.GetDescriptor(0)->GetBuffer()->Copy(device, sizeof(data), data);

    auto *input_manager = new InputManager(window);
    input_manager->SetWindow(window);

    auto *camera = new FpsCamera(
            input_manager,
            window,
            1024,
            768,
            70.0f,
            0.05f,
            250.0f
    );

    bool running = true;

    Frame *frame = nullptr;

    uint64_t tick_now = SDL_GetPerformanceCounter();
    uint64_t tick_last = 0;
    double delta_time = 0;


    /* Compute */

    v2::Shader::ID compute_shader_id{};
    {
        auto compute_shader = std::make_unique<v2::Shader>(std::vector{
            SpirvObject{ SpirvObject::Type::COMPUTE, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/imagestore.comp.spv").Read() }
            });

        compute_shader_id = engine.AddShader(std::move(compute_shader));
    }

    v2::ComputePipeline::ID compute_pipeline_id = engine.AddComputePipeline(std::make_unique<v2::ComputePipeline>(compute_shader_id));


    auto compute_command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);

    SemaphoreChain compute_semaphore_chain({}, { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    AssertThrow(compute_semaphore_chain.Create(engine.GetInstance()->GetDevice()));

    auto compute_cmd_result = compute_command_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetComputeCommandPool());
    AssertThrow(compute_cmd_result, "Failed to create compute commandbuffer: %s\n", compute_cmd_result.message);
    
    for (size_t i = 0; i < engine.GetInstance()->GetFrameHandler()->GetNumFrames(); i++) {
        compute_semaphore_chain >> engine.GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores();
    }

    

    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine.GetInstance()->GetFrameHandler()->GetNumFrames());

    SemaphoreChain graphics_semaphore_chain({}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});
    AssertThrow(graphics_semaphore_chain.Create(engine.GetInstance()->GetDevice()));

    for (uint32_t i = 0; i < per_frame_data.GetNumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_PRIMARY);
        AssertThrow(cmd_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsQueueData().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
        
        graphics_semaphore_chain >> engine.GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores();
    }
    
    engine.BuildPipelines();

    uint32_t previous_frame_index = 0;

    VkFence compute_fc;
    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(engine.GetInstance()->GetDevice()->GetDevice(), &fence_info, nullptr, &compute_fc) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute fence!");
    }
    

    while (running) {
        tick_last = tick_now;
        tick_now = SDL_GetPerformanceCounter();
        delta_time = (double(tick_now)-double(tick_last)) / double(SDL_GetPerformanceFrequency());

        while (SystemSDL::PollEvent(&event)) {
            input_manager->CheckEvent(&event);
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                default:
                    break;
            }
        }

        timer += delta_time;

        camera->Update(delta_time);

        Transform transform(Vector3(0, 0, 0), Vector3(1.0f), Quaternion(Vector3::One(), timer));

        matrices_block.model = transform.GetMatrix();
        matrices_block.view = camera->GetViewMatrix();
        matrices_block.projection = camera->GetProjectionMatrix();

        scene_data_block.camera_position = camera->GetTranslation();
        scene_data_block.light_direction = Vector3(-0.5, -0.5, 0).Normalize();


        frame = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();

        engine.GetInstance()->PrepareFrame(frame);

#if 1
        auto *compute_pipeline = engine.GetComputePipeline(compute_pipeline_id);
        compute_pipeline->GetWrappedObject()->push_constants.counter = timer;

        /* Compute */
        vkWaitForFences(engine.GetInstance()->GetDevice()->GetDevice(),
            1, &compute_fc, true, UINT64_MAX);
        vkResetFences(engine.GetInstance()->GetDevice()->GetDevice(),
            1, &compute_fc);
        
        compute_command_buffer->Reset(engine.GetInstance()->GetDevice());
        compute_cmd_result = compute_command_buffer->Record(engine.GetInstance()->GetDevice(), nullptr, [&](CommandBuffer *cmd) {
            compute_pipeline->Dispatch(&engine, cmd,
                8, 8, 1);

            HYPERION_RETURN_OK;
        });
        AssertThrowMsg(compute_cmd_result, "Failed to record compute cmd buffer: %s\n", compute_cmd_result.message);
        compute_command_buffer->SubmitPrimary(engine.GetInstance()->GetComputeQueue(), compute_fc, &compute_semaphore_chain);
#endif

        v2::Pipeline *fbo_pl = engine.GetPipeline(scene_pass_pipeline_id);
        
        matrices_descriptor_buffer.Copy(device, sizeof(matrices_block), (void *)&matrices_block);
        scene_data_descriptor_buffer.Copy(device, sizeof(scene_data_block), (void *)&scene_data_block);

        
        const uint32_t frame_index = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        per_frame_data[frame_index].Get<CommandBuffer>()->Reset(engine.GetInstance()->GetDevice());
        per_frame_data[frame_index].Get<CommandBuffer>()->Record(
            engine.GetInstance()->GetDevice(),
            nullptr,
            [&](CommandBuffer *command_buffer) {
                /* forward / albedo layer */
                fbo_pl->GetWrappedObject()->BeginRenderPass(command_buffer, 0, VK_SUBPASS_CONTENTS_INLINE);
                fbo_pl->GetWrappedObject()->Bind(command_buffer);
                engine.GetInstance()->GetDescriptorPool().BindDescriptorSets(command_buffer, fbo_pl->GetWrappedObject(), 0, 1);

                monkey_mesh->RenderVk(command_buffer, engine.GetInstance(), nullptr);
                fbo_pl->GetWrappedObject()->EndRenderPass(command_buffer, 0);


                engine.RenderPostProcessing(command_buffer, frame_index);

                HYPERION_RETURN_OK;
            });
        
        per_frame_data[frame_index].Get<CommandBuffer>()->SubmitPrimary(
            engine.GetInstance()->GetGraphicsQueue(), VK_NULL_HANDLE, &graphics_semaphore_chain
        );


        //renderer::SemaphoreChain sc(std::vector<std::tuple<VkPipelineStageFlags>>{ {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT } }, { {VK_SHADER_STAGE_COMPUTE_BIT} });

        /*VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        // We won't be changing the layout of the image
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.image = image_storage->GetGPUImage()->image;
        imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(
            frame->command_buffer->GetCommandBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);*/

        frame->BeginCapture();
        engine.GetPipeline(engine.GetSwapchainData().pipeline_id)->GetWrappedObject()->BeginRenderPass(frame->command_buffer.get(), frame_index, VK_SUBPASS_CONTENTS_INLINE);// Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
        
        frame->GetCommandBuffer()->RecordCommandsWithContext(
            [&engine, &frame_index, &previous_frame_index](CommandBuffer *cmd) {
                engine.GetPipeline(engine.GetSwapchainData().pipeline_id)->GetWrappedObject()->push_constants.previous_frame_index = previous_frame_index;
                engine.GetPipeline(engine.GetSwapchainData().pipeline_id)->GetWrappedObject()->push_constants.current_frame_index = frame_index;

                engine.RenderSwapchain(cmd);

                HYPERION_RETURN_OK;
            });
        engine.GetPipeline(engine.GetSwapchainData().pipeline_id)->GetWrappedObject()->EndRenderPass(frame->command_buffer.get(), frame_index);
        frame->EndCapture();

        frame->Submit(engine.GetInstance()->GetGraphicsQueue());

        engine.GetInstance()->PresentFrame(frame);

        previous_frame_index = frame_index;
    }

    AssertThrow(engine.GetInstance()->GetDevice()->Wait());

    graphics_semaphore_chain.Destroy(engine.GetInstance()->GetDevice());

    v2::Filter::full_screen_quad.reset();// have to do this here for now or else buffer does not get cleared before device is deleted

    monkey_mesh.reset(); // TMP: here to delete the mesh, so that it doesn't crash when renderer is disposed before the vbo + ibo
    cube_mesh.reset();
    full_screen_quad.reset();

    matrices_descriptor_buffer.Destroy(device);
    scene_data_descriptor_buffer.Destroy(device);

    image_storage_view.Destroy(device);
    test_image_view.Destroy(device);

    test_sampler.Destroy(device);
    image->Destroy(device);

    for (size_t i = 0; i < per_frame_data.GetNumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();

    compute_command_buffer->Destroy(device, engine.GetInstance()->GetComputeCommandPool());

    delete window;

    return 0;
}
