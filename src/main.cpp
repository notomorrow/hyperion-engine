
#include "system/sdl_system.h"
#include "system/debug.h"
#include "rendering/backend/renderer_instance.h"
#include "rendering/backend/renderer_descriptor_set.h"
#include "rendering/backend/renderer_image.h"
#include "rendering/backend/renderer_render_pass.h"
#include "rendering/backend/rt/renderer_raytracing_pipeline.h"

#include <engine.h>
#include <scene/node.h>
#include <rendering/atomics.h>
#include <animation/bone.h>
#include <asset/model_loaders/obj_model_loader.h>
#include <rendering/rt/acceleration_structure_builder.h>
#include <rendering/probe_system.h>
#include <game_thread.h>
#include <game.h>

#include <rendering/vct/vct.h>

#include "input_manager.h"
#include "asset/asset_manager.h"
#include <camera/fps_camera.h>

#include "util/profile.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>

using namespace hyperion;


#define HYPERION_VK_TEST_IMAGE_STORE 1
#define HYPERION_VK_TEST_ATOMICS     1
#define HYPERION_VK_TEST_VISUALIZE_OCTREE 0
#define HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE 0
#define HYPERION_VK_TEST_VCT 1
#define HYPERION_VK_TEST_RAYTRACING 0
#define HYPERION_RUN_TESTS 1

namespace hyperion::v2 {
class MyGame : public v2::Game {

public:
    MyGame()
        : Game()
    {
    }

    virtual void Init(Engine *engine, SystemWindow *window) override
    {
        using namespace v2;

        Game::Init(engine, window);

        input_manager = new InputManager(window);
        input_manager->SetWindow(window);

        scene = engine->resources.scenes.Add(std::make_unique<v2::Scene>(
            std::make_unique<FpsCamera>(
                input_manager,
                window,
                1024, 768,
                70.0f,
                0.05f, 250.0f
            )
        ));


        auto base_path = AssetManager::GetInstance()->GetRootDir();

        auto loaded_assets = engine->assets.Load<Node>(
            base_path + "models/ogrexml/dragger_Body.mesh.xml",
            base_path + "models/sponza/sponza.obj",
            base_path + "models/cube.obj",
            base_path + "models/material_sphere/material_sphere.obj"
        );

        zombie = std::move(loaded_assets[0]);
        test_model = std::move(loaded_assets[1]);
        cube_obj = std::move(loaded_assets[2]);
        material_test_obj = std::move(loaded_assets[3]);

        // remove textures so we can manipulate the material and see our changes easier
        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, nullptr);
        //material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_NORMAL_MAP, nullptr);
        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ROUGHNESS_MAP, nullptr);
        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_METALNESS_MAP, nullptr);
        
        auto cubemap = engine->resources.textures.Add(std::make_unique<TextureCube>(
           engine->assets.Load<Texture>(
               base_path + "textures/Lycksele3/posx.jpg",
               base_path + "textures/Lycksele3/negx.jpg",
               base_path + "textures/Lycksele3/posy.jpg",
               base_path + "textures/Lycksele3/negy.jpg",
               base_path + "textures/Lycksele3/posz.jpg",
               base_path + "textures/Lycksele3/negz.jpg"
            )
        ));
        cubemap->GetImage().SetIsSRGB(true);

        zombie->GetChild(0)->GetSpatial()->SetBucket(Bucket::BUCKET_TRANSLUCENT);
        zombie->Scale(0.1f);
        zombie->Translate({0, 0, -5});
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({1.0f, 0.0f, 0.0f}, MathUtil::DegToRad(90.0f)));
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();


        scene->SetEnvironmentTexture(0, cubemap.IncRef());
        test_model->Translate({0, 0, 5});
        test_model->Scale(0.025f);
        
        tex1 = engine->resources.textures.Add(
            engine->assets.Load<Texture>(base_path + "textures/dirt.jpg")
        );

        tex2 = engine->resources.textures.Add(
            engine->assets.Load<Texture>(base_path + "textures/dummy.jpg")
        );

        auto metal_material = engine->resources.materials.Add(std::make_unique<v2::Material>());
        metal_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 0.5f, 0.2f, 1.0f }));
        metal_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, tex2.IncRef());
        metal_material.Init();

        auto skybox_material = engine->resources.materials.Add(std::make_unique<v2::Material>());
        skybox_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
        skybox_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, cubemap.IncRef());
        skybox_material.Init();

        auto *skybox_spatial = cube_obj->GetChild(0)->GetSpatial();
        skybox_spatial->SetMaterial(std::move(skybox_material));
        skybox_spatial->SetBucket(v2::Bucket::BUCKET_SKYBOX);
        skybox_spatial->SetShader(engine->shader_manager.GetShader(v2::ShaderManager::Key::BASIC_SKYBOX).IncRef());
    }

    virtual void Teardown(Engine *engine) override
    {
        delete input_manager;

        Game::Teardown(engine);
    }

    virtual void OnFrameBegin(Engine *engine) override
    {
        engine->render_state.BindScene(scene);
    }

    virtual void OnFrameEnd(Engine *engine) override
    {
        engine->render_state.UnbindScene();
    }

    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) override
    {
        timer += delta;
        ++counter;

        scene->Update(engine, delta);
    
        test_model->Update(engine, delta);
        
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalTranslation(Vector3(0, std::sin(timer * 0.3f), 0));
       // zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("thigh.L")->SetLocalRotation(Quaternion({0, 1, 0}, timer * 0.35f));
        //zombie->GetChild(0)->GetSpatial()->GetSkeleton()->GetRootBone()->UpdateWorldTransform();

        if (uint32_t(timer) % 2 == 0) {
            zombie->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, tex1.IncRef());
        } else {
            zombie->GetChild(0)->GetSpatial()->GetMaterial()->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, tex2.IncRef());
        }
        
        material_test_obj->SetLocalScale(2.45f);
        //material_test_obj->SetLocalRotation(Quaternion({0, 1, 0}, timer * 0.25f));
        material_test_obj->SetLocalTranslation({7, 1.25f, 0});
        //material_test_obj->SetLocalTranslation(Vector3(std::sin(timer * 0.5f) * 5.0f, 0, std::cos(timer * 0.5f) * 5.0f) + Vector3(7, 3, 0));

        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(
            std::sin(timer) * 0.5f + 0.5f,
            std::cos(timer) * 0.5f + 0.5f,
            ((std::sin(timer) * 0.5f + 0.5f) + (std::cos(timer) * 0.5f + 0.5f)) * 0.5f,
            1.0f
        ));

        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, std::sin(timer) * 0.5f + 0.5f);
        material_test_obj->GetChild(0)->GetSpatial()->GetMaterial()->SetParameter(Material::MATERIAL_KEY_METALNESS, std::cos(timer) * 0.5f + 0.5f);
        material_test_obj->Update(engine, delta);

        zombie->Update(engine, delta);
        
        cube_obj->SetLocalTranslation(scene->GetCamera()->GetTranslation());
        cube_obj->Update(engine, delta);

        engine->GetOctree().CalculateVisibility(scene.ptr);
    }

    
    InputManager *input_manager;

    Ref<Scene> scene;
    Ref<Texture> tex1, tex2;
    std::unique_ptr<Node> test_model, zombie, cube_obj, material_test_obj;
    double timer = 0.0;
    int counter = 0;

};
} // namespace hyperion::v2

