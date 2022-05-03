#include "probe_system.h"
#include "../engine.h"

#include <rendering/backend/renderer_shader.h>

/* TMP */
#include <asset/asset_manager.h>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ImageStorageDescriptor;
using renderer::StorageBufferDescriptor;
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

    CreateStorageBuffers(engine);
    CreateUniformBuffer(engine);
    AddDescriptors(engine);

    /* TMP */
    engine->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](Engine *engine) {
        CreatePipeline(engine);
    });
    
    CreateComputePipelines(engine);
}

void ProbeSystem::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    Device *device = engine->GetDevice();

    HYPERION_PASS_ERRORS(m_uniform_buffer->Destroy(device), result);
    
    HYPERION_PASS_ERRORS(m_radiance_buffer->Destroy(device), result);

    HYPERION_PASS_ERRORS(m_irradiance_image->Destroy(device), result);
    HYPERION_PASS_ERRORS(m_irradiance_image_view->Destroy(device), result);
    
    HYPERION_PASS_ERRORS(m_depth_image->Destroy(device), result);
    HYPERION_PASS_ERRORS(m_depth_image_view->Destroy(device), result);

    HYPERION_PASS_ERRORS(m_pipeline->Destroy(device), result);

    HYPERION_ASSERT_RESULT(result);
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

void ProbeSystem::CreateComputePipelines(Engine *engine)
{
    m_update_irradiance = engine->resources.compute_pipelines.Add(engine, std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/rt/probe_update_irradiance.comp.spv").Read()}}
            }
        ))
    ));

    m_update_depth = engine->resources.compute_pipelines.Add(engine, std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/rt/probe_update_depth.comp.spv").Read()}}
            }
        ))
    ));
}

void ProbeSystem::CreateUniformBuffer(Engine *engine)
{
    ProbeSystemUniforms uniforms{
        .aabb_max           = m_setup.aabb.max.ToVector4(),
        .aabb_min           = m_setup.aabb.min.ToVector4(),
        .probe_border       = m_setup.probe_border,
        .probe_counts       = m_setup.NumProbesPerDimension(),
        .image_dimensions   = m_setup.GetImageDimensions(),
        .probe_distance     = m_setup.probe_distance,
        .num_rays_per_probe = m_setup.num_rays_per_probe
    };

    m_uniform_buffer = std::make_unique<UniformBuffer>();
    HYPERION_ASSERT_RESULT(m_uniform_buffer->Create(engine->GetDevice(), sizeof(ProbeSystemUniforms)));

    m_uniform_buffer->Copy(engine->GetDevice(), sizeof(ProbeSystemUniforms), &uniforms);
}

void ProbeSystem::CreateStorageBuffers(Engine *engine)
{
    const auto probe_counts = m_setup.NumProbesPerDimension();

    m_radiance_buffer = std::make_unique<StorageBuffer>();
    HYPERION_ASSERT_RESULT(m_radiance_buffer->Create(engine->GetDevice(), m_setup.GetImageDimensions().Size() * sizeof(ProbeRayData)));

    m_irradiance_image = std::make_unique<StorageImage>(
        Extent3D{{(m_setup.irradiance_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2, (m_setup.irradiance_octahedron_size + 2) * probe_counts.depth + 2}},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );
    HYPERION_ASSERT_RESULT(m_irradiance_image->Create(engine->GetDevice()));

    m_irradiance_image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(m_irradiance_image_view->Create(engine->GetDevice(), m_irradiance_image.get()));

    m_depth_image = std::make_unique<StorageImage>(
        Extent3D{{(m_setup.depth_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2, (m_setup.depth_octahedron_size + 2) * probe_counts.depth + 2}},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );
    HYPERION_ASSERT_RESULT(m_depth_image->Create(engine->GetDevice()));

    m_depth_image_view = std::make_unique<ImageView>();
    HYPERION_ASSERT_RESULT(m_depth_image_view->Create(engine->GetDevice(), m_depth_image.get()));
}

void ProbeSystem::AddDescriptors(Engine *engine)
{
    /* TMP, have to set this up in a way such that it's not relying on something else to populate the other RT stuff */

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::Index::DESCRIPTOR_SET_INDEX_RAYTRACING);

    auto *probe_uniforms = descriptor_set->AddDescriptor<UniformBufferDescriptor>(9);
    probe_uniforms->AddSubDescriptor({.buffer = m_uniform_buffer.get()});

    auto *probe_ray_data = descriptor_set->AddDescriptor<StorageBufferDescriptor>(10);
    probe_ray_data->AddSubDescriptor({.buffer = m_radiance_buffer.get()});

    auto *irradiance_image_descriptor = descriptor_set->AddDescriptor<ImageStorageDescriptor>(11);
    irradiance_image_descriptor->AddSubDescriptor({.image_view = m_irradiance_image_view.get()});

    auto *irradiance_depth_image_descriptor = descriptor_set->AddDescriptor<ImageStorageDescriptor>(12);
    irradiance_depth_image_descriptor->AddSubDescriptor({.image_view = m_depth_image_view.get()});
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
    m_radiance_buffer->InsertBarrier(command_buffer, GPUMemory::ResourceState::UNORDERED_ACCESS);

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
        Extent3D(m_setup.GetImageDimensions())
    );

    m_radiance_buffer->InsertBarrier(command_buffer, GPUMemory::ResourceState::UNORDERED_ACCESS);
}

void ProbeSystem:: ComputeIrradiance(Engine *engine, CommandBuffer *command_buffer)
{
    const auto probe_counts = m_setup.NumProbesPerDimension();

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    auto &update_irradiance = *engine->resources.compute_pipelines[m_update_irradiance],
         &update_depth      = *engine->resources.compute_pipelines[m_update_depth];

    update_irradiance->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        &update_irradiance.Get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    update_irradiance->Dispatch(command_buffer, Extent3D{{probe_counts.width * probe_counts.height, probe_counts.depth}});

    update_depth->Bind(command_buffer);
    
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        &update_depth.Get(),
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING, .count = 1}
        }
    );

    update_depth->Dispatch(command_buffer, Extent3D{{probe_counts.width * probe_counts.height, probe_counts.depth}});

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        command_buffer,
        GPUMemory::ResourceState::UNORDERED_ACCESS
    );
}

} // namespace hyperion::v2