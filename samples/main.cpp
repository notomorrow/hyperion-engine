
#include "system/SdlSystem.hpp"
#include "system/Debug.hpp"
#include "rendering/backend/RendererInstance.hpp"
#include "rendering/backend/RendererDescriptorSet.hpp"
#include "rendering/backend/RendererImage.hpp"
#include "rendering/backend/RendererRenderPass.hpp"
#include "rendering/backend/rt/RendererRaytracingPipeline.hpp"

#include <Engine.hpp>
#include <scene/Node.hpp>
#include <rendering/Atomics.hpp>
#include <animation/Bone.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <rendering/rt/AccelerationStructureBuilder.hpp>
#include <rendering/ProbeSystem.hpp>
#include <rendering/post_fx/SSAO.hpp>
#include <rendering/post_fx/FXAA.hpp>
#include <rendering/post_fx/Tonemap.hpp>
#include <scene/controllers/AudioController.hpp>
#include <scene/controllers/AnimationController.hpp>
#include <scene/controllers/AABBDebugController.hpp>
#include <scene/controllers/FollowCameraController.hpp>
#include <scene/controllers/paging/BasicPagingController.hpp>
#include <scene/controllers/ScriptedController.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>
#include <GameThread.hpp>
#include <Game.hpp>

#include <terrain/controllers/TerrainPagingController.hpp>

#include <rendering/vct/VoxelConeTracing.hpp>

#include <util/fs/FsUtil.hpp>

#include <scene/NodeProxy.hpp>

#include <input/InputManager.hpp>
#include <camera/FirstPersonCamera.hpp>
#include <camera/FollowCamera.hpp>

#include <builders/MeshBuilder.hpp>

#include "util/Profile.hpp"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>
#include <list>

#include "rendering/RenderEnvironment.hpp"
#include "rendering/render_components/CubemapRenderer.hpp"

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

using namespace hyperion;
using namespace hyperion::v2;


#define HYPERION_VK_TEST_VCT 1
#define HYPERION_VK_TEST_RAYTRACING 0
#define HYPERION_RUN_TESTS 1

namespace hyperion::v2 {
    
class MyGame : public Game {

public:
    Ref<Material> base_material;//hack

    Ref<Light>    m_point_light;

    MyGame()
        : Game()
    {
    }

    virtual void Init(Engine *engine, SystemWindow *window) override
    {
        Game::Init(engine, window);

        input_manager = new InputManager(window);
        input_manager->SetWindow(window);
        
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<SSAOEffect>();
        engine->GetDeferredRenderer().GetPostProcessing().AddEffect<FXAAEffect>();
    }

    virtual void OnPostInit(Engine *engine) override
    {
        scene = engine->resources.scenes.Add(new Scene(
            engine->resources.cameras.Add(new FirstPersonCamera(//FollowCamera(
                //Vector3(0, 0, 0), Vector3(0, 0.5f, -2),
                2048, 2048,//2048, 1080,
                75.0f,
                0.5f, 30000.0f
            ))
        ));
        engine->GetWorld().AddScene(scene.IncRef());
        // std::cout << (int)scene->GetClass().fields["foo"].type << "\n";

        DebugLog(LogType::Debug, "%s\n", scene->GetClass().GetName());

        base_material = engine->resources.materials.Add(new Material());
        base_material.Init();

        auto loaded_assets = engine->assets.Load<Node>(
            "models/ogrexml/dragger_Body.mesh.xml",
            "models/sponza/sponza.obj",//testbed/testbed.obj", ////sibenik/sibenik.obj",//, //, //", //
            "models/cube.obj",
            "models/material_sphere/material_sphere.obj",
            "models/grass/grass.obj"
        );

        zombie = std::move(loaded_assets[0]);
        test_model = std::move(loaded_assets[1]);
        cube_obj = std::move(loaded_assets[2]);
        material_test_obj = std::move(loaded_assets[3]);

        for (int i = 0; i < 10; i++) {
            auto sphere = engine->assets.Load<Node>("models/sphere_hq.obj");
            sphere->Scale(1.0f);
            sphere->SetName("sphere");
            // sphere->GetChild(0)->GetEntity()->SetMaterial(engine->resources.materials.Add(new Material()));
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, MathUtil::Clamp(float(i) / 10.0f, 0.05f, 0.95f));
            sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            // sphere->GetChild(0).Get()->GetEntity()->GetMaterial()->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/plastic/plasticpattern1-normal2-unity2b.png").release()));
            sphere->GetChild(0).Get()->GetEntity()->GetInitInfo().flags &= ~Entity::ComponentInitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED;
            sphere->SetLocalTranslation(Vector3(0 + (i * 6.0f), 7.0f, 0.0f));
            scene->GetRoot().AddChild(NodeProxy(sphere.release()));
        }



