#include "probe_system.h"
#include "../engine.h"

#include <rendering/backend/renderer_shader.h>

/* TMP */
#include <asset/asset_manager.h>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ImageStorageDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::GPUMemory;

ProbeSystem::ProbeSystem(ProbeSystemSetup &&setup)
    : m_setup(std::move(setup)),
      m_time(0)
{
}

ProbeSystem::~ProbeSystem()
{
}

void ProbeSystem::Init(Engine *engine)
{
    const auto grid = m_setup.NumProbesPerDimension();
    
    m_probes.resize(m_setup.NumProbes());

    for (uint32_t x = 0; x < grid.width; x++) {
        for (uint32_t y = 0; y < grid.height; y++) {
            for (uint32_t z = 0; z < grid.depth; z++) {
                const uint32_t index = x * grid.height * grid.depth
                                     + y * grid.depth
                                     + z;

                m_probes[index] = Probe{
                    .position = (Vector3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) - (m_setup.probe_border.ToVector3() * 0.5f)) * m_setup.probe_distance
                };
            }
        }
    }

    CreateStorageImages(engine);
    CreateUniformBuffer(engine);
    AddDescriptors(engine);

    /* TMP */
    engine->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](Engine *engine) {
        CreatePipeline(engine);
    });
}

void ProbeSystem::CreatePipeline(Engine *engine)
{
    auto rt_shader = std::make_unique<ShaderProgram>();

    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_GEN, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/probe.rgen.spv").Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_MISS, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/probe.rmiss.spv").Read()
    });
    rt_shader->AttachShader(engine->GetDevice(), ShaderModule::Type::RAY_CLOSEST_HIT, {
        FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/rt/probe.rchit.spv").Read()
    });

    m_pipeline = std::make_unique<RaytracingPipeline>(std::move(rt_shader));
    HYPERION_ASSERT_RESULT(m_pipeline->Create(engine->GetDevice(), &engine->GetInstance()->GetDescriptorPool()));
}

void ProbeSystem::CreateUniformBuffer(Engine *engine)
{
    ProbeSystemUniforms uniforms{
        .aabb_max           = m_setup.aabb.max.ToVector4(),
        .aabb_min           = m_setup.aabb.min.ToVector4(),
        .probe_border       = m_setup.probe_border,
        .probe_counts       = m_setup.NumProbesPerDimension(),
        .probe_distance     = m_setup.probe_distance,
        .num_rays_per_probe = m_setup.num_rays_per_probe
    };

    m_uniform_buffer = std::make_unique<UniformBuffer>();
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create(engine->GetDevice(), sizeof(ProbeSystemUniforms)));

    m_uniform_buffer->Copy(engine->GetDevice(), sizeof(ProbeSystemUniforms), &uniforms);
}

void ProbeSystem::CreateStorageImages(Engine *engine)
{
    m_radiance_image = std::make_unique<StorageImage>(
        Extent3D({m_setup.NumProbes(), m_setup.num_rays_per_probe}),
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    HYPERION_ASSERT_RESULT(m_radiance_image->Create(engine->GetDevice()));

    m_radiance_image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(m_radiance_image_view->Create(engine->GetDevice(), m_radiance_image.get()));
}

void ProbeSystem::AddDescriptors(Engine *engine)
{
    /* TMP, have to set this up in a way such that it's not relying on something else to populate the other RT stuff */

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);
  
    auto *radiance_storage = descriptor_set->AddDescriptor<ImageStorageDescriptor>(5);
    radiance_storage->AddSubDescriptor({.image_view = m_radiance_image_view.get()});

    auto *probe_uniforms = descriptor_set->AddDescriptor<UniformBufferDescriptor>(9);
    probe_uniforms->AddSubDescriptor({.buffer = m_uniform_buffer.get()});
}

void ProbeSystem::SubmitPushConstants(Engine *engine, CommandBuffer *command_buffer)
{
    m_random_generator.Next();

    std::memcpy(
        m_pipeline->push_constants.probe_data.matrix,
        m_random_generator.matrix.values,
        std::size(m_pipeline->push_constants.probe_data.matrix) * sizeof(m_pipeline->push_constants.probe_data.matrix[0])
    );

    m_pipeline->push_constants.probe_data.time = m_time++;

    m_pipeline->SubmitPushConstants(command_buffer);
}

void ProbeSystem::RenderProbes(Engine *engine, CommandBuffer *command_buffer)
{
    m_radiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    SubmitPushConstants(engine, command_buffer);

    m_pipeline->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_pipeline.get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
            {.offsets = {0}}
        }
    );

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        m_pipeline.get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    m_pipeline->TraceRays(
        engine->GetDevice(),
        command_buffer,
        Extent3D({m_setup.NumProbes(), m_setup.num_rays_per_probe})
    );

    m_radiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );
}

} // namespace hyperion::v2