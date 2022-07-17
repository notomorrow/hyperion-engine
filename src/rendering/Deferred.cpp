#include "Deferred.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

ScreenspaceReflectionRenderer::ScreenspaceReflectionRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_is_rendered(false)
{
}

ScreenspaceReflectionRenderer::~ScreenspaceReflectionRenderer()
{
}

void ScreenspaceReflectionRenderer::Create(Engine *engine)
{
    CreateComputePipelines(engine);
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j] = SSRImageOutput {
                .image = std::make_unique<StorageImage>(
                    Extent3D(m_extent),
                    Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                    Image::Type::TEXTURE_TYPE_2D,
                    nullptr
                ),
                .image_view = std::make_unique<ImageView>()
            };

            m_ssr_image_outputs[i][j].Create(engine->GetDevice());
        }

        m_ssr_radius_output[i] = SSRImageOutput {
            .image = std::make_unique<StorageImage>(
                Extent3D(m_extent),
                Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
                Image::Type::TEXTURE_TYPE_2D,
                nullptr
            ),
            .image_view = std::make_unique<ImageView>()
        };

        m_ssr_radius_output[i].Create(engine->GetDevice());
    }

    CreateDescriptors(engine);
}

void ScreenspaceReflectionRenderer::Destroy(Engine *engine)
{
    m_is_rendered = false;

    m_ssr_write_uvs.Reset();
    m_ssr_sample.Reset();
    m_ssr_blur_hor.Reset();
    m_ssr_blur_vert.Reset();

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (UInt j = 0; j < static_cast<UInt>(m_ssr_image_outputs[i].size()); j++) {
            m_ssr_image_outputs[i][j].Destroy(engine->GetDevice());
        }

        m_ssr_radius_output[i].Destroy(engine->GetDevice());
    }
}

void ScreenspaceReflectionRenderer::CreateDescriptors(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

        /* SSR Data */
        descriptor_set_pass // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_UV_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][0].image_view.get()
            });

        descriptor_set_pass // 2nd stage -- sample
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_SAMPLE_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][1].image_view.get()
            });

        descriptor_set_pass // 2nd stage -- write radii
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_RADIUS_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_radius_output[i].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][2].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::StorageImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_IMAGE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][3].image_view.get()
            });

        /* SSR Data */
        descriptor_set_pass // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_UV_TEXTURE)
            ->SetSubDescriptor({
               .image_view = m_ssr_image_outputs[i][0].image_view.get()
           });

        descriptor_set_pass // 2nd stage -- sample
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_SAMPLE_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][1].image_view.get()
           });

        descriptor_set_pass // 2nd stage -- write radii
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_RADIUS_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_radius_output[i].image_view.get()
           });

        descriptor_set_pass // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_HOR_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][2].image_view.get()
            });

        descriptor_set_pass // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::ImageDescriptor>(DescriptorKey::SSR_BLUR_VERT_TEXTURE)
            ->SetSubDescriptor({
                .image_view = m_ssr_image_outputs[i][3].image_view.get()
            });
    }
}

void ScreenspaceReflectionRenderer::CreateComputePipelines(Engine *engine)
{
    m_ssr_write_uvs = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_write_uvs.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_write_uvs.Init();

    m_ssr_sample = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_sample.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_sample.Init();

    m_ssr_blur_hor = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_blur_hor.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_blur_hor.Init();

    m_ssr_blur_vert = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/ssr/ssr_blur_vert.comp.spv")).Read()}}
            }
        ))
    ));

    m_ssr_blur_vert.Init();
}