        // auto character_entity = engine->resources.entities.Add(new Spatial());
        // character_entity->AddController<BasicCharacterController>();
        // scene->AddSpatial(std::move(character_entity));

        // auto tmp_terrain = engine->assets.Load<Node>("models/tmp_terrain.obj");
        // tmp_terrain->Rotate(Quaternion({ 1, 0, 0 }, MathUtil::DegToRad(90.0f)));
        // tmp_terrain->Scale(500.0f);
        // scene->AddSpatial(tmp_terrain->GetChild(0)->GetEntity().IncRef());

        if (auto terrain_node = scene->GetRoot().AddChild()) {
            terrain_node.Get()->SetEntity(engine->resources.entities.Add(new Entity()));
            terrain_node.Get()->GetEntity()->AddController<TerrainPagingController>(0xBEEF, Extent3D { 256 } , Vector3(35.0f, 32.0f, 35.0f), 2.0f);
        }
        
        if (auto grass = scene->GetRoot().AddChild(NodeProxy(loaded_assets[4].release()))) {
            //grass->GetChild(0)->GetEntity()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
            grass.GetChild(0).Get()->GetEntity()->SetShader(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_VEGETATION).IncRef());
            grass.Scale(1.0f);
            grass.Translate({0, 1, 0});
        }


        material_test_obj->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.1f);
        material_test_obj->Scale(3.45f);
        material_test_obj->Translate(Vector3(0, 22, 0));
        //scene->GetRoot().AddChild(NodeProxy(material_test_obj.release()));

        // remove textures so we can manipulate the material and see our changes easier
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetEntity()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, nullptr);
        
        auto cubemap = engine->resources.textures.Add(new TextureCube(
           engine->assets.Load<Texture>(
               "textures/chapel/posx.jpg",
               "textures/chapel/negx.jpg",
               "textures/chapel/posy.jpg",
               "textures/chapel/negy.jpg",
               "textures/chapel/posz.jpg",
               "textures/chapel/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);
        cubemap.Init();

        zombie->GetChild(0).Get()->GetEntity()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        zombie->Scale(1.25f);
        zombie->Translate({0, 0, -5});
        zombie->GetChild(0).Get()->GetEntity()->GetController<AnimationController>()->Play(1.0f, LoopMode::REPEAT);
        // zombie->GetChild(0)->GetEntity()->AddController<AABBDebugController>(engine);
        // scene->GetRoot().AddChild(NodeProxy(zombie.release()));

        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({1.0f, 0.0f, 0.0f}, MathUtil::DegToRad(90.0f)));
        //zombie->GetChild(0)->GetEntity()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        auto my_light = engine->resources.lights.Add(new DirectionalLight(
            Vector3(-0.5f, 0.5f, 0.0f).Normalize(),
            Vector4::One(),
            110000.0f
        ));
        scene->GetEnvironment()->AddLight(my_light.IncRef());

        m_point_light = engine->resources.lights.Add(new PointLight(
            Vector3(0.0f, 6.0f, 0.0f),
            Vector4(1.0f, 0.3f, 0.1f, 1.0f),
            5.0f,
            25.0f
        ));

       // scene->GetEnvironment()->AddLight(m_point_light.IncRef());

        // test_model->Scale(10.0f);
        test_model->Scale(0.08f);//14.075f);

        /*auto &terrain_material = test_model->GetChild(0)->GetEntity()->GetMaterial();
        terrain_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 50.0f);
        terrain_material->SetParameter(Material::MATERIAL_KEY_PARALLAX_HEIGHT, 0.08f);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-albedo.png")));
        terrain_material->GetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP)->GetImage().SetIsSRGB(true);
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-normal-dx.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_AO_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-ao.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_PARALLAX_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Height.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Roughness.png")));
        terrain_material->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, engine->resources.textures.Add(engine->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-metallic.png")));
        test_model->Rotate(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)));*/

        if (auto test = scene->GetRoot().AddChild(NodeProxy(test_model.release()))) {
            // for (auto &child : test->GetChildren()) {
            //     if (auto &ent = child->GetEntity()) {
            //         std::cout << "Adding debug controller to  " << child->GetName() << "\n";
            //         ent->AddController<AABBDebugController>(engine);
            //     }
            // }
        }

        auto quad = engine->resources.meshes.Add(MeshBuilder::NormalizedCubeSphere(8).release());//MeshBuilder::DividedQuad(8).release());    //MeshBuilder::Quad());
        // quad->SetVertexAttributes(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes);
        auto quad_spatial = engine->resources.entities.Add(new Entity(
            std::move(quad),
            engine->shader_manager.GetShader(ShaderKey::BASIC_FORWARD).IncRef(),
            engine->resources.materials.Add(new Material()),
            RenderableAttributeSet {
                .bucket            = Bucket::BUCKET_OPAQUE,
                .shader_id         = engine->shader_manager.GetShader(ShaderKey::BASIC_FORWARD)->GetId(),
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            }
        ));
        quad_spatial.Init();
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(1.0f));//0.00f, 0.4f, 0.9f, 1.0f));
        quad_spatial->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.2f);
        quad_spatial->SetScale(Vector3(3.0f));
        quad_spatial->SetRotation(Quaternion(Vector3(1, 1, 1), MathUtil::DegToRad(-40.0f)));
        quad_spatial->SetTranslation(Vector3(0, 12.0f, 0));
        // scene->AddEntity(std::move(quad_spatial));
        
        scene->GetEnvironment()->AddRenderComponent<ShadowRenderer>(
            my_light.IncRef(),
            Vector3::Zero(),
            80.0f
        );

        scene->GetEnvironment()->AddRenderComponent<CubemapRenderer>(
            renderer::Extent2D {128, 128},
            BoundingBox { Vector(-128, -10, -128), Vector(128, 100, 128) },
            renderer::Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
        );
        scene->ForceUpdate();