int main()
{
    using namespace hyperion::renderer;
    
    std::string base_path = HYP_ROOT_DIR;
    AssetManager::GetInstance()->SetRootDir(base_path + "/res/");

    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    v2::Engine engine(system, "My app");

    v2::MyGame my_game;


    auto texture = engine.resources.textures.Add(
        engine.assets.Load<v2::Texture>(base_path + "/res/textures/dirt.jpg")
    );

    auto texture2 = engine.resources.textures.Add(
        engine.assets.Load<v2::Texture>(base_path + "/res/textures/dummy.jpg")
    );

#if HYPERION_VK_TEST_IMAGE_STORE
    renderer::Image *image_storage = new renderer::StorageImage(
        Extent3D{512, 512, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView image_storage_view;
#endif


    engine.Initialize();
    
    auto mat1 = engine.resources.materials.Add(std::make_unique<v2::Material>());
    mat1->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 0.6f }));
    mat1->SetTexture(v2::Material::MATERIAL_TEXTURE_ALBEDO_MAP, texture.IncRef());
    mat1.Init();

    Device *device = engine.GetInstance()->GetDevice();

    auto *descriptor_set_globals = engine.GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);

#if HYPERION_VK_TEST_IMAGE_STORE
    descriptor_set_globals
        ->AddDescriptor<StorageImageDescriptor>(16)
        ->AddSubDescriptor({ .image_view = &image_storage_view });

    HYPERION_ASSERT_RESULT(image_storage->Create(
        device,
        engine.GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));

    HYPERION_ASSERT_RESULT(image_storage_view.Create(device, image_storage));
#endif

#if HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE
    v2::SparseVoxelOctree svo;
    svo.Init(&engine);
