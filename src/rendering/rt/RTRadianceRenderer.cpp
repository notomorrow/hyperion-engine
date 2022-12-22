#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::StorageBufferDescriptor;
using renderer::TlasDescriptor;
using renderer::ResourceState;
using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(CreateRTRadianceDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;
    FixedArray<ImageViewRef, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateRTRadianceDescriptorSets)(
        const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets,
        const FixedArray<ImageViewRef, max_frames_in_flight> &image_views
    ) : descriptor_sets(descriptor_sets),
        image_views(image_views)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetElementSRV(0, image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateRTRadiancePipeline) : RenderCommand
{
    RaytracingPipelineRef pipeline;
    ShaderProgramRef shader_program;

    RENDER_COMMAND(CreateRTRadiancePipeline)(const RaytracingPipelineRef &pipeline, const ShaderProgramRef &shader_program)
        : pipeline(pipeline),
          shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            Engine::Get()->GetGPUDevice(),
            shader_program.Get(),
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyRTRadianceRenderer) : RenderCommand
{
    RTRadianceRenderer::ImageOutput *image_outputs;

    RENDER_COMMAND(DestroyRTRadianceRenderer)(RTRadianceRenderer::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        // remove result image from global descriptor set
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_PASS_ERRORS(
                image_outputs[frame_index].Destroy(Engine::Get()->GetGPUDevice()),
                result
            );

            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            // set to placeholder image
            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateRTRadianceImageOutputs) : RenderCommand
{
    RTRadianceRenderer::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateRTRadianceImageOutputs)(RTRadianceRenderer::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(Engine::Get()->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Result RTRadianceRenderer::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result RTRadianceRenderer::ImageOutput::Destroy(Device *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(
        image.Destroy(device),
        result
    );

    HYPERION_PASS_ERRORS(
        image_view.Destroy(device),
        result
    );

    return result;
}

RTRadianceRenderer::RTRadianceRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          ))
      },
      m_temporal_blending(
          extent,
          InternalFormat::RGBA8,
          TemporalBlendTechnique::TECHNIQUE_2,
          FixedArray<ImageView *, max_frames_in_flight> { &m_image_outputs[0].image_view, &m_image_outputs[1].image_view }
      ),
      m_has_tlas_updates { false, false }
{
}

RTRadianceRenderer::~RTRadianceRenderer()
{
}

void RTRadianceRenderer::Create()
{
    AssertThrowMsg(
        InitObject(m_tlas),
        "Failed to initialize the top level acceleration structure!"
    );

    CreateImages();
    CreateTemporalBlending();
    CreateDescriptorSets();
    CreateRaytracingPipeline();
}

void RTRadianceRenderer::Destroy()
{
    m_temporal_blending.Destroy();

    Engine::Get()->SafeReleaseHandle(std::move(m_shader));

    SafeRelease(std::move(m_raytracing_pipeline));

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        SafeRelease(std::move(descriptor_set));
    }

    PUSH_RENDER_COMMAND(DestroyRTRadianceRenderer, m_image_outputs.Data());

    HYP_SYNC_RENDER();
}

void RTRadianceRenderer::Render(Frame *frame)
{
    if (m_has_tlas_updates[frame->GetFrameIndex()]) {
        m_descriptor_sets[frame->GetFrameIndex()]->ApplyUpdates(Engine::Get()->GetGPUDevice());

        m_has_tlas_updates[frame->GetFrameIndex()] = false;
    }

    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, frame->GetFrameIndex()),
        1,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->render_state.GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex())
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        2
    );

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_raytracing_pipeline->TraceRays(
        Engine::Get()->GetGPUDevice(),
        frame->GetCommandBuffer(),
        m_image_outputs[frame->GetFrameIndex()].image.GetExtent()
    );

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );

    m_temporal_blending.Render(frame);
}

void RTRadianceRenderer::CreateImages()
{
    PUSH_RENDER_COMMAND(CreateRTRadianceImageOutputs, m_image_outputs.Data());
}

void RTRadianceRenderer::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(0)
                ->SetElementAccelerationStructure(0, &m_tlas->GetInternalTLAS());
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(2)
                ->SetElementBuffer(0, m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer());
        }

        m_has_tlas_updates[frame_index] = true;
    }
}

void RTRadianceRenderer::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = DescriptorSetRef::Construct();

        descriptor_set->GetOrAddDescriptor<TlasDescriptor>(0)
            ->SetElementAccelerationStructure(0, &m_tlas->GetInternalTLAS());

        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(1)
            ->SetElementUAV(0, &m_image_outputs[frame_index].image_view);
        
        // mesh descriptions
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer());
        
        // materials
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(3)
            ->SetElementBuffer(0, Engine::Get()->shader_globals->materials.GetBuffers()[frame_index].get());
        
        // entities
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(4)
            ->SetElementBuffer(0, Engine::Get()->shader_globals->objects.GetBuffers()[frame_index].get());

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    PUSH_RENDER_COMMAND(
        CreateRTRadianceDescriptorSets,
        m_descriptor_sets,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending.GetImageOutput(0).image_view,
            m_temporal_blending.GetImageOutput(1).image_view
        }
    );
}

void RTRadianceRenderer::CreateRaytracingPipeline()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTRadiance"));
    if (!InitObject(m_shader)) {
        return;
    }

    m_raytracing_pipeline = RaytracingPipelineRef::Construct(
        Array<const DescriptorSet *> {
            m_descriptor_sets[0].Get(),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        }
    );

    Engine::Get()->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](...) {
        PUSH_RENDER_COMMAND(
            CreateRTRadiancePipeline,
            m_raytracing_pipeline,
            m_shader->GetShaderProgram()
        );
    });
}

void RTRadianceRenderer::CreateTemporalBlending()
{
    m_temporal_blending.Create();
}

} // namespace hyperion::v2