#if HYPERION_VK_TEST_VCT
        scene->GetEnvironment()->AddRenderComponent<VoxelConeTracing>(
            VoxelConeTracing::Params {
                BoundingBox(-128, 128)
            }
        );
#endif


        tex1 = engine->resources.textures.Add(
            engine->assets.Load<Texture>("textures/dirt.jpg").release()
        );
        //tex1.Init();

        tex2 = engine->resources.textures.Add(
            engine->assets.Load<Texture>("textures/dummy.jpg").release()
        );
        //tex2.Init();

        cube_obj->Scale(50.0f);

        auto metal_material = engine->resources.materials.Add(new Material());
        metal_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 0.5f, 0.2f, 1.0f }));
        metal_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, tex2.IncRef());
        metal_material.Init();

        auto skybox_material = engine->resources.materials.Add(new Material());
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, cubemap.IncRef());
        skybox_material.Init();

        auto &skybox_spatial = cube_obj->GetChild(0).Get()->GetEntity();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetBucket(BUCKET_SKYBOX);
        skybox_spatial->SetShader(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX).IncRef());
        skybox_spatial->SetMeshAttributes(
            FaceCullMode::FRONT,
            false,
            false
        );

        scene->AddEntity(cube_obj->GetChild(0).Get()->GetEntity().IncRef());

        auto monkey = engine->assets.Load<Node>("models/monkey/monkey.obj");

        monkey->GetChild(0).Get()->GetEntity()->AddController<ScriptedController>(engine->assets.Load<Script>("scripts/examples/controller.hypscript"));
        monkey->GetChild(0).Get()->GetEntity()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.01f);
        monkey->Translate(Vector3(0, 10, 0));
        monkey->Scale(2.0f);
        scene->GetRoot().AddChild(NodeProxy(monkey.release()));

        for (auto &x : scene->GetRoot().GetChildren()) {
            DebugLog(LogType::Debug, "%s\n", x.Get()->GetName());
        }
    }

    virtual void Teardown(Engine *engine) override
    {
        delete input_manager;

        Game::Teardown(engine);
    }

    virtual void OnFrameBegin(Engine *engine, Frame *frame) override
    {
        scene->GetEnvironment()->RenderComponents(engine, frame);

        engine->render_state.visibility_cursor = engine->GetWorld().GetOctree().LoadPreviousVisibilityCursor();
        engine->render_state.BindScene(scene);
    }

    virtual void OnFrameEnd(Engine *engine, Frame *) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;
        ++counter;

        engine->GetWorld().Update(engine, delta);

        //scene->Update(engine, delta);

        //scene->GetEnvironment()->GetShadowRenderer(0)->SetOrigin(scene->GetCamera()->GetTranslation());


        #if 0 // bad performance on large meshes. need bvh
        //if (input_manager->IsButtonDown(MOUSE_BUTTON_LEFT) && ray_cast_timer > 1.0f) {
        //    ray_cast_timer = 0.0f;
            const auto &mouse_position = input_manager->GetMousePosition();

            const auto mouse_x = mouse_position.x.load();
            const auto mouse_y = mouse_position.y.load();

            const auto mouse_world = scene->GetCamera()->TransformScreenToWorld(
                Vector2(
                    mouse_x / static_cast<float>(input_manager->GetWindow()->width),
                    mouse_y / static_cast<float>(input_manager->GetWindow()->height)
                )
            );

            auto ray_direction = mouse_world.Normalized() * -1.0f;

            // std::cout << "ray direction: " << ray_direction << "\n";

            Ray ray{scene->GetCamera()->GetTranslation(), Vector3(ray_direction)};
            RayTestResults results;

            if (engine->GetWorld().GetOctree().TestRay(ray, results)) {
                // std::cout << "hit with aabb : " << results.Front().hitpoint << "\n";
                RayTestResults triangle_mesh_results;

                for (const auto &hit : results) {
                    // now ray test each result as triangle mesh to find exact hit point

                    if (auto lookup_result = engine->resources.entities.Lookup(Entity::ID{hit.id})) {
                        if (auto &mesh = lookup_result->GetMesh()) {
                            ray.TestTriangleList(
                                mesh->GetVertices(),
                                mesh->GetIndices(),
                                lookup_result->GetTransform(),
                                lookup_result->GetId().value,
                                triangle_mesh_results
                            );
                        }
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    const auto &mesh_hit = triangle_mesh_results.Front();

                    if (auto sphere = scene->GetRoot().Select("sphere")) {
                        sphere.SetLocalTranslation(mesh_hit.hitpoint);
                    }
                }
            }
        #endif
    }

    
    InputManager *input_manager;

    Ref<Scene> scene;
    Ref<Texture> tex1, tex2;
    std::unique_ptr<Node> test_model, zombie, cube_obj, material_test_obj;
    GameCounter::TickUnit timer{};
    GameCounter::TickUnit ray_cast_timer{};
    std::atomic_uint32_t counter{0};

};
} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;