void ScreenspaceReflectionRenderer::Render(
    Engine *engine,
    Frame *frame
)
{
    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

    auto *command_buffer   = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");
    
    // PASS 1 -- write UVs

    // start by putting the UV image in a writeable state
    const Pipeline::PushConstantData ssr_push_constant_data {
        .ssr_data = {
            .width                  = m_extent.width,
            .height                 = m_extent.height,
            .ray_step               = 0.75f,
            .num_iterations         = 80.0f,
            .max_ray_distance       = 128.0f,
            .distance_bias          = 0.1f,
            .offset                 = 0.01f,
            .eye_fade_start         = 0.45f,
            .eye_fade_end           = 0.75f,
            .screen_edge_fade_start = 0.45f,
            .screen_edge_fade_end   = 0.75f
        }
    };

    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_write_uvs->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_write_uvs->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_write_uvs->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition the UV image back into read state
    m_ssr_image_outputs[frame_index][0].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);
    // put radius image in writeable state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_sample->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_sample->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_sample->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition sample image back into read state
    m_ssr_image_outputs[frame_index][1].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    // transition radius image back into read state
    m_ssr_radius_output[frame_index].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    // PASS 3 - blur image using radii in output from previous stage

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_hor->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_hor->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_blur_hor->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][2].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    // PASS 4 - blur image vertically

    //put blur image in writeable state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::UNORDERED_ACCESS);

    m_ssr_blur_vert->GetPipeline()->Bind(command_buffer, ssr_push_constant_data);

    // bind `global` descriptor set
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(), command_buffer, m_ssr_blur_vert->GetPipeline(),
        {
            {.set = DescriptorSet::scene_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {
                .offsets = {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0 * sizeof(LightShaderData)) // light unused here
                }
            }
        }
    );

    m_ssr_blur_vert->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_ssr_image_outputs[frame_index][3].image->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);


    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

DepthPyramidRenderer::DepthPyramidRenderer()
    : m_depth_attachment_ref(nullptr),
      m_is_rendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer()
{
}

void DepthPyramidRenderer::Create(Engine *engine, const AttachmentRef *depth_attachment_ref)
{
    AssertThrow(m_depth_attachment_ref == nullptr);
    AssertThrow(depth_attachment_ref->IsDepthAttachment());
    m_depth_attachment_ref = depth_attachment_ref->IncRef(HYP_ATTACHMENT_REF_INSTANCE);

    // nearest for now -- will use 4x4 min sampler
    m_depth_pyramid_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_NEAREST);
    HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Create(engine->GetDevice()));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        const auto *depth_attachment = m_depth_attachment_ref->GetAttachment();
        AssertThrow(depth_attachment != nullptr);

        const auto *depth_image = depth_attachment->GetImage();
        AssertThrow(depth_image != nullptr);

        // create depth pyramid image
        m_depth_pyramid[i] = std::make_unique<StorageImage>(
            Extent3D {
                static_cast<UInt>(MathUtil::PreviousPowerOf2(depth_image->GetExtent().width)),
                static_cast<UInt>(MathUtil::PreviousPowerOf2(depth_image->GetExtent().height)),
                1
            },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8,
            Image::Type::TEXTURE_TYPE_2D,
            Image::FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,//Image::FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP,
            nullptr
        );

        m_depth_pyramid[i]->Create(engine->GetDevice());

        m_depth_pyramid_results[i] = std::make_unique<ImageView>();
        m_depth_pyramid_results[i]->Create(engine->GetDevice(), m_depth_pyramid[i].get());

        const auto num_mip_levels = m_depth_pyramid[i]->NumMipmaps();

        m_depth_pyramid_mips[i].Reserve(num_mip_levels);

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            auto mip_image_view = std::make_unique<ImageView>();

            HYPERION_ASSERT_RESULT(mip_image_view->Create(
                engine->GetDevice(),
                m_depth_pyramid[i].get(),
                mip_level, 1,
                0, m_depth_pyramid[i]->NumFaces()
            ));

            m_depth_pyramid_mips[i].PushBack(std::move(mip_image_view));
        // }

            // create descriptor sets for depth pyramid generation.
            auto depth_pyramid_descriptor_set = std::make_unique<DescriptorSet>();

            /* Depth pyramid - generated w/ compute shader */
            auto *depth_pyramid_in = depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::ImageDescriptor>(0);

            if (mip_level == 0) {
                // first mip level -- input is the actual depth image
                depth_pyramid_in->SetSubDescriptor({
                    .element_index = 0,
                    .image_view    = depth_attachment_ref->GetImageView()
                });
            } else {
                depth_pyramid_in->SetSubDescriptor({
                    .element_index = 0,
                    .image_view    = m_depth_pyramid_mips[i][mip_level - 1].get()
                });
            }

            auto *depth_pyramid_out = depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::StorageImageDescriptor>(1);

            depth_pyramid_out->SetSubDescriptor({
                .element_index = 0,
                .image_view    = m_depth_pyramid_mips[i][mip_level].get()
            });

            depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::SamplerDescriptor>(2)
                ->SetSubDescriptor({
                    .sampler = m_depth_pyramid_sampler.get()
                });

            HYPERION_ASSERT_RESULT(depth_pyramid_descriptor_set->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            m_depth_pyramid_descriptor_sets[i].PushBack(std::move(depth_pyramid_descriptor_set));
        }
    }
    // create compute pipeline for rendering depth image
    m_generate_depth_pyramid = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/generate_depth_pyramid.comp.spv")).Read()}}
            }
        )),
        DynArray<const DescriptorSet *> { m_depth_pyramid_descriptor_sets[0].Front().get() } // only need to pass first to use for layout.
    ));

    m_generate_depth_pyramid.Init();
}

