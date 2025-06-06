#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>

// temp
#include <rendering/ParticleSystem.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/Texture.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/RigidBodyComponent.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridPlugin.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIWindow.hpp>

#include <core/logging/Logger.hpp>

#include <core/net/HTTPRequest.hpp>

#include <scripting/Script.hpp>
#include <scripting/ScriptingService.hpp>

#include <core/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <system/SystemEvent.hpp>

#include <core/config/Config.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace editor {

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::Init()
{
    Game::Init();

    RC<EditorSubsystem> editor_subsystem = MakeRefCountedPtr<EditorSubsystem>(GetAppContext());
    editor_subsystem->OnProjectOpened.Bind([this](const Handle<EditorProject>& project)
                                         {
                                             m_scene = project->GetScene();
                                         })
        .Detach();

    g_engine->GetWorld()->AddSubsystem(editor_subsystem);

    m_scene = editor_subsystem->GetScene();

    // Calculate memory pool usage
    Array<SizeType> memory_usage_per_pool;
    CalculateMemoryPoolUsage(memory_usage_per_pool);

    SizeType total_memory_pool_usage = 0;
    for (SizeType i = 0; i < memory_usage_per_pool.Size(); i++)
    {
        DebugLog(LogType::Debug, "Memory Usage for pool %d : %f MiB\n", i, double(memory_usage_per_pool[i]) / 1024 / 1024);
        total_memory_pool_usage += memory_usage_per_pool[i];
    }

    DebugLog(LogType::Debug, "Total Memory Usage for pools : %f MiB\n", double(total_memory_pool_usage) / 1024 / 1024);

    DebugLog(LogType::Debug, "ShaderManager memory usage: %f MiB\n",
        double(ShaderManager::GetInstance()->CalculateMemoryUsage()) / 1024 / 1024);

    // return;

    auto test_particle_spawner = CreateObject<ParticleSpawner>(ParticleSpawnerParams {
        .texture = AssetManager::GetInstance()->Load<Texture>("textures/spark.png").GetValue().Result(),
        .origin = Vec3f(0.0f, 6.0f, 0.0f),
        .start_size = 0.2f,
        .has_physics = true });
    InitObject(test_particle_spawner);

    m_scene->GetRenderResource().GetEnvironment()->GetParticleSystem()->GetParticleSpawners().Add(test_particle_spawner);

    if (false)
    { // add test area light
        Handle<Light> light = CreateObject<Light>(
            LightType::AREA_RECT,
            Vec3f(0.0f, 1.25f, 0.0f),
            Vec3f(0.0f, 0.0f, -1.0f).Normalize(),
            Vec2f(2.0f, 2.0f),
            Color(1.0f, 0.0f, 0.0f),
            1.0f,
            1.0f);

        Handle<Texture> dummy_light_texture;

        if (auto dummy_light_texture_asset = AssetManager::GetInstance()->Load<Texture>("textures/brdfLUT.png"))
        {
            dummy_light_texture = dummy_light_texture_asset->Result();
        }

        light->SetMaterial(MaterialCache::GetInstance()->GetOrCreate(
            { .shader_definition = ShaderDefinition {
                  HYP_NAME(Forward),
                  ShaderProperties(static_mesh_vertex_attributes) },
                .bucket = Bucket::BUCKET_OPAQUE },
            {}, { { MaterialTextureKey::ALBEDO_MAP, std::move(dummy_light_texture) } }));
        AssertThrow(light->GetMaterial().IsValid());

        InitObject(light);

        Handle<Node> light_node = m_scene->GetRoot()->AddChild();
        light_node->SetName("AreaLight");

        auto area_light_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent<TransformComponent>(area_light_entity, { Transform(light->GetPosition(), Vec3f(1.0f), Quaternion::Identity()) });

        m_scene->GetEntityManager()->AddComponent<LightComponent>(area_light_entity, { light });

        light_node->SetEntity(area_light_entity);
    }

#if 1

#if 0 // point light test

    const Vec3f positions[] = {
        Vec3f(0.0f, 5.5f, 2.0f),
        Vec3f(30.0f, 5.5f, 0.0f),
        Vec3f(-30.0f, 5.5f, 0.0f),
    };

    const Color colors[] = {
        Color(1.0f, 0.0f, 0.0f),
        Color(0.0f, 1.0f, 0.0f),
        Color(0.0f, 0.0f, 1.0f)
    };

    // add pointlight (Test)
    for (uint32 i = 0; i < std::size(positions); i++){
        auto point_light = CreateObject<Light>(
            LightType::POINT,
            positions[i],
            colors[i],
            30.0f,
            50.0f
        );

        Handle<Node> point_light_node = m_scene->GetRoot()->AddChild();
        point_light_node->SetName(HYP_FORMAT("point_light_node_{}", i));

        auto point_light_entity = m_scene->GetEntityManager()->AddEntity();
        point_light_node.SetEntity(point_light_entity);
        point_light_node.SetWorldTranslation(positions[i]);

        m_scene->GetEntityManager()->AddComponent<LightComponent>(point_light_entity, LightComponent {
            point_light
        });

        m_scene->GetEntityManager()->AddComponent<ShadowMapComponent>(point_light_entity, ShadowMapComponent { });
    }

#endif

// add sun
#if 1
    auto sun = CreateObject<Light>(
        LightType::DIRECTIONAL,
        Vec3f(-0.4f, 0.8f, 0.0f).Normalize(),
        Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)),
        5.0f,
        0.0f);

    InitObject(sun);

    Handle<Node> sun_node = m_scene->GetRoot()->AddChild();
    sun_node->SetName("Sun");

    Handle<Entity> sun_entity = m_scene->GetEntityManager()->AddEntity();
    sun_node->SetEntity(sun_entity);
    sun_node->SetWorldTranslation(Vec3f { -0.4f, 0.8f, 0.0f }.Normalize());

    m_scene->GetEntityManager()->AddComponent<LightComponent>(sun_entity, LightComponent { sun });

    m_scene->GetEntityManager()->AddComponent<ShadowMapComponent>(sun_entity, ShadowMapComponent { .mode = ShadowMapFilterMode::PCF, .radius = 80.0f, .resolution = { 1024, 1024 } });