#if 0
    Profile control([]() {
        std::queue<TestStruct> test_queue;
        for (int i = 0; i < 1000; i++) {
            test_queue.push(TestStruct{i});
        }

        // std::cout << "Size: " << test_queue.size() << "\n";

        while (!test_queue.empty()) {
            test_queue.pop();
        }
    });
    // Profile profile_1([]() {
    //     std::queue<TestStruct> test_queue;
    //     for (int i = 0; i < 500; i++) {
    //         test_queue.push(TestStruct{i});
    //     }

    //     std::queue<TestStruct> test_queue_copy = test_queue;

    //     for (int i = 0; i < 5; i++) {
    //         while (!test_queue.empty()) {
    //             std::cout << "Front: " << test_queue.front().id << "\n";
    //             test_queue.pop();
    //         }
    //     }
    //     std::cout << "Size: " << test_queue_copy.size() << "\n";

    //     while (!test_queue_copy.empty()) {
    //         test_queue_copy.pop();
    //     }
    // });

    // Profile profile_2([]() {
    //     hyperion::Queue<TestStruct> test_queue;
    //     test_queue.Reserve(1000);
    //     for (int i = 0; i < 500; i++) {
    //         test_queue.Push(TestStruct{i});
    //     }

    //     hyperion::Queue<TestStruct> test_queue_copy = test_queue;

    //     for (int i = 0; i < 5; i++) {
    //         while (!test_queue.Empty()) {
    //             std::cout << "Front: " << test_queue.Front().id << "\n";
    //             test_queue.Pop();
    //         }
    //     }

    //     std::cout << "Size: " << test_queue_copy.Size() << "\n";

    //     while (!test_queue_copy.Empty()) {
    //         test_queue_copy.Pop();
    //     }
    // });


    
    Profile profile_1([]() {
        std::vector<TestStruct> test_queue;
        // test_queue.reserve(1000);
        for (int i = 0; i < 256; i++) {
            test_queue.push_back(TestStruct{i});
        }

        std::vector<TestStruct> test_queue_copy = test_queue;

        // for (int i = 0; i < 5; i++) {
            while (!test_queue.empty()) {
                // std::cout << "Back: " << test_queue.front().id << "\n";
                test_queue.pop_back();
            }
        // }
        // std::cout << "Size: " << test_queue_copy.size() << "\n";

        while (!test_queue_copy.empty()) {
            test_queue_copy.pop_back();
        }
    });

    Profile profile_2([]() {
        hyperion::DynArray<TestStruct> test_queue;
        // test_queue.Reserve(1000);
        for (int i = 0; i < 256; i++) {
            test_queue.PushBack(TestStruct{i});
        }
        // std::cout << "Size after pushing: " << test_queue.Size() << "\n";

        hyperion::DynArray<TestStruct> test_queue_copy = test_queue;

        // for (int i = 0; i < 5; i++) {
            while (!test_queue.Empty()) {
                // std::cout << "Back: " << test_queue.Front().id << "\n";
                test_queue.PopBack();
            }
        // }
        // std::cout << "back: " << test_queue_copy.Back().id << "\n";


        while (!test_queue_copy.Empty()) {
            test_queue_copy.PopBack();
        }
    });

    // profile_2.Run(10, 10);

    auto results = Profile::RunInterleved({ control, profile_1, profile_2 }, 5, 10, 1000);

    HYP_BREAKPOINT;