#endif

    engine.PrepareSwapchain();
    
    engine.shader_manager.SetShader(
        v2::ShaderManager::Key::BASIC_FORWARD,
        engine.resources.shaders.Add(std::make_unique<v2::Shader>(
            std::vector<v2::SubShader>{
                {
                    ShaderModule::Type::VERTEX, {
                        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read(),
                        {.name = "main vert"}
                    }
                },
                {
                    ShaderModule::Type::FRAGMENT, {
                        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/forward_frag.spv").Read(),
                        {.name = "forward frag"}
                    }
                }
            }
        ))
    );

    float timer = 0.0;

    Frame *frame = nullptr;

#if HYPERION_VK_TEST_IMAGE_STORE
    /* Compute */
    auto compute_pipeline = engine.resources.compute_pipelines.Add(
        std::make_unique<v2::ComputePipeline>(
            engine.resources.shaders.Add(std::make_unique<v2::Shader>(
                std::vector<v2::SubShader>{
                    { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/imagestore.comp.spv").Read()}}
                }
            ))
        )
    );

    compute_pipeline.Init();

    auto compute_command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);

    SemaphoreChain compute_semaphore_chain({}, { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    AssertThrow(compute_semaphore_chain.Create(engine.GetInstance()->GetDevice()));

    HYPERION_ASSERT_RESULT(compute_command_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetComputeCommandPool()));
    
    for (size_t i = 0; i < engine.GetInstance()->GetFrameHandler()->NumFrames(); i++) {
        /* Wait for compute to finish */
        compute_semaphore_chain.SignalsTo(engine.GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores());
    }
#endif
    

    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine.GetInstance()->GetFrameHandler()->NumFrames());

    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);
        AssertThrow(cmd_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsQueue().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
    }
    
    /*{
        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            engine.shader_manager.GetShader(v2::ShaderManager::Key::BASIC_FORWARD).IncRef(),
            engine.GetRenderListContainer()[v2::BUCKET_OPAQUE].render_pass.IncRef(),
            VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton,
            v2::Bucket::BUCKET_OPAQUE
        );
        
        engine.AddGraphicsPipeline(std::move(pipeline));
    }*/


    {
        engine.shader_manager.SetShader(
            v2::ShaderManager::Key::BASIC_SKYBOX,
            engine.resources.shaders.Add(std::make_unique<v2::Shader>(
                std::vector<v2::SubShader>{
                    {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/skybox_vert.spv").Read()}},
                    {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/skybox_frag.spv").Read()}}
                }
            ))
        );

        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            engine.shader_manager.GetShader(v2::ShaderManager::Key::BASIC_SKYBOX).IncRef(),
            engine.GetRenderListContainer().Get(v2::BUCKET_SKYBOX).render_pass.IncRef(),
            VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton,
            v2::Bucket::BUCKET_SKYBOX
        );
        pipeline->SetFaceCullMode(FaceCullMode::FRONT);
        pipeline->SetDepthTest(false);
        pipeline->SetDepthWrite(false);
        
        engine.AddGraphicsPipeline(std::move(pipeline));
    }
    
    {
        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            engine.shader_manager.GetShader(v2::ShaderManager::Key::BASIC_FORWARD).IncRef(),
            engine.GetRenderListContainer().Get(v2::BUCKET_TRANSLUCENT).render_pass.IncRef(),
            VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton,
            v2::Bucket::BUCKET_TRANSLUCENT
        );
        pipeline->SetBlendEnabled(true);
        
        engine.AddGraphicsPipeline(std::move(pipeline));
    }
    

    my_game.Init(&engine, window);

#if HYPERION_VK_TEST_VCT
    v2::VoxelConeTracing vct({
        /* scene bounds for vct to capture */
        .aabb = BoundingBox(Vector3(-128), Vector3(128))
    });

    vct.Init(&engine);
#endif


