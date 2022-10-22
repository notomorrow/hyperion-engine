#include <rendering/HBAO.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::DynamicStorageBufferDescriptor;

Result HBAO::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result HBAO::ImageOutput::Destroy(Device *device)
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

HBAO::HBAO(const Extent2D &extent)
    : m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              ImageType::TEXTURE_TYPE_2D
          ))
      }
{
    
}

HBAO::~HBAO() = default;

void HBAO::Create(Engine *engine)
{
    CreateImages(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void HBAO::Destroy(Engine *engine)
{
    m_compute_hbao.Reset();
    m_denoiser.Reset();

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        engine->SafeRelease(std::move(descriptor_set));
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_image_outputs[frame_index].Destroy(engine->GetDevice());

            // unset final result from the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void HBAO::CreateImages(Engine *engine)
{
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (auto &image_output : m_image_outputs) {
            HYPERION_BUBBLE_ERRORS(image_output.Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    });
}

void HBAO::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_ALBEDO)->GetImageView()
            });

        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView()
            });

        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });

        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        descriptor_set->GetOrAddDescriptor<DynamicStorageBufferDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = engine->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
                .range = static_cast<UInt>(sizeof(SceneShaderData))
            });

        // output image
        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(5)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[frame_index].image_view
            });

        // prev image
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(6)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }
    
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(m_descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(m_descriptor_sets[frame_index]->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &m_image_outputs[frame_index].image_view
                });
        }

        HYPERION_RETURN_OK;
    });
}

void HBAO::CreateComputePipelines(Engine *engine)
{
    m_compute_hbao = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("HBAO")),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_compute_hbao);
}

void HBAO::Render(
    Engine *engine,
    Frame *frame
)
{
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    //m_image_outputs[(frame->GetFrameIndex() + 1) % max_frames_in_flight].image.GetGPUImage()
    //    ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> dimension = { };
    } push_constants;

    push_constants.dimension = Extent2D(extent);

    m_compute_hbao->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_hbao->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_compute_hbao->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { static_cast<UInt32>(engine->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
    );

    m_compute_hbao->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (extent.width + 7) / 8,
            (extent.height + 7) / 8,
            1
        }
    );
    
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2