#endif
    
    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 1024);//2048, 1080);
    system.SetCurrentWindow(window);

    SystemEvent event;

    auto *engine = new Engine(system, "My app");

    engine->assets.SetBasePath(FileSystem::Join(HYP_ROOT_DIR, "..", "res"));

    auto *my_game = new MyGame;
;
    engine->Initialize();

    Device *device = engine->GetInstance()->GetDevice();

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_VEGETATION,
        engine->resources.shaders.Add(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vegetation.vert.spv")).Read(),
                        {.name = "vegetation vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::DEBUG_AABB,
        engine->resources.shaders.Add(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.vert.spv")).Read(),
                        {.name = "aabb vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/aabb.frag.spv")).Read(),
                        {.name = "aabb frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::BASIC_FORWARD,
        engine->resources.shaders.Add(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/forward_frag.spv")).Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    engine->shader_manager.SetShader(
        ShaderKey::TERRAIN,
        engine->resources.shaders.Add(new Shader(
            std::vector<SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/Terrain.frag.spv")).Read(),
                        {.name = "Terrain frag"}
                    }
                }
            }
        ))
    );
    
    Frame *frame = nullptr;
    
    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine->GetInstance()->GetFrameHandler()->NumFrames());

    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);
        AssertThrow(cmd_buffer->Create(engine->GetInstance()->GetDevice(), engine->GetInstance()->GetGraphicsQueue().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
    }

    
    // engine->shader_manager.SetShader(
    //     ShaderManager::Key::STENCIL_OUTLINE,
    //     engine->resources.shaders.Add(new Shader(
    //         std::vector<SubShader>{
    //             {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.vert.spv")).Read()}},
    //             {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/outline.frag.spv")).Read()}}
    //         }
    //     ))
    // );


    {
        engine->shader_manager.SetShader(
            ShaderManager::Key::BASIC_SKYBOX,
            engine->resources.shaders.Add(new Shader(
                std::vector<SubShader>{
                    {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_vert.spv")).Read()}},
                    {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/skybox_frag.spv")).Read()}}
                }
            ))
        );

        /*auto pipeline = std::make_unique<RendererInstance>(
            engine->shader_manager.GetShader(ShaderManager::Key::BASIC_SKYBOX).IncRef(),
            engine->GetRenderListContainer().Get(BUCKET_SKYBOX).GetRenderPass().IncRef(),
            renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
            Bucket::BUCKET_SKYBOX
        );
        pipeline->SetFaceCullMode(FaceCullMode::FRONT);
        pipeline->SetDepthTest(false);
        pipeline->SetDepthWrite(false);
        
        engine->AddRendererInstance(std::move(pipeline));*/
    }
    
    {
        auto pipeline = std::make_unique<RendererInstance>(
            engine->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD).IncRef(),
            engine->GetRenderListContainer().Get(BUCKET_TRANSLUCENT).GetRenderPass().IncRef(),
            RenderableAttributeSet{
                .bucket            = Bucket::BUCKET_TRANSLUCENT,
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            }
        );
        pipeline->SetBlendEnabled(true);
        
        engine->AddRendererInstance(std::move(pipeline));
    }
    

    my_game->Init(engine, window);

#if HYPERION_VK_TEST_RAYTRACING
    auto rt_shader = std::make_unique<ShaderProgram>();
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rgen.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rmiss.spv")).Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/rt/test.rchit.spv")).Read()
    });

    auto rt = std::make_unique<RaytracingPipeline>(std::move(rt_shader));

    my_game->material_test_obj->GetChild(0).Get()->GetEntity()->SetTransform({{ 0, 7, 0 }});


    ProbeGrid probe_system({
        .aabb = {{-20.0f, -5.0f, -20.0f}, {20.0f, 5.0f, 20.0f}}
    });
    probe_system.Init(engine);

    auto my_tlas = std::make_unique<Tlas>();

    my_tlas->AddBlas(engine->resources.blas.Add(new Blas(
        engine->resources.meshes.IncRef(my_game->material_test_obj->GetChild(0).Get()->GetEntity()->GetMesh()),
        my_game->material_test_obj->GetChild(0).Get()->GetEntity()->GetTransform()
    )));
    
    my_tlas->AddBlas(engine->resources.blas.Add(new Blas(
        engine->resources.meshes.IncRef(my_game->cube_obj->GetChild(0).Get()->GetEntity()->GetMesh()),
        my_game->cube_obj->GetChild(0).Get()->GetEntity()->GetTransform()
    )));

    my_tlas->Init(engine);
    
    Image *rt_image_storage = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_image_storage_view;

    auto *rt_descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
    rt_descriptor_set->AddDescriptor<TlasDescriptor>(0)
        ->SetSubDescriptor({.acceleration_structure = &my_tlas->Get()});
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(1)
        ->SetSubDescriptor({.image_view = &rt_image_storage_view});
    
    auto rt_storage_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(3);
    rt_storage_buffer->SetSubDescriptor({.buffer = my_tlas->Get().GetMeshDescriptionsBuffer()});

    HYPERION_ASSERT_RESULT(rt_image_storage->Create(
        device,
        engine->GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_image_storage_view.Create(engine->GetDevice(), rt_image_storage));
#endif

    engine->Compile();

#if HYPERION_VK_TEST_RAYTRACING
    HYPERION_ASSERT_RESULT(rt->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
#endif

#if HYPERION_RUN_TESTS
    AssertThrow(test::GlobalTestManager::PrintReport(test::GlobalTestManager::Instance()->RunAll()));
#endif

    engine->game_thread.Start(engine, my_game, window);

    bool running = true;

    float tmp_render_timer = 0.0f;

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    while (running) {
        while (SystemSDL::PollEvent(&event)) {
            my_game->input_manager->CheckEvent(&event);
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                case SystemEventType::EVENT_MOUSESCROLL:
                {
                    if (my_game->scene != nullptr) {
                        int wheel_x, wheel_y;
                        event.GetMouseWheel(&wheel_x, &wheel_y);

                        my_game->scene->GetCamera()->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_SCROLL,
                            .scroll_data = {
                                .wheel_x = wheel_x,
                                .wheel_y = wheel_y
                            }
                        });
                    }

                    break;
                }
                case SystemEventType::EVENT_MOUSEMOTION:
                {
                    const auto &mouse_position = my_game->input_manager->GetMousePosition();

                    int mouse_x = mouse_position.x.load(),
                        mouse_y = mouse_position.y.load();

                    float mx, my;

                    int window_width, window_height;
                    my_game->input_manager->GetWindow()->GetSize(&window_width, &window_height);

                    mx = (static_cast<float>(mouse_x) - static_cast<float>(window_width) * 0.5f) / (static_cast<float>(window_width));
                    my = (static_cast<float>(mouse_y) - static_cast<float>(window_height) * 0.5f) / (static_cast<float>(window_height));
                    
                    if (my_game->scene != nullptr) {
                        my_game->scene->GetCamera()->PushCommand(CameraCommand {
                            .command = CameraCommand::CAMERA_COMMAND_MAG,
                            .mag_data = {
                                .mouse_x = mouse_x,
                                .mouse_y = mouse_y,
                                .mx      = mx,
                                .my      = my
                            }
                        });
                    }

                    break;
                }
                default:
                    break;
            }
        }

        if (my_game->input_manager->IsKeyDown(KEY_W)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_FORWARD,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_S)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_BACKWARD,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_A)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_LEFT,
                        .amount        = 1.0f
                    }
                });
            }
        }
        if (my_game->input_manager->IsKeyDown(KEY_D)) {
            if (my_game->scene != nullptr) {
                my_game->scene->GetCamera()->PushCommand(CameraCommand {
                    .command = CameraCommand::CAMERA_COMMAND_MOVEMENT,
                    .movement_data = {
                        .movement_type = CameraCommand::CAMERA_MOVEMENT_RIGHT,
                        .amount        = 1.0f
                    }
                });
            }
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 250) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / static_cast<float>(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }

        HYPERION_ASSERT_RESULT(engine->GetInstance()->GetFrameHandler()->PrepareFrame(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetSwapchain()
        ));

        frame = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        auto *command_buffer = frame->GetCommandBuffer();
        const auto frame_index = engine->GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        engine->PreFrameUpdate(frame);

        /* === rendering === */
        HYPERION_ASSERT_RESULT(frame->BeginCapture(engine->GetInstance()->GetDevice()));

        my_game->OnFrameBegin(engine, frame);

