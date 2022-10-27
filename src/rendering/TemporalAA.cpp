#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <math/Vector2.hpp>
#include <math/MathUtil.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::ShaderVec2;
using renderer::ShaderMat4;

struct HaltonSequence
{
    FixedArray<Vector2, 128> sequence;

    HaltonSequence()
    {
        for (UInt index = 0; index < static_cast<UInt>(sequence.Size()); index++) {
            sequence[index].x = GetHalton(index + 1, 2);
            sequence[index].y = GetHalton(index + 1, 3);
        }
    }

    static inline Float GetHalton(UInt index, UInt base)
    {
        AssertThrow(base != 0);

        Float f = 1.0f;
        Float r = 0.0f;
        UInt current = index;

        do {
            f = f / static_cast<Float>(base);
            r = r + f * (current % base);
            current = current / base;
        } while (current != 0);

        return r;
    }
};

struct alignas(64) TemporalAAUniforms
{
    ShaderMat4 jitter_matrix;
    ShaderMat4 view_matrix;
    ShaderMat4 projection_matrix;
};

Result TemporalAA::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result TemporalAA::ImageOutput::Destroy(Device *device)
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

TemporalAA::TemporalAA(const Extent2D &extent)
    : m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
              ImageType::TEXTURE_TYPE_2D
          ))
      }
{
}

TemporalAA::~TemporalAA() = default;

void TemporalAA::Create(Engine *engine)
{
    CreateImages(engine);
    CreateBuffers(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void TemporalAA::Destroy(Engine *engine)
{
    m_compute_taa.Reset();

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

void TemporalAA::CreateBuffers(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_uniform_buffers[frame_index].Construct();
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(m_uniform_buffers[frame_index]->Create(
                engine->GetDevice(),
                sizeof(TemporalAAUniforms)
            ));

            m_uniform_buffers[frame_index]->Memset(
                engine->GetDevice(),
                sizeof(TemporalAAUniforms),
                0x00
            );
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalAA::CreateImages(Engine *engine)
{
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (auto &image_output : m_image_outputs) {
            HYPERION_BUBBLE_ERRORS(image_output.Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalAA::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();
        
        // AA uniforms
        descriptor_set->GetOrAddDescriptor<UniformBufferDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_uniform_buffers[frame_index].Get()
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
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &m_image_outputs[frame_index].image_view
                });
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalAA::CreateComputePipelines(Engine *engine)
{
    m_compute_taa = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("TemporalAA")),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_compute_taa);
}

void TemporalAA::BuildJitterMatrix(const SceneDrawProxy &scene)
{
    static const HaltonSequence halton;

    const Vector2 pixel_size = Vector2::one / scene.camera.dimensions;
    const UInt index = scene.frame_counter % static_cast<UInt>(halton.sequence.Size());

    const Vector2 jitter = halton.sequence[index];

    m_jitter_matrix = scene.camera.projection;
    m_jitter_matrix[3][0] += jitter.x;
    m_jitter_matrix[3][1] += jitter.y;
}

void TemporalAA::Render(
    Engine *engine,
    Frame *frame
)
{
    const auto &scene = engine->GetRenderState().GetScene().scene;

    BuildJitterMatrix(scene);

    const TemporalAAUniforms uniforms {
        .jitter_matrix = m_jitter_matrix,
        .view_matrix = scene.camera.view,
        .projection_matrix = scene.camera.projection
    };

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(
        engine->GetDevice(),
        sizeof(uniforms),
        &uniforms
    );

    auto &prev_image = m_image_outputs[(frame->GetFrameIndex() + 1) % max_frames_in_flight].image;
    
    if (prev_image.GetGPUImage()->GetResourceState() != renderer::ResourceState::SHADER_RESOURCE) {
        prev_image.GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> dimension;
    } push_constants;

    push_constants.dimension = Extent2D(extent);

    m_compute_taa->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_compute_taa->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_compute_taa->GetPipeline()->Dispatch(
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