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
#include "rendering/vulkan/vk_renderer.h"
#include "rendering/vulkan/renderer_descriptor_pool.h"
#include "rendering/vulkan/renderer_descriptor_set.h"
#include "rendering/vulkan/renderer_descriptor.h"
#include "rendering/vulkan/renderer_image.h"
#include "rendering/vulkan/renderer_fbo.h"
#include "rendering/vulkan/renderer_render_pass.h"

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
                m_dragging_timer += dt;

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

int main()
{
    std::string base_path = HYP_ROOT_DIR;
    AssetManager::GetInstance()->SetRootDir(base_path + "/res/");

    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    auto my_node = AssetManager::GetInstance()->LoadFromFile<Node>("models/mitsuba.obj");
    auto mesh = std::dynamic_pointer_cast<Mesh>(my_node->GetChild(0)->GetRenderable());

    /* Max frames/sync objects to have available to render to. This prevents the graphics
     * pipeline from stalling when waiting for device upload/download. */
    const uint16_t pending_frames = 2;

    VkRenderer renderer(system, "Hyperion Vulkan Test", "HyperionEngine");

    auto renderer_initialize_result = renderer.Initialize(true);
    AssertThrowMsg(renderer_initialize_result, "%s", renderer_initialize_result.message);

    RendererDevice *device = renderer.GetRendererDevice();

    /* Test fbo */
    //RendererFramebufferObject test_fbo(1024, 768);// 512, 512);

    auto color_format = device->GetRendererFeatures().FindSupportedFormat(
        std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    );
    
    auto depth_format = device->GetRendererFeatures().FindSupportedFormat(
        std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    /* Descriptor sets */
    RendererGPUBuffer test_gpu_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // test image
    auto texture = AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirt.jpg");
    RendererImage image(
        texture->GetWidth(),
        texture->GetHeight(),
        1,
        texture->GetInternalFormat(),
        texture->GetTextureType(),
        RendererImage::InternalInfo{
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        },
        texture->GetBytes()
    );
    RendererImageView test_image_view(VK_IMAGE_ASPECT_COLOR_BIT);
    RendererSampler test_sampler(
        Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST,
        Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    );

    RendererShader shader;
    shader.AttachShader(device, SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read() });
    shader.AttachShader(device, SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/frag.spv").Read() });
    shader.CreateProgram("main");

    RendererShader mirror_shader;
    mirror_shader.AttachShader(device, SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/mirror_vert.spv").Read() });
    mirror_shader.AttachShader(device, SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/mirror_frag.spv").Read() });
    mirror_shader.CreateProgram("main");

    std::array<RendererPipeline *, 2> pipelines{};

    /* Add a "default" pipeline
     * we'll have to set up a framebuffer with attachments for any image views retrieved
     * from the swapchain */

    RendererPipeline::Builder root_pipeline_builder;

    root_pipeline_builder
        .Shader(&shader)
        .VertexAttributes(RendererMeshInputAttributeSet({
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_POSITIONS).GetAttributeDescription(0),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_NORMALS).GetAttributeDescription(1),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS0).GetAttributeDescription(2),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS1).GetAttributeDescription(3),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TANGENTS).GetAttributeDescription(4),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_BITANGENTS).GetAttributeDescription(5)
        }))
        .RenderPass([&renderer, color_format, depth_format](RendererRenderPass &render_pass) {
            /* For our color attachment */
            render_pass.AddAttachment(RendererRenderPass::AttachmentInfo{
                .attachment = std::make_unique<RendererAttachment>(
                    renderer.swapchain->image_format,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    0,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = false
            });

            /* For our depth attachment */
            render_pass.AddAttachment(RendererRenderPass::AttachmentInfo{
                .attachment = std::make_unique<RendererAttachment>(
                    helpers::ToVkFormat(depth_format),
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    1,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = true
            });

            render_pass.AddDependency(VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            });
        });

    for (auto img : renderer.swapchain->images) {
        auto image_view = std::make_unique<RendererImageView>(VK_IMAGE_ASPECT_COLOR_BIT);

        auto image_view_result = image_view->Create(
            device,
            img,
            renderer.swapchain->image_format,
            VK_IMAGE_VIEW_TYPE_2D
        );

        AssertThrowMsg(image_view_result, "%s", image_view_result.message);

        root_pipeline_builder.Framebuffer(
            renderer.swapchain->extent.width,
            renderer.swapchain->extent.height,
            [&image_view, color_format, depth_format](RendererFramebufferObject &fbo) {
                /* Add color attachment */
                fbo.AddAttachment(
                    RendererFramebufferObject::AttachmentImageInfo{
                        .image = nullptr,
                        .image_view = std::move(image_view),
                        .sampler = nullptr,
                        .image_needs_creation = false,
                        .image_view_needs_creation = false,
                        .sampler_needs_creation = true
                    },
                    color_format, // unused
                    false
                );

                /* Now we add a depth buffer */
                fbo.AddAttachment(
                    depth_format,
                    true
                );
            });
    }

    auto add_pipeline_result = renderer.AddPipeline(std::move(root_pipeline_builder), &pipelines[0]);
    AssertThrowMsg(add_pipeline_result, "%s", add_pipeline_result.message);


    RendererPipeline::Builder fbo_pipeline_builder;

    fbo_pipeline_builder
        .Shader(&mirror_shader)
        .VertexAttributes(RendererMeshInputAttributeSet({
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_POSITIONS).GetAttributeDescription(0),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_NORMALS).GetAttributeDescription(1),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS0).GetAttributeDescription(2),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TEXCOORDS1).GetAttributeDescription(3),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_TANGENTS).GetAttributeDescription(4),
            mesh->GetAttributes().at(Mesh::MeshAttributeType::ATTR_BITANGENTS).GetAttributeDescription(5)
        }))
        .RenderPass([&renderer, color_format, depth_format](RendererRenderPass &render_pass) {
            /* For our color attachment */
            render_pass.AddAttachment(RendererRenderPass::AttachmentInfo{
                .attachment = std::make_unique<RendererAttachment>(
                    helpers::ToVkFormat(color_format),
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    0,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = false
            });

            /* For our depth attachment */
            render_pass.AddAttachment(RendererRenderPass::AttachmentInfo{
                .attachment = std::make_unique<RendererAttachment>(
                    helpers::ToVkFormat(depth_format),
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    1,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = true
            });

            render_pass.AddDependency(VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            });

            render_pass.AddDependency(VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            });
        })
        .Framebuffer(
            512,
            512,
            [color_format, depth_format](RendererFramebufferObject &fbo) {
                /* Add color attachment */
                fbo.AddAttachment(
                    color_format,
                    false
                );

                /* Now we add a depth buffer */
                fbo.AddAttachment(
                    depth_format,
                    true
                );
            });

    add_pipeline_result = renderer.AddPipeline(std::move(fbo_pipeline_builder), &pipelines[1]);
    AssertThrowMsg(add_pipeline_result, "%s", add_pipeline_result.message);

    renderer.descriptor_pool
        .AddDescriptorSet()
        .AddDescriptor(std::make_unique<RendererBufferDescriptor>(0, non_owning_ptr(&test_gpu_buffer), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT))
        .AddDescriptor(std::make_unique<RendererImageSamplerDescriptor>(1, non_owning_ptr(&test_image_view), non_owning_ptr(&test_sampler), VK_SHADER_STAGE_FRAGMENT_BIT));

    renderer.descriptor_pool
        .AddDescriptorSet()
        .AddDescriptor(std::make_unique<RendererImageSamplerDescriptor>(
            0,
            non_owning_ptr(pipelines[1]->GetConstructionInfo().fbos[0]->GetAttachmentImageInfos()[0].image_view.get()),
            non_owning_ptr(pipelines[1]->GetConstructionInfo().fbos[0]->GetAttachmentImageInfos()[0].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ));

    // test data for descriptors
    struct MainShaderData {
        alignas(16) Matrix4 model;
        alignas(16) Matrix4 pv;
    };
    test_gpu_buffer.Create(device, sizeof(MainShaderData));

    auto image_create_result = image.Create(
        device,
        &renderer,
        RendererImage::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>{},
        RendererImage::LayoutTransferState<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>{}
    );
    AssertThrowMsg(image_create_result, "%s", image_create_result.message);

    auto image_view_result = test_image_view.Create(device, &image);
    AssertThrowMsg(image_view_result, "%s", image_view_result.message);

    auto sampler_result = test_sampler.Create(device, &test_image_view);
    AssertThrowMsg(sampler_result, "%s", sampler_result.message);

    /* Initialize descriptor pool */
    auto descriptor_pool_result = renderer.descriptor_pool.Create(renderer.GetRendererDevice());
    AssertThrowMsg(descriptor_pool_result, "%s", descriptor_pool_result.message);


    // TODO: move out of here
    for (auto *pl : pipelines) {
        pl->Build(&renderer.descriptor_pool);
    }

    //pipelines[1]->GetConstructionInfo().fbos[0]->GetAttachmentImageInfos()[0]->image->GetGPUImage()->Map();

    float timer = 0.0;

    //float data[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    //set.GetDescriptor(0)->GetBuffer()->Copy(device, sizeof(data), data);

    Transform tf;
    tf.SetScale(Vector3(0.25f));

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

    RendererFrame *frame = nullptr;

    uint64_t tick_now = SDL_GetPerformanceCounter();
    uint64_t tick_last = 0;
    double delta_time = 0;

    while (running) {
        tick_last = tick_now;
        tick_now = SDL_GetPerformanceCounter();
        delta_time = ((double)tick_now-(double)tick_last) / (double)SDL_GetPerformanceFrequency();

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
        MainShaderData shad_data;

        Transform tf2;
        tf2.SetTranslation(Vector3(6, 0, 6));

        shad_data.model = tf.GetMatrix();
        shad_data.pv = camera->GetViewProjectionMatrix();


        RendererPipeline *pl = pipelines[0];
        RendererPipeline *fbo_pl = pipelines[1];

        frame = renderer.GetNextFrame();
        renderer.BeginFrame(frame);
        
        test_gpu_buffer.Copy(device, sizeof(shad_data), (void *)&shad_data);

        fbo_pl->StartRenderPass(frame->command_buffer, 0);
        renderer.descriptor_pool.BindDescriptorSets(frame->command_buffer, fbo_pl->layout, 0, 1);
        mesh->RenderVk(frame, &renderer, nullptr);
        fbo_pl->EndRenderPass(frame->command_buffer, 0);

        pl->StartRenderPass(frame->command_buffer, renderer.acquired_frames_index);
        renderer.descriptor_pool.BindDescriptorSets(frame->command_buffer, pl->layout);
        mesh->RenderVk(frame, &renderer, nullptr);
        pl->EndRenderPass(frame->command_buffer, renderer.acquired_frames_index);

        renderer.EndFrame(frame);


        //RendererPipeline *fbo_pl = pipelines[1];
        //fbo_pl->StartRenderPass(frame->command_buffer, )


        renderer.PresentFrame(frame);
    }

    renderer.WaitDeviceIdle();

    mesh.reset(); // TMP: here to delete the mesh, so that it doesn't crash when renderer is disposed before the vbo + ibo

    test_gpu_buffer.Destroy(device);
    test_image_view.Destroy(device);
    test_sampler.Destroy(device);
    image.Destroy(device);

    shader.Destroy();
    mirror_shader.Destroy();
    renderer.Destroy();
    delete window;

    return  0;


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
            result = asset_manager->LoadFromFile<Node>("models/sphere_hq.obj", false);
        }
    
        // Get ending timepoint
        auto stop = high_resolution_clock::now();
    
        // Get duration. Substart timepoints to 
        // get durarion. To cast it to proper unit
        // use duration cast method
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

        std::cout << "OBJ time: " << duration << "\n";
    }*/


    // std::shared_ptr<Node> my_entity = std::make_shared<Node>("FOO BAR");
    // my_entity->AddControl(std::make_shared<NoiseTerrainControl>(nullptr, 12345));

    // auto my_entity = asset_manager->LoadFromFile<Node>("models/sphere_hq.obj", true);
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

    if (auto entity = std::dynamic_pointer_cast<Node>(result)) {
        std::cout << "Loaded entity name: " << entity->GetName() << "\n";
    }

    return 0;*/

    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new SceneEditor(window);

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}