void DepthPyramidRenderer::Destroy(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &descriptor_set : m_depth_pyramid_descriptor_sets[i]) {
            HYPERION_ASSERT_RESULT(descriptor_set->Destroy(engine->GetDevice()));
        }

        m_depth_pyramid_descriptor_sets[i].Clear();

        for (auto &mip_image_view : m_depth_pyramid_mips[i]) {
            HYPERION_ASSERT_RESULT(mip_image_view->Destroy(engine->GetDevice()));
        }

        m_depth_pyramid_mips[i].Clear();

        HYPERION_ASSERT_RESULT(m_depth_pyramid_results[i]->Destroy(engine->GetDevice()));
        HYPERION_ASSERT_RESULT(m_depth_pyramid[i]->Destroy(engine->GetDevice()));
    }

    HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Destroy(engine->GetDevice()));

    if (m_depth_attachment_ref != nullptr) {
        m_depth_attachment_ref->DecRef(HYP_ATTACHMENT_REF_INSTANCE);
        m_depth_attachment_ref = nullptr;
    }

    m_is_rendered = false;
}

void DepthPyramidRenderer::Render(Engine *engine, Frame *frame)
{
    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    DebugMarker marker(primary, "Depth pyramid generation");

    const auto num_depth_pyramid_mip_levels = m_depth_pyramid_mips[frame_index].Size();

    const auto &image_extent         = m_depth_attachment_ref->GetAttachment()->GetImage()->GetExtent();
    const auto &depth_pyramid_extent = m_depth_pyramid[frame_index]->GetExtent();

    UInt32 mip_width  = image_extent.width,
           mip_height = image_extent.height;

    for (UInt mip_level = 0; mip_level < num_depth_pyramid_mip_levels; mip_level++) {
        // frame 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        m_depth_pyramid[frame_index]->GetGPUImage()->InsertSubResourceBarrier(
            primary,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        
        const auto prev_mip_width  = mip_width,
                   prev_mip_height = mip_height;

        mip_width  = MathUtil::Max(1, depth_pyramid_extent.width >> mip_level);
        mip_height = MathUtil::Max(1, depth_pyramid_extent.height >> mip_level);

        // bind descriptor set to compute pipeline
        primary->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            m_generate_depth_pyramid->GetPipeline(),
            m_depth_pyramid_descriptor_sets[frame_index][mip_level].get(), // for now.. could go with 1 per mip level
            static_cast<DescriptorSet::Index>(0)
        );

        // set push constant data for the current mip level
        m_generate_depth_pyramid->GetPipeline()->Bind(
            primary,
            Pipeline::PushConstantData {
                .depth_pyramid_data = {
                    .mip_width       = mip_width,
                    .mip_height      = mip_height,
                    .prev_mip_width  = prev_mip_width,
                    .prev_mip_height = prev_mip_height,
                    .mip_level        = mip_level
                }
            }
        );

        // dispatch to generate this mip level
        m_generate_depth_pyramid->GetPipeline()->Dispatch(
            primary,
            Extent3D {
                (mip_width + 31)  / 32,
                (mip_height + 31) / 32,
                1
            }
        );

        // put this mip into readable state
        m_depth_pyramid[frame_index]->GetGPUImage()->InsertSubResourceBarrier(
            primary,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::GPUMemory::ResourceState::SHADER_RESOURCE
        );
    }

    // all mip levels have been transitioned into this state
    m_depth_pyramid[frame_index]->GetGPUImage()->SetResourceState(
        renderer::GPUMemory::ResourceState::SHADER_RESOURCE
    );

    m_is_rendered = true;
}

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    if (m_is_indirect_pass) {
        m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred indirect vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_indirect.frag.spv")).Read(),
                    {.name = "deferred indirect frag"}
                }}
            }
        ));
    } else {
        m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred direct vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_direct.frag.spv")).Read(),
                    {.name = "deferred direct frag"}
                }}
            }
        ));
    }

    m_shader->Init(engine);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    if (m_is_indirect_pass) {
        return;
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler(),
                });
            }
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    CreateShader(engine);
    CreateRenderPass(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i].IncRef();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    RenderableAttributeSet renderable_attributes {
        .bucket            = BUCKET_INTERNAL,
        .vertex_attributes = renderer::static_mesh_vertex_attributes,
        .fill_mode         = FillMode::FILL,
        .depth_write       = false,
        .depth_test        = false
    };

    if (!m_is_indirect_pass) {
        renderable_attributes.alpha_blending = true;
    }

    CreatePipeline(engine, renderable_attributes);
}

