#ifndef HYPERION_V2_PARTICLE_SYSTEM_HPP
#define HYPERION_V2_PARTICLE_SYSTEM_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <Threads.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <atomic>
#include <mutex>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::IndirectBuffer;
using renderer::Frame;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::Result;

class Engine;

struct ParticleSpawnerParams
{
    Handle<Texture> texture;
    SizeType max_particles = 256u;
    Vector3 origin = Vector3::zero;
    Float radius = 5.0f;
    Float randomness = 0.5f;
    Float lifespan = 1.0f;
};

class ParticleSpawner
    : public EngineComponentBase<STUB_CLASS(ParticleSpawner)>
{
public:
    ParticleSpawner();
    ParticleSpawner(const ParticleSpawnerParams &params);
    ParticleSpawner(const ParticleSpawner &other) = delete;
    ParticleSpawner &operator=(const ParticleSpawner &other) = delete;
    ~ParticleSpawner();

    const ParticleSpawnerParams &GetParams() const
        { return m_params; }

    const FixedArray<DescriptorSet, max_frames_in_flight> &GetDescriptorSets() const
        { return m_descriptor_sets; }

    UniquePtr<StorageBuffer> &GetParticleBuffer()
        { return m_particle_buffer; }

    const UniquePtr<StorageBuffer> &GetParticleBuffer() const
        { return m_particle_buffer; }

    UniquePtr<IndirectBuffer> &GetIndirectBuffer()
        { return m_indirect_buffer; }

    const UniquePtr<IndirectBuffer> &GetIndirectBuffer() const
        { return m_indirect_buffer; }

    Handle<RendererInstance> &GetRendererInstance()
        { return m_renderer_instance; }

    const Handle<RendererInstance> &GetRendererInstance() const
        { return m_renderer_instance; }

    Handle<ComputePipeline> &GetComputePipeline()
        { return m_update_particles; }

    const Handle<ComputePipeline> &GetComputePipeline() const
        { return m_update_particles; }

    BoundingBox GetEstimatedAABB() const
    {
        return BoundingBox(
            m_params.origin - m_params.radius,
            m_params.origin + m_params.radius
        );
    }

    void Init();
    void Record(CommandBuffer *command_buffer);

private:
    void CreateNoiseMap();
    void CreateBuffers();
    void CreateShader();
    void CreateDescriptorSets();
    void CreateRendererInstance();
    void CreateComputePipelines();

    ParticleSpawnerParams m_params;
    FixedArray<DescriptorSet, max_frames_in_flight> m_descriptor_sets;
    UniquePtr<StorageBuffer> m_particle_buffer;
    UniquePtr<IndirectBuffer> m_indirect_buffer;
    UniquePtr<StorageBuffer> m_noise_buffer;
    Handle<ComputePipeline> m_update_particles;
    Handle<Shader> m_shader;
    Handle<RendererInstance> m_renderer_instance;
    Bitmap<1> m_noise_map;
};

class ParticleSystem
    : public EngineComponentBase<STUB_CLASS(ParticleSystem)>
{
public:
    ParticleSystem();
    ParticleSystem(const ParticleSystem &other) = delete;
    ParticleSystem &operator=(const ParticleSystem &other) = delete;
    ~ParticleSystem();

    ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners()
        { return m_particle_spawners; }

    const ThreadSafeContainer<ParticleSpawner> &GetParticleSpawners() const
        { return m_particle_spawners; }

    void Init();

    // called in render thread, updates particles using compute shader
    void UpdateParticles(Frame *frame);

    void Render(Frame *frame);

private:
    void CreateBuffers();
    void CreateCommandBuffers();

    Handle<Mesh> m_quad_mesh;

    // for zeroing out data
    StagingBuffer m_staging_buffer;

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording. size will never change once created
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> m_command_buffers;

    ThreadSafeContainer<ParticleSpawner> m_particle_spawners;

    UInt32 m_counter;
};

} // namespace hyperion::v2

#endif