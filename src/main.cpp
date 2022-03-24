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


#define HYPERION_VK_TEST_CUBEMAP 1
#define HYPERION_VK_TEST_MIPMAP 0
#define HYPERION_VK_TEST_IMAGE_STORE 0

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
        Image::InternalFormat(cubemap_faces[0]->GetInternalFormat()),
        Image::FilterMode::TEXTURE_FILTER_LINEAR,
        bytes
    );

    ImageView test_image_view;
    Sampler test_sampler(
        Image::FilterMode::TEXTURE_FILTER_LINEAR,
        Image::WrapMode::TEXTURE_WRAP_REPEAT
    );


    delete[] bytes;
#elif HYPERION_VK_TEST_MIPMAP
    // test image
    auto texture = AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dummy.jpg");
    Image *image = new TextureImage2D(
        texture->GetWidth(),
        texture->GetHeight(),
        Image::InternalFormat(texture->GetInternalFormat()),
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        texture->GetBytes()
    );

    ImageView test_image_view;
    Sampler test_sampler(
        Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_REPEAT
    );

#endif

#if HYPERION_VK_TEST_IMAGE_STORE
    renderer::Image *image_storage = new renderer::StorageImage(
        512,
        512,
        1,
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView image_storage_view;
#endif


    engine.Initialize();

    v2::RenderPass::ID render_pass_id{};
    {
        auto render_pass = std::make_unique<v2::RenderPass>(RenderPass::RENDER_PASS_STAGE_SHADER, RenderPass::RENDER_PASS_INLINE);

        /* For our color attachment */
        render_pass->Get().AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
        });
        /* For our normals attachment */
        render_pass->Get().AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });
        /* For our positions attachment */
        render_pass->Get().AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_GBUFFER)
        });

        render_pass->Get().AddAttachment({
            .format = engine.GetDefaultFormat(v2::Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)
        });
        
        render_pass_id = engine.AddRenderPass(std::move(render_pass));
    }

    v2::Framebuffer::ID my_fbo_id = engine.AddFramebuffer(engine.GetInstance()->swapchain->extent.width, engine.GetInstance()->swapchain->extent.height, render_pass_id);

    {
        auto *descriptor_set_globals = engine.GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBALS);

        descriptor_set_globals
            ->AddDescriptor<BufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            ->AddSubDescriptor({ .gpu_buffer = &matrices_descriptor_buffer });

        descriptor_set_globals
            ->AddDescriptor<BufferDescriptor>(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            ->AddSubDescriptor({ .gpu_buffer = &scene_data_descriptor_buffer });

        descriptor_set_globals
            ->AddDescriptor<ImageSamplerDescriptor>(2, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            ->AddSubDescriptor({ .image_view = &test_image_view, .sampler = &test_sampler });

#if HYPERION_VK_TEST_IMAGE_STORE
        descriptor_set_globals
            ->AddDescriptor<ImageStorageDescriptor>(3, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            ->AddSubDescriptor({ .image_view = &image_storage_view });
#endif
    }

    {
        auto *descriptor_set_pass = engine.GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_PASS);

        /* Albedo texture */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(0, VK_SHADER_STAGE_FRAGMENT_BIT)
            ->AddSubDescriptor({
                .image_view = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[0].image_view.get(),
                .sampler = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[0].sampler.get()
            });

        /* Normals texture*/
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(1, VK_SHADER_STAGE_FRAGMENT_BIT)
            ->AddSubDescriptor({
                .image_view = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[1].image_view.get(),
                .sampler = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[1].sampler.get()
            });

        /* Position texture */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(2, VK_SHADER_STAGE_FRAGMENT_BIT)
            ->AddSubDescriptor({
                .image_view = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[2].image_view.get(),
                .sampler = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[2].sampler.get()
            });

        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(3, VK_SHADER_STAGE_FRAGMENT_BIT)
            ->AddSubDescriptor({
                .image_view = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[3].image_view.get(),
                .sampler = engine.GetFramebuffer(my_fbo_id)->Get().GetAttachmentImageInfos()[3].sampler.get()
            });
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

#if HYPERION_VK_TEST_IMAGE_STORE
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
#endif

    matrices_descriptor_buffer.Create(device, sizeof(MatricesBlock));
    scene_data_descriptor_buffer.Create(device, sizeof(SceneDataBlock));

    engine.PrepareSwapchain();
    
    v2::Shader::ID mirror_shader_id{};
    {
        auto mirror_shader = std::make_unique<v2::Shader>(std::vector<v2::SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read() }},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/forward_frag.spv").Read() }}
        });

        mirror_shader_id = engine.AddShader(std::move(mirror_shader));
    }

    auto scene_pass_container = std::make_unique<v2::GraphicsPipeline>(mirror_shader_id, render_pass_id);
    scene_pass_container->AddFramebuffer(my_fbo_id);

    auto scene_pass_pipeline_id = engine.AddGraphicsPipeline(std::move(scene_pass_container));
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