#endif

    // Add Skybox
    if (true)
    {
        Handle<Entity> skybox_entity = m_scene->GetEntityManager()->AddEntity();

        m_scene->GetEntityManager()->AddComponent<TransformComponent>(skybox_entity, TransformComponent { Transform(Vec3f::Zero(), Vec3f(1000.0f), Quaternion::Identity()) });

        m_scene->GetEntityManager()->AddComponent<SkyComponent>(skybox_entity, SkyComponent {});
        m_scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(skybox_entity, VisibilityStateComponent { VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE });
        m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(skybox_entity, BoundingBoxComponent { BoundingBox(Vec3f(-1000.0f), Vec3f(1000.0f)) });

        Handle<Node> skydome_node = m_scene->GetRoot()->AddChild();
        skydome_node->SetEntity(skybox_entity);
        skydome_node->SetName("Sky");
    }

#if 1
    // temp
    RC<AssetBatch> batch = AssetManager::GetInstance()->CreateBatch();
    batch->Add("test_model", "models/sponza/sponza.obj");
    // batch->Add("test_model", "models/pica_pica/pica_pica.obj");
    // batch->Add("test_model", "models/testbed/testbed.obj");
    // batch->Add("zombie", "models/ogrexml/dragger_Body.mesh.xml");
    // batch->Add("house", "models/house.obj");

    Handle<Entity> root_entity = GetScene()->GetEntityManager()->AddEntity();
    GetScene()->GetRoot()->SetEntity(root_entity);

    batch->OnComplete.Bind([this](AssetMap& results)
                         {

#if 1
                             Handle<Node> node = results["test_model"].ExtractAs<Node>();

                             // node->Scale(3.0f);
                             node->Scale(0.03f);
                             node->SetName("test_model");
                             node->LockTransform();

                             GetScene()->GetRoot()->AddChild(node);

#if 1
                             Handle<Entity> env_grid_entity = m_scene->GetEntityManager()->AddEntity();

                             m_scene->GetEntityManager()->AddComponent<TransformComponent>(env_grid_entity, TransformComponent {});

                             m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(env_grid_entity, BoundingBoxComponent { node->GetWorldAABB() * 1.01f, node->GetWorldAABB() * 1.01f });

                             // Add env grid component
                             m_scene->GetEntityManager()->AddComponent<EnvGridComponent>(env_grid_entity, EnvGridComponent {
                                                                                                              EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD, Vec3u { 8, 4, 8 },
                                                                                                              EnvGridMobility::STATIONARY // EnvGridMobility::FOLLOW_CAMERA_X | EnvGridMobility::FOLLOW_CAMERA_Z
                                                                                                          });

                             Handle<Node> env_grid_node = m_scene->GetRoot()->AddChild();
                             env_grid_node->SetEntity(env_grid_entity);
                             env_grid_node->SetName("EnvGrid2");
#endif

                             if (false)
                             {
                                 // testing reflection capture
                                 Handle<Entity> reflection_probe_entity = m_scene->GetEntityManager()->AddEntity();

                                 m_scene->GetEntityManager()->AddComponent<TransformComponent>(reflection_probe_entity, TransformComponent {});

                                 m_scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(reflection_probe_entity, BoundingBoxComponent { node->GetWorldAABB() * 1.01f, node->GetWorldAABB() * 1.01f });

                                 m_scene->GetEntityManager()->AddComponent<ReflectionProbeComponent>(reflection_probe_entity, ReflectionProbeComponent {});

                                 Handle<Node> reflection_probe_node = m_scene->GetRoot()->AddChild();
                                 reflection_probe_node->SetEntity(reflection_probe_entity);
                                 reflection_probe_node->SetName("ReflectionProbeTest");
                                 reflection_probe_node->SetLocalTranslation(Vec3f(0.0f, 4.0f, 0.0f));
                             }

#endif

                             if (auto& zombie_asset = results["zombie"]; zombie_asset.IsValid())
                             {
                                 Handle<Node> zombie = zombie_asset.ExtractAs<Node>();
                                 zombie->Scale(0.25f);
                                 zombie->Translate(Vec3f(0, 2.0f, -1.0f));

                                 Handle<Entity> zombie_entity = zombie->GetChild(0)->GetEntity();

                                 m_scene->GetRoot()->AddChild(zombie);

                                 if (zombie_entity.IsValid())
                                 {
                                     if (auto* mesh_component = m_scene->GetEntityManager()->TryGetComponent<MeshComponent>(zombie_entity))
                                     {
                                         mesh_component->material = mesh_component->material->Clone();
                                         mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ALBEDO, Vec4f(1.0f));
                                         mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_ROUGHNESS, 0.1f);
                                         mesh_component->material->SetParameter(Material::MaterialKey::MATERIAL_KEY_METALNESS, 0.0f);
                                         InitObject(mesh_component->material);
                                     }

                                     m_scene->GetEntityManager()->AddComponent<AudioComponent>(zombie_entity, AudioComponent { .audio_source = AssetManager::GetInstance()->Load<AudioSource>("sounds/taunt.wav")->Result(), .playback_state = { .loop_mode = AudioLoopMode::AUDIO_LOOP_MODE_ONCE, .speed = 2.0f } });
                                 }

                                 zombie->SetName("zombie");
                             }