#if HYPERION_VK_TEST_RAYTRACING
        rt->Bind(frame->GetCommandBuffer());
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {
                {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
                {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                {.offsets = {0, 0}}
            }
        );
        engine->GetInstance()->GetDescriptorPool().Bind(
            engine->GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {{
                .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                .count = 1
            }}
        );
        rt->TraceRays(engine->GetDevice(), frame->GetCommandBuffer(), rt_image_storage->GetExtent());
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );

        
        probe_system.RenderProbes(engine, frame->GetCommandBuffer());
        probe_system.ComputeIrradiance(engine, frame->GetCommandBuffer());
#endif

        engine->RenderDeferred(frame);
        

        engine->RenderFinalPass(frame);

        HYPERION_ASSERT_RESULT(frame->EndCapture(engine->GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(frame->Submit(&engine->GetInstance()->GetGraphicsQueue()));
        
        my_game->OnFrameEnd(engine, frame);

        engine->GetInstance()->GetFrameHandler()->PresentFrame(&engine->GetInstance()->GetGraphicsQueue(), engine->GetInstance()->GetSwapchain());
        engine->GetInstance()->GetFrameHandler()->NextFrame();

    }

    AssertThrow(engine->GetInstance()->GetDevice()->Wait());

    for (size_t i = 0; i < per_frame_data.NumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine->GetInstance()->GetDevice(), engine->GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();
    
#if HYPERION_VK_TEST_RAYTRACING

    rt_image_storage->Destroy(engine->GetDevice());
    rt_image_storage_view.Destroy(engine->GetDevice());
    //tlas->Destroy(engine->GetInstance());
    rt->Destroy(engine->GetDevice());
#endif


    engine->task_system.Stop();

    engine->m_running = false;

    HYP_FLUSH_RENDER_QUEUE(engine);

    engine->game_thread.Join();

    delete my_game;

    delete engine;
    delete window;

    return 0;
}