#if HYPERION_VK_TEST_IMAGE_STORE
    /* Compute */

    v2::Shader::ID compute_shader_id{};
    {
        auto compute_shader = std::make_unique<v2::Shader>(std::vector<v2::SubShader>{
            { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/imagestore.comp.spv").Read()}}
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
        /* Wait for compute to finish */
        compute_semaphore_chain >> engine.GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores();
    }
#endif
    

    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine.GetInstance()->GetFrameHandler()->GetNumFrames());

    //SemaphoreChain graphics_semaphore_chain({}, {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT});
    //AssertThrow(graphics_semaphore_chain.Create(engine.GetInstance()->GetDevice()));

    for (uint32_t i = 0; i < per_frame_data.GetNumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);
        AssertThrow(cmd_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsQueueData().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
        
        //graphics_semaphore_chain >> engine.GetInstance()->GetFrameHandler()
        //    ->GetPerFrameData()[i].Get<Frame>()
        //    ->GetPresentSemaphores();
    }

    auto mat1 = std::make_unique<v2::Material>();
    mat1->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(std::array{ 1.0f, 0.0f, 0.0f, 1.0f }));
    engine.AddMaterial(std::move(mat1));
    auto mat2 = std::make_unique<v2::Material>();
    mat2->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(std::array{ 0.0f, 0.0f, 1.0f, 1.0f }));
    engine.AddMaterial(std::move(mat2));

    v2::GraphicsPipeline::ID render_container_id;
    {
        auto container = std::make_unique<v2::GraphicsPipeline>(mirror_shader_id, render_pass_id);
        container->AddFramebuffer(my_fbo_id);
        container->AddSpatial(&engine, v2::Spatial{
            .id = 0,
            .mesh = monkey_mesh
        });
        container->AddSpatial(&engine, v2::Spatial{
            .id = 1,
            .mesh = cube_mesh
        });
        render_container_id = engine.AddGraphicsPipeline(std::move(container));
    }
    
    engine.Compile();

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

#if HYPERION_VK_TEST_IMAGE_STORE
        auto *compute_pipeline = engine.GetComputePipeline(compute_pipeline_id);
        compute_pipeline->Get().push_constants.counter_x = std::sin(timer) * 20.0f;
        compute_pipeline->Get().push_constants.counter_y = std::cos(timer) * 20.0f;

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

        auto *fbo_pl = engine.GetGraphicsPipeline(scene_pass_pipeline_id);
        
        matrices_descriptor_buffer.Copy(device, sizeof(matrices_block), (void *)&matrices_block);
        scene_data_descriptor_buffer.Copy(device, sizeof(scene_data_block), (void *)&scene_data_block);

        
        const uint32_t frame_index = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();


        frame->BeginCapture();

        frame->GetCommandBuffer()->RecordCommandsWithContext(
            [&](CommandBuffer *primary) {
                auto *secondary = per_frame_data[frame_index].Get<CommandBuffer>();
                secondary->Reset(engine.GetInstance()->GetDevice());

                engine.GetGraphicsPipeline(render_container_id)->Render(&engine, primary, secondary, 0);
                engine.RenderPostProcessing(primary, frame_index);

                HYPERION_RETURN_OK;
            });
        
        /*per_frame_data[frame_index].Get<CommandBuffer>()->SubmitPrimary(
            engine.GetInstance()->GetGraphicsQueue(), VK_NULL_HANDLE, &graphics_semaphore_chain
        );*/


#if HYPERION_VK_TEST_IMAGE_STORE
        VkImageMemoryBarrier imageMemoryBarrier = {};
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
            frame->GetCommandBuffer()->GetCommandBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
#endif

        engine.m_swapchain_render_container->Get().BeginRenderPass(frame->command_buffer.get(), frame_index, VK_SUBPASS_CONTENTS_INLINE);
        
        frame->GetCommandBuffer()->RecordCommandsWithContext(
            [&](CommandBuffer *cmd) {
                
                engine.m_swapchain_render_container->Get().push_constants.previous_frame_index = previous_frame_index;
                engine.m_swapchain_render_container->Get().push_constants.current_frame_index = frame_index;

                engine.RenderSwapchain(cmd);

                HYPERION_RETURN_OK;
            });
        engine.m_swapchain_render_container->Get().EndRenderPass(frame->command_buffer.get(), frame_index);
        frame->EndCapture();

        frame->Submit(engine.GetInstance()->GetGraphicsQueue());

        engine.GetInstance()->PresentFrame(frame);

        previous_frame_index = frame_index;
    }

    AssertThrow(engine.GetInstance()->GetDevice()->Wait());

    //graphics_semaphore_chain.Destroy(engine.GetInstance()->GetDevice());

    v2::Filter::full_screen_quad.reset();// have to do this here for now or else buffer does not get cleared before device is deleted

    monkey_mesh.reset(); // TMP: here to delete the mesh, so that it doesn't crash when renderer is disposed before the vbo + ibo
    cube_mesh.reset();
    full_screen_quad.reset();

    matrices_descriptor_buffer.Destroy(device);
    scene_data_descriptor_buffer.Destroy(device);
#if HYPERION_VK_TEST_IMAGE_STORE
    image_storage_view.Destroy(device);
#endif
    test_image_view.Destroy(device);

    test_sampler.Destroy(device);
    image->Destroy(device);

    for (size_t i = 0; i < per_frame_data.GetNumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();

#if HYPERION_VK_TEST_IMAGE_STORE
    compute_command_buffer->Destroy(device, engine.GetInstance()->GetComputeCommandPool());
    compute_semaphore_chain.Destroy(engine.GetInstance()->GetDevice());
#endif

    delete window;

    return 0;
}