#if 0
        // testing serialization / deserialization
        FileByteWriter byte_writer("Scene2.hyp");
        fbom::FBOMWriter writer { fbom::FBOMWriterConfig { } };
        writer.Append(*GetScene());
        auto write_err = writer.Emit(&byte_writer);
        byte_writer.Close();

        if (write_err != fbom::FBOMResult::FBOM_OK) {
            HYP_FAIL("Failed to save scene: %s", write_err.message.Data());
        }
#endif
                         })
        .Detach();

    batch->LoadAsync();
#endif

#elif 0
    HypData loaded_scene_data;
    fbom::FBOMReader reader({});
    if (auto err = reader.LoadFromFile("Scene2.hyp", loaded_scene_data))
    {
        HYP_FAIL("failed to load: %s", *err.message);
    }
    DebugLog(LogType::Debug, "static data buffer size: %u\n", reader.m_static_data_buffer.Size());

    Handle<Scene> loaded_scene = loaded_scene_data.Get<Handle<Scene>>();

    DebugLog(LogType::Debug, "Loaded scene root node : %s\n", *loaded_scene->GetRoot().GetName());

    Proc<void(const Handle<Node>&, int)> DebugPrintNode;

    DebugPrintNode = [this, &DebugPrintNode](const Handle<Node>& node, int depth)
    {
        if (!node.IsValid())
        {
            return;
        }

        String str;

        for (int i = 0; i < depth; i++)
        {
            str += "  ";
        }

        json::JSONObject node_json;
        node_json["name"] = node.GetName();

        json::JSONValue entity_json = json::JSONUndefined();
        if (auto entity = node->GetEntity())
        {
            json::JSONObject entity_json_object;
            entity_json_object["id"] = entity->GetID().Value();

            EntityManager* entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity);
            AssertThrow(entity_manager != nullptr);

            Optional<const TypeMap<ComponentID>&> all_components = entity_manager->GetAllComponents(entity);
            AssertThrow(all_components.HasValue());

            json::JSONArray components_json;

            for (const KeyValuePair<TypeID, ComponentID>& it : *all_components)
            {
                const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(it.first);

                json::JSONObject component_json;
                component_json["type"] = component_interface->GetTypeName();
                component_json["id"] = it.second;

                if (component_interface->GetTypeID() == TypeID::ForType<MeshComponent>())
                {
                    const MeshComponent* mesh_component = entity_manager->TryGetComponent<MeshComponent>(entity);
                    AssertThrow(mesh_component != nullptr);

                    json::JSONObject mesh_component_json;
                    mesh_component_json["mesh_id"] = mesh_component->mesh->GetID().Value();
                    mesh_component_json["material_id"] = mesh_component->material->GetID().Value();
                    mesh_component_json["skeleton_id"] = mesh_component->skeleton->GetID().Value();

                    json::JSONObject mesh_json;
                    mesh_json["type"] = "Mesh";
                    mesh_json["num_indices"] = mesh_component->mesh->NumIndices();

                    mesh_component_json["mesh"] = std::move(mesh_json);

                    component_json["data"] = std::move(mesh_component_json);
                }
                else if (component_interface->GetTypeID() == TypeID::ForType<TransformComponent>())
                {
                    const TransformComponent* transform_component = entity_manager->TryGetComponent<TransformComponent>(entity);
                    AssertThrow(transform_component != nullptr);

                    json::JSONObject transform_component_json;
                    transform_component_json["translation"] = HYP_FORMAT("{}", transform_component->transform.GetTranslation());
                    transform_component_json["scale"] = HYP_FORMAT("{}", transform_component->transform.GetScale());
                    transform_component_json["rotation"] = HYP_FORMAT("{}", transform_component->transform.GetRotation());

                    component_json["data"] = std::move(transform_component_json);
                }

                components_json.PushBack(std::move(component_json));
            }

            entity_json_object["components"] = std::move(components_json);

            entity_json = std::move(entity_json_object);
        }

        node_json["entity"] = std::move(entity_json);

        DebugLog(LogType::Debug, "%s\n", (str + json::JSONValue(node_json).ToString()).Data());

        for (auto& child : node->GetChildren())
        {
            DebugPrintNode(child, depth + 1);
        }
    };

    // m_scene->GetRoot()->AddChild(loaded_scene->GetRoot());

    // DebugPrintNode(m_scene->GetRoot(), 0);

    // HYP_BREAKPOINT;

    Handle<Node> root = loaded_scene->GetRoot();

    loaded_scene->SetRoot(Handle<Node>::empty);

    m_scene->GetRoot()->AddChild(root);
#endif
}

void HyperionEditor::Teardown()
{
}

void HyperionEditor::Logic(GameCounter::TickUnit delta)
{
}

void HyperionEditor::OnInputEvent(const SystemEvent& event)
{
    Game::OnInputEvent(event);

    if (event.GetType() == SystemEventType::EVENT_KEYDOWN && event.GetNormalizedKeyCode() == KeyCode::KEY_L)
    {
        Handle<Node> sun = m_scene->FindNodeByName("Sun");

        if (sun)
        {
            sun->UnlockTransform();
            sun->Rotate(Quaternion::AxisAngles(Vec3f(0.0f, 1.0f, 0.0f), 0.1f));
            HYP_LOG(Editor, Info, "Sun rotation: {}", sun->GetWorldRotation());
            sun->LockTransform();
        }
    }
}

#pragma endregion HyperionEditor

} // namespace editor
} // namespace hyperion