#if HYPERION_VK_TEST_RAYTRACING
    auto rt_shader = std::make_unique<ShaderProgram>();
    rt_shader->AttachShader(engine.GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/test.rgen.spv").Read()
    });
    rt_shader->AttachShader(engine.GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/test.rmiss.spv").Read()
    });
    rt_shader->AttachShader(engine.GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/test.rchit.spv").Read()
    });

    auto rt = std::make_unique<RaytracingPipeline>(std::move(rt_shader));

    my_game.material_test_obj->GetChild(0)->GetSpatial()->SetTransform({{ 0, 5, 0 }});


    v2::ProbeGrid probe_system({
        .aabb = {{-20.0f, -5.0f, -20.0f}, {20.0f, 5.0f, 20.0f}}
    });
    probe_system.Init(&engine);

    auto my_tlas = std::make_unique<v2::Tlas>();

    my_tlas->AddBlas(engine.resources.blas.Add(std::make_unique<v2::Blas>(
        engine.resources.meshes.IncRef(my_game.material_test_obj->GetChild(0)->GetSpatial()->GetMesh()),
        my_game.material_test_obj->GetChild(0)->GetSpatial()->GetTransform()
    )));
    
    my_tlas->AddBlas(engine.resources.blas.Add(std::make_unique<v2::Blas>(
        engine.resources.meshes.IncRef(my_game.cube_obj->GetChild(0)->GetSpatial()->GetMesh()),
        my_game.cube_obj->GetChild(0)->GetSpatial()->GetTransform()
    )));

    my_tlas->Init(&engine);
    
    Image *rt_image_storage = new StorageImage(
        Extent3D{1024, 1024, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView rt_image_storage_view;

    auto *rt_descriptor_set = engine.GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
    rt_descriptor_set->AddDescriptor<TlasDescriptor>(0)
        ->AddSubDescriptor({.acceleration_structure = &my_tlas->Get()});
    rt_descriptor_set->AddDescriptor<StorageImageDescriptor>(1)
        ->AddSubDescriptor({.image_view = &rt_image_storage_view});
    
    auto rt_storage_buffer = rt_descriptor_set->AddDescriptor<StorageBufferDescriptor>(3);
    rt_storage_buffer->AddSubDescriptor({.buffer = my_tlas->Get().GetMeshDescriptionsBuffer()});

    HYPERION_ASSERT_RESULT(rt_image_storage->Create(
        device,
        engine.GetInstance(),
        GPUMemory::ResourceState::UNORDERED_ACCESS
    ));
    HYPERION_ASSERT_RESULT(rt_image_storage_view.Create(engine.GetDevice(), rt_image_storage));
#endif

    engine.Compile();

#if HYPERION_VK_TEST_RAYTRACING
    HYPERION_ASSERT_RESULT(rt->Create(engine.GetDevice(), &engine.GetInstance()->GetDescriptorPool()));
#endif

#if HYPERION_RUN_TESTS
    AssertThrow(test::GlobalTestManager::PrintReport(test::GlobalTestManager::Instance()->RunAll()));
#endif


#if HYPERION_VK_TEST_SPARSE_VOXEL_OCTREE
    svo.Build(&engine);
#endif

#if HYPERION_VK_TEST_IMAGE_STORE
    auto compute_fc = std::make_unique<Fence>(true);
    HYPERION_ASSERT_RESULT(compute_fc->Create(engine.GetDevice()));
#endif

    engine.game_thread.Start(&engine, &my_game, window);

    bool running = true;

    while (running) {
        while (SystemSDL::PollEvent(&event)) {
            my_game.input_manager->CheckEvent(&event);
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                default:
                    break;
            }
        }


        HYPERION_ASSERT_RESULT(engine.GetInstance()->GetFrameHandler()->PrepareFrame(
            engine.GetInstance()->GetDevice(),
            engine.GetInstance()->GetSwapchain()
        ));


        frame = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        auto *command_buffer = frame->GetCommandBuffer();
        const uint32_t frame_index = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

        if (auto num_enqueued = engine.render_scheduler.NumEnqueued()) {
            engine.render_scheduler.Flush([command_buffer, frame_index](auto &fn) {
                HYPERION_ASSERT_RESULT(fn(command_buffer, frame_index));
            });

            /*DebugLog(
                LogType::Debug,
                "[Renderer] Execute %lu enqueued tasks\n",
                num_enqueued
            );*/
        }

#if HYPERION_VK_TEST_IMAGE_STORE
        compute_pipeline->GetPipeline()->push_constants = {
            .counter = {
                .x = static_cast<uint32_t>(std::sin(timer) * 20.0f),
                .y = static_cast<uint32_t>(std::cos(timer) * 20.0f)
            }
        };

        /* Compute */
        HYPERION_ASSERT_RESULT(compute_fc->WaitForGpu(engine.GetDevice()));
        HYPERION_ASSERT_RESULT(compute_fc->Reset(engine.GetDevice()));
        
        HYPERION_ASSERT_RESULT(compute_command_buffer->Reset(engine.GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(compute_command_buffer->Record(engine.GetInstance()->GetDevice(), nullptr, [&](CommandBuffer *cmd) {
            compute_pipeline->GetPipeline()->Bind(cmd);

            engine.GetInstance()->GetDescriptorPool().Bind(
                engine.GetInstance()->GetDevice(),
                cmd,
                compute_pipeline->GetPipeline(),
                {{
                    .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
                    .count = 1
                }}
            );

            compute_pipeline->GetPipeline()->Dispatch(cmd, {8, 8, 1});

            HYPERION_RETURN_OK;
        }));

        HYPERION_ASSERT_RESULT(compute_command_buffer->SubmitPrimary(
            engine.GetInstance()->GetComputeQueue().queue,
            compute_fc.get(),
            &compute_semaphore_chain
        ));
#endif

        


        /* Only update sets that are double - buffered so we don't
         * end up updating data that is in use by the gpu
         */
        engine.UpdateBuffersAndDescriptors(frame_index);

        engine.ResetRenderState();

        /* === rendering === */
        HYPERION_ASSERT_RESULT(frame->BeginCapture(engine.GetInstance()->GetDevice()));

        my_game.OnFrameBegin(&engine);

#if HYPERION_VK_TEST_RAYTRACING
        rt->Bind(frame->GetCommandBuffer());
        engine.GetInstance()->GetDescriptorPool().Bind(
            engine.GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {
                {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
                {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                {.offsets = {0}}
            }
        );
        engine.GetInstance()->GetDescriptorPool().Bind(
            engine.GetDevice(),
            frame->GetCommandBuffer(),
            rt.get(),
            {{
                .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                .count = 1
            }}
        );
        rt->TraceRays(engine.GetDevice(), frame->GetCommandBuffer(), rt_image_storage->GetExtent());
        rt_image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );

        
        probe_system.RenderProbes(&engine, frame->GetCommandBuffer());
        probe_system.ComputeIrradiance(&engine, frame->GetCommandBuffer());
#endif

#if HYPERION_VK_TEST_VCT
        vct.RenderVoxels(&engine, frame->GetCommandBuffer(), frame_index);
#endif

            
        /* Shadow cam test */

        Matrix4 shadow_view;
        MatrixUtil::ToLookAt(shadow_view, {5, 5, 5}, {0, 0, 0}, {0, 1, 0});
        Matrix4 shadow_proj;
        MatrixUtil::ToOrtho(shadow_proj, -50, 50, -50, 50, -50, 50);

        engine.shader_globals->scenes.Set(1, {
            .view       = shadow_view,
            .projection = shadow_proj,
            .camera_position = {5, 5, 5, 1},
            .light_direction = Vector4(Vector3(-0.5f, 0.5f, 0.0f).Normalize(), 1.0f)
        });

        
        engine.RenderDeferred(frame->GetCommandBuffer(), frame_index);

        

#if HYPERION_VK_TEST_IMAGE_STORE
        image_storage->GetGPUImage()->InsertBarrier(
            frame->GetCommandBuffer(),
            GPUMemory::ResourceState::UNORDERED_ACCESS
        );
#endif

        engine.RenderSwapchain(frame->GetCommandBuffer());

        HYPERION_ASSERT_RESULT(frame->EndCapture(engine.GetInstance()->GetDevice()));
        HYPERION_ASSERT_RESULT(frame->Submit(&engine.GetInstance()->GetGraphicsQueue()));
        
        my_game.OnFrameEnd(&engine);

        engine.GetInstance()->GetFrameHandler()->PresentFrame(&engine.GetInstance()->GetGraphicsQueue(), engine.GetInstance()->GetSwapchain());
        engine.GetInstance()->GetFrameHandler()->NextFrame();

    }

    AssertThrow(engine.GetInstance()->GetDevice()->Wait());

    v2::FullScreenPass::full_screen_quad.reset();// have to do this here for now or else buffer does not get cleared before device is deleted

#if HYPERION_VK_TEST_IMAGE_STORE
    HYPERION_ASSERT_RESULT(image_storage_view.Destroy(device));
    HYPERION_ASSERT_RESULT(image_storage->Destroy(device));

    delete image_storage;

#endif

    for (size_t i = 0; i < per_frame_data.NumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();

#if HYPERION_VK_TEST_IMAGE_STORE
    compute_command_buffer->Destroy(device, engine.GetInstance()->GetComputeCommandPool());
    compute_semaphore_chain.Destroy(engine.GetInstance()->GetDevice());

    compute_fc->Destroy(engine.GetDevice());
#endif

#if HYPERION_VK_TEST_RAYTRACING

    rt_image_storage->Destroy(engine.GetDevice());
    rt_image_storage_view.Destroy(engine.GetDevice());
    //tlas->Destroy(engine.GetInstance());
    rt->Destroy(engine.GetDevice());
#endif

    engine.Destroy();


    delete window;

    return 0;
}
