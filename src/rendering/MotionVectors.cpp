#include <rendering/MotionVectors.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::TextureImage2D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::DynamicStorageBufferDescriptor;

Result MotionVectors::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result MotionVectors::ImageOutput::Destroy(Device *device)
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

MotionVectors::MotionVectors(const Extent2D &extent)
    : m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
              ImageType::TEXTURE_TYPE_2D
          ))
      },
      m_depth_image(nullptr),
      m_depth_image_view(nullptr)
{
}

MotionVectors::~MotionVectors() = default;

void MotionVectors::Create(Engine *engine)
{
    CreateImages(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void MotionVectors::Destroy(Engine *engine)
{
    m_calculate_motion_vectors.Reset();

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        engine->SafeRelease(std::move(descriptor_set));
    }

    engine->SafeRelease(std::move(m_previous_depth_image));
    engine->SafeRelease(std::move(m_previous_depth_image_view));

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_image_outputs[frame_index].Destroy(engine->GetDevice());

            // unset final result from the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::MOTION_VECTORS_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void MotionVectors::CreateImages(Engine *engine)
{
    auto *depth_attachment_ref = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
        .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH);

    m_depth_image = depth_attachment_ref->GetAttachment()->GetImage();
    m_depth_image_view = depth_attachment_ref->GetImageView();

    AssertThrow(m_depth_image != nullptr);
    AssertThrow(m_depth_image_view != nullptr);

    m_previous_depth_image.Reset(new TextureImage2D(
        Extent2D(m_depth_image->GetExtent()),
        m_depth_image->GetTextureFormat(),
        FilterMode::TEXTURE_FILTER_NEAREST,
        nullptr
    ));

    m_previous_depth_image_view.Reset(new ImageView);

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        HYPERION_BUBBLE_ERRORS(m_previous_depth_image->Create(engine->GetDevice()));
        HYPERION_BUBBLE_ERRORS(m_previous_depth_image_view->Create(engine->GetDevice(), m_previous_depth_image.Get()));

        for (auto &image_output : m_image_outputs) {
            HYPERION_BUBBLE_ERRORS(image_output.Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    });
}

void MotionVectors::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        // current depth image
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_depth_image_view
            });

        // previous depth image
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_previous_depth_image_view.Get()
            });

        // nearest sampler
        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        // output image (motion vectors)
        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[frame_index].image_view
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
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::MOTION_VECTORS_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &m_image_outputs[frame_index].image_view
                });
        }

        HYPERION_RETURN_OK;
    });
}

void MotionVectors::CreateComputePipelines(Engine *engine)
{
    m_calculate_motion_vectors = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("CalculateMotionVectors")),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_calculate_motion_vectors);
}

void MotionVectors::Render(
    Engine *engine,
    Frame *frame
)
{
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> dimension;
    } push_constants;

    push_constants.dimension = Extent2D(extent);

    m_calculate_motion_vectors->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_calculate_motion_vectors->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_calculate_motion_vectors->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_calculate_motion_vectors->GetPipeline()->Dispatch(
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