void DeferredPass::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Record(Engine *engine, UInt frame_index)
{
    if (m_is_indirect_pass) {
        FullScreenPass::Record(engine, frame_index);
        
        return;
    }

    // no lights bound, do not render direct shading at all
    if (engine->render_state.light_ids.Empty()) {
        return;
    }

    using renderer::Result;

    auto *command_buffer = m_command_buffers[frame_index].get();

    auto record_result = command_buffer->Record(
        engine->GetInstance()->GetDevice(),
        m_pipeline->GetPipeline()->GetConstructionInfo().render_pass,
        [this, engine, frame_index](CommandBuffer *cmd) {
            m_pipeline->GetPipeline()->push_constants = m_push_constant_data;
            m_pipeline->GetPipeline()->Bind(cmd);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif

            // render with each light
            for (const auto &light_id : engine->render_state.light_ids) {
                cmd->BindDescriptorSet(
                    engine->GetInstance()->GetDescriptorPool(),
                    m_pipeline->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        UInt32(sizeof(SceneShaderData) * scene_index),
                        UInt32(sizeof(LightShaderData) * (light_id.value - 1))
                    }
                );

                full_screen_quad->Render(engine, cmd);
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer()
    : m_ssr(Extent2D { 1024, 1024 }),
      m_indirect_pass(true),
      m_direct_pass(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create(engine);

    m_indirect_pass.Create(engine);
    m_direct_pass.Create(engine);

    const auto &attachment_refs = m_indirect_pass.GetRenderPass()->GetRenderPass().GetAttachmentRefs();

    const auto *depth_attachment_ref = attachment_refs.back();
    AssertThrow(depth_attachment_ref != nullptr);

    m_dpr.Create(engine, depth_attachment_ref);
    m_ssr.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_mipmapped_results[i] = engine->resources.textures.Add(std::make_unique<Texture2D>(
            Extent2D { 1024, 1024 },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        m_mipmapped_results[i].Init();
    }
    

    m_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetDevice()));

    m_depth_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_NEAREST);
    HYPERION_ASSERT_RESULT(m_depth_sampler->Create(engine->GetDevice()));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];
        
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        
        descriptor_set_globals->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

        UInt attachment_index = 0;

        /* Gbuffer textures */
        for (; attachment_index < RenderListContainer::gbuffer_textures.size() - 1; attachment_index++) {
            descriptor_set_globals
                ->GetDescriptor(DescriptorKey::GBUFFER_TEXTURES)
                ->SetSubDescriptor({
                    .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });
        }

        auto *depth_image = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index];

        /* Depth texture */
        descriptor_set_globals
            ->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
            ->SetSubDescriptor({
                .image_view = depth_image->GetImageView()
            });

        /* Mip chain */
        descriptor_set_globals
            ->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
            ->SetSubDescriptor({
                .image_view = &m_mipmapped_results[i]->GetImageView()
            });

        /* Gbuffer depth sampler */
        descriptor_set_globals
            ->AddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
            ->SetSubDescriptor({
                .sampler = m_depth_sampler.get()
            });

        /* Gbuffer sampler */
        descriptor_set_globals
            ->AddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetSubDescriptor({
                .sampler = m_sampler.get()
            });

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
            ->SetSubDescriptor({
                .image_view = m_dpr.GetResults()[i].get()
            });
    }
    
    m_indirect_pass.CreateDescriptors(engine); // no-op
    m_direct_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_ssr.Destroy(engine);
    m_dpr.Destroy(engine);

    m_post_processing.Destroy(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        engine->SafeReleaseRenderable(std::move(m_mipmapped_results[i]));
    }
    
    HYPERION_ASSERT_RESULT(m_depth_sampler->Destroy(engine->GetDevice()));
    HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetDevice()));

    m_indirect_pass.Destroy(engine);  // flushes render queue
    m_direct_pass.Destroy(engine);    // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    RenderOpaqueObjects(engine, frame, true);
    RenderTranslucentObjects(engine, frame, true);

    auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

    if (ssr_enabled && mipmapped_result.GetGPUImage()->GetResourceState() != renderer::GPUMemory::ResourceState::UNDEFINED) {
        m_ssr.Render(engine, frame);
    }

    {
        DebugMarker marker(primary, "Record deferred indirect lighting pass");

        m_indirect_pass.m_push_constant_data.deferred_data = {
            .flags = (DeferredRenderer::ssr_enabled && m_ssr.IsRendered())
                ? DEFERRED_FLAGS_SSR_ENABLED
                : 0,
            .depth_pyramid_num_mips = m_dpr.IsRendered()
                ? static_cast<UInt32>(m_dpr.GetMips()[frame_index].Size())
                : 0
        };

        m_indirect_pass.Record(engine, frame_index); // could be moved to only do once
    }

    {
        DebugMarker marker(primary, "Record deferred direct lighting pass");

        m_direct_pass.m_push_constant_data = m_indirect_pass.m_push_constant_data;

        m_direct_pass.Record(engine, frame_index);
    }

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    {
        DebugMarker marker(primary, "Render opaque objects");

        bucket.GetFramebuffers()[frame_index]->BeginCapture(primary);
        RenderOpaqueObjects(engine, frame, false);
        bucket.GetFramebuffers()[frame_index]->EndCapture(primary);
    }
    // end opaque objs
    

    m_post_processing.RenderPre(engine, frame);

    // begin shading
    m_direct_pass.GetFramebuffer(frame_index)->BeginCapture(primary);

    // indirect shading
    HYPERION_ASSERT_RESULT(m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    // direct shading
    if (!engine->render_state.light_ids.Empty()) {
        HYPERION_ASSERT_RESULT(m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));
    }

    // begin translucent with forward rendering
    RenderTranslucentObjects(engine, frame, false);

    // end shading
    m_direct_pass.GetFramebuffer(frame_index)->EndCapture(primary);

    // render depth pyramid
    m_dpr.Render(engine, frame);

    /* ========== BEGIN MIP CHAIN GENERATION ========== */
    {
        DebugMarker marker(primary, "Mipmap chain generation");

        auto *framebuffer_image = m_direct_pass.GetFramebuffer(frame_index)->GetFramebuffer()//bucket.GetFramebuffers()[frame_index]->GetFramebuffer()
            .GetAttachmentRefs()[0]->GetAttachment()->GetImage();
        
        framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_SRC);
        mipmapped_result.GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_DST);

        // Blit into the mipmap chain img
        mipmapped_result.Blit(
            primary,
            framebuffer_image,
            Rect { 0, 0, framebuffer_image->GetExtent().width, framebuffer_image->GetExtent().height },
            Rect { 0, 0, mipmapped_result.GetExtent().width, mipmapped_result.GetExtent().height }
        );

        HYPERION_ASSERT_RESULT(mipmapped_result.GenerateMipmaps(engine->GetDevice(), primary));

        framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    }

    /* ==========  END MIP CHAIN GENERATION ========== */

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame, bool collect)
{
    if (collect) {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
            pipeline->CollectDrawCalls(engine, frame);
        }
        
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
            pipeline->CollectDrawCalls(engine, frame);
        }
    } else {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
            pipeline->PerformRendering(engine, frame);
        }
        
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
            pipeline->PerformRendering(engine, frame);
        }
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame, bool collect)
{
    if (collect) {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
            pipeline->CollectDrawCalls(engine, frame);
        }
    } else {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
            pipeline->PerformRendering(engine, frame);
        }
    }
}

} // namespace hyperion::v2
