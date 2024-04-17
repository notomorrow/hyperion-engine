/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/SSRRenderer.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <Engine.hpp>
#include <Threads.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion {

using renderer::ShaderVec2;
using renderer::Result;
using renderer::GPUBufferType;

static constexpr bool use_temporal_blending = true;

static constexpr InternalFormat ssr_format = InternalFormat::RGBA16F;

struct alignas(16) SSRParams
{
    ShaderVec4<uint32> dimensions;
    float32 ray_step,
        num_iterations,
        max_ray_distance,
        distance_bias,
        offset,
        eye_fade_start,
        eye_fade_end,
        screen_edge_fade_start,
        screen_edge_fade_end;
};

#pragma region Render commands

struct RENDER_COMMAND(CreateSSRUniformBuffer) : renderer::RenderCommand
{
    Extent2D        extent;
    GPUBufferRef    uniform_buffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        Extent2D extent,
        GPUBufferRef uniform_buffers
    ) : extent(extent),
        uniform_buffer(std::move(uniform_buffers))
    {
        AssertThrow(extent.Size() != 0);
        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual Result operator()() override
    {
        const SSRParams ssr_params {
            .dimensions             = { extent.width, extent.height, 0, 0 },
            .ray_step               = 0.1f,
            .num_iterations         = 128.0f,
            .max_ray_distance       = 150.0f,
            .distance_bias          = 0.1f,
            .offset                 = 0.001f,
            .eye_fade_start         = 0.90f,
            .eye_fade_end           = 0.96f,
            .screen_edge_fade_start = 0.7f,
            .screen_edge_fade_end   = 0.98f
        };
            
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(ssr_params)
        ));

        uniform_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(ssr_params),
            &ssr_params
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSSRDescriptors) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateSSRDescriptors)(
        const FixedArray<ImageViewRef, max_frames_in_flight> &image_views
    ) : image_views(image_views)
    {
    }

    virtual ~RENDER_COMMAND(CreateSSRDescriptors)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(SSRResultTexture), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveSSRDescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(RemoveSSRDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(RemoveSSRDescriptors)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(SSRResultTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

SSRRenderer::SSRRenderer(const Extent2D &extent, SSRRendererOptions options)
    : m_extent(extent),
      m_options(options),
      m_is_rendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
}

void SSRRenderer::Create()
{
    m_image_outputs[0] = CreateObject<Texture>(Texture2D(
        m_extent,
        ssr_format,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_image_outputs[0]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[0]);

    m_image_outputs[1] = CreateObject<Texture>(Texture2D(
        m_extent,
        ssr_format,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_image_outputs[1]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[1]);

    m_image_outputs[2] = CreateObject<Texture>(Texture2D(
        m_extent,
        ssr_format,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_image_outputs[2]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[2]);

    m_image_outputs[3] = CreateObject<Texture>(Texture2D(
        m_extent,
        ssr_format,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_image_outputs[3]->GetImage()->SetIsRWTexture(true);
    InitObject(m_image_outputs[3]);

    CreateUniformBuffers();

    if (use_temporal_blending) {
        m_temporal_blending.Reset(new TemporalBlending(
            m_extent,
            InternalFormat::RGBA16F,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::LOW,
            FixedArray<ImageViewRef, 2> {
                m_image_outputs[1]->GetImageView(),
                m_image_outputs[1]->GetImageView()
            }
        ));

        m_temporal_blending->Create();
    }

    CreateComputePipelines();
}

void SSRRenderer::Destroy()
{
    m_is_rendered = false;

    m_write_uvs.Reset();
    m_sample.Reset();

    if (m_temporal_blending) {
        m_temporal_blending->Destroy();
        m_temporal_blending.Reset();
    }
    
    SafeRelease(std::move(m_uniform_buffer));

    PUSH_RENDER_COMMAND(RemoveSSRDescriptors);

    HYP_SYNC_RENDER();
}

ShaderProperties SSRRenderer::GetShaderProperties() const
{
    ShaderProperties shader_properties;
    shader_properties.Set("CONE_TRACING", bool(m_options & SSR_RENDERER_OPTIONS_CONE_TRACING));
    shader_properties.Set("ROUGHNESS_SCATTERING", bool(m_options & SSR_RENDERER_OPTIONS_ROUGHNESS_SCATTERING));

    switch (ssr_format) {
    case InternalFormat::RGBA8:
        shader_properties.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_properties.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_properties.Set("OUTPUT_RGBA32F");
        break;
    }

    return shader_properties;
}

void SSRRenderer::CreateUniformBuffers()
{
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateSSRUniformBuffer,
        m_extent,
        m_uniform_buffer
    );
}

void SSRRenderer::CreateComputePipelines()
{
    const ShaderProperties shader_properties = GetShaderProperties();

    // Write UVs pass

    Handle<Shader> write_uvs_shader = g_shader_manager->GetOrCreate(HYP_NAME(SSRWriteUVs), shader_properties);
    AssertThrow(write_uvs_shader.IsValid());

    const renderer::DescriptorTableDeclaration write_uvs_shader_descriptor_table_decl = write_uvs_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef write_uvs_shader_descriptor_table = MakeRenderObject<renderer::DescriptorTable>(write_uvs_shader_descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = write_uvs_shader_descriptor_table->GetDescriptorSet(HYP_NAME(SSRDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(UVImage), m_image_outputs[0]->GetImageView());
        descriptor_set->SetElement(HYP_NAME(SSRParams), m_uniform_buffer);
    }

    DeferCreate(write_uvs_shader_descriptor_table, g_engine->GetGPUDevice());

    m_write_uvs = MakeRenderObject<renderer::ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(SSRWriteUVs), shader_properties)->GetShaderProgram(),
        write_uvs_shader_descriptor_table
    );

    DeferCreate(m_write_uvs, g_engine->GetGPUDevice());

    // Sample pass

    Handle<Shader> sample_shader = g_shader_manager->GetOrCreate(HYP_NAME(SSRSample), shader_properties);
    AssertThrow(sample_shader.IsValid());

    const renderer::DescriptorTableDeclaration sample_shader_descriptor_table_decl = sample_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef sample_shader_descriptor_table = MakeRenderObject<renderer::DescriptorTable>(sample_shader_descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = sample_shader_descriptor_table->GetDescriptorSet(HYP_NAME(SSRDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(UVImage), m_image_outputs[0]->GetImageView());
        descriptor_set->SetElement(HYP_NAME(SampleImage), m_image_outputs[1]->GetImageView());
        descriptor_set->SetElement(HYP_NAME(SSRParams), m_uniform_buffer);
    }

    DeferCreate(sample_shader_descriptor_table, g_engine->GetGPUDevice());

    m_sample = MakeRenderObject<renderer::ComputePipeline>(
        sample_shader->GetShaderProgram(),
        sample_shader_descriptor_table
    );

    DeferCreate(m_sample, g_engine->GetGPUDevice());

    PUSH_RENDER_COMMAND(
        CreateSSRDescriptors,
        FixedArray {
            m_temporal_blending ? m_temporal_blending->GetImageOutput(0).image_view : m_image_outputs[1]->GetImageView(),
            m_temporal_blending ? m_temporal_blending->GetImageOutput(1).image_view : m_image_outputs[1]->GetImageView()
        }
    );
}

void SSRRenderer::Render(Frame *frame)
{
    const auto &scene_binding = g_engine->render_state.GetScene();
    const uint scene_index = scene_binding.id.ToIndex();

    const auto &camera_binding = g_engine->render_state.GetCamera();
    const uint camera_index = camera_binding.id.ToIndex();

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");

    // We will be dispatching half the number of pixels, due to checkerboarding
    const SizeType total_pixels_in_image = m_extent.Size();
    const SizeType total_pixels_in_image_div_2 = total_pixels_in_image / 2;

    const uint32 num_dispatch_calls = (uint32(total_pixels_in_image_div_2) + 255) / 256;

    // PASS 1 -- write UVs

    m_image_outputs[0]->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_write_uvs->Bind(command_buffer);

    m_write_uvs->GetDescriptorTable()->Bind(
        frame,
        m_write_uvs,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                }
            }
        }
    );

    m_write_uvs->Dispatch(command_buffer, Extent3D {
        num_dispatch_calls, 1, 1
    });

    // transition the UV image back into read state
    m_image_outputs[0]->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_image_outputs[1]->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_sample->Bind(command_buffer);

    m_sample->GetDescriptorTable()->Bind(
        frame,
        m_sample,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                }
            }
        }
    );

    m_sample->Dispatch(command_buffer, Extent3D {
        num_dispatch_calls, 1, 1
    });

    // transition sample image back into read state
    m_image_outputs[1]->GetImage()->GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    if (use_temporal_blending && m_temporal_blending != nullptr) {
        m_temporal_blending->Render(frame);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

} // namespace hyperion
