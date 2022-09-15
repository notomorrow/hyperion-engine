#ifndef HYPERION_V2_PARTICLE_SYSTEM_HPP
#define HYPERION_V2_PARTICLE_SYSTEM_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <Threads.hpp>

#include <math/Vector3.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <rendering/Mesh.hpp>

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

template <class T>
class ThreadSafeContainer
{
public:
    using Iterator = typename DynArray<Handle<T>>::Iterator;
    using ConstIterator = typename DynArray<Handle<T>>::ConstIterator;

    ThreadSafeContainer(ThreadName owner_thread)
        : m_owner_thread(owner_thread)
    {
    }

    ThreadSafeContainer(const ThreadSafeContainer &other) = delete;
    ThreadSafeContainer &operator=(const ThreadSafeContainer &other) = delete;
    ThreadSafeContainer(ThreadSafeContainer &&other) noexcept = delete;
    ThreadSafeContainer &operator=(ThreadSafeContainer &&other) noexcept = delete;

    ~ThreadSafeContainer()
    {
        Clear(false);
    }

    void Add(const Handle<T> &item)
    {
        if (!item) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(item->GetID());
        m_items_pending_addition.PushBack(item);

        m_updates_pending.store(true);
    }

    void Add(Handle<T> &&item)
    {
        if (!item) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        m_items_pending_removal.Erase(item->GetID());
        m_items_pending_addition.PushBack(std::move(item));

        m_updates_pending.store(true);
    }

    void Remove(typename Handle<T>::ID id)
    {
        if (!id) {
            return;
        }

        std::lock_guard guard(m_update_mutex);

        auto it = m_items_pending_addition.FindIf([&id](const auto &item) {
            return item->GetID() == id;
        });

        if (it != m_items_pending_addition.End()) {
            m_items_pending_addition.Erase(it);
        }

        m_items_pending_removal.PushBack(id);

        m_updates_pending.store(true);
    }

    bool HasUpdatesPending() const
        { return m_updates_pending.load(); }

    /*! \brief Adds and removes all pending items to be added or removed.
        Only call from the owner thread.
        @param engine A pointer to the Engine object. Used to initialize any newly objects. */
    void UpdateItems(Engine *engine)
    {
        Threads::AssertOnThread(m_owner_thread);

        m_update_mutex.lock();

        auto pending_removal = std::move(m_items_pending_removal);
        auto pending_addition = std::move(m_items_pending_addition);

        m_updates_pending.store(false);

        m_update_mutex.unlock();

        // do most of the work outside of the lock.
        // due to the std::move() usage, m_items_pending_removal and m_items_pending_addition
        // will be cleared, we've taken all items and will work off them here.

        while (pending_removal.Any()) {
            auto front = pending_removal.PopFront();

            auto it = m_owned_items.FindIf([&front](const auto &item) {
                return item->GetID() == front;
            });

            if (it != m_owned_items.End()) {
                m_owned_items.Erase(it);
            }
        }

        while (pending_addition.Any()) {
            auto front = pending_addition.PopFront();

            auto it = m_owned_items.FindIf([&front](const auto &item) {
                return item->GetID() == front;
            });

            if (it == m_owned_items.End()) {
                // engine->InitObject(front);

                m_owned_items.PushBack(std::move(front));
            }
        }
    }

    void Clear(bool check_thread_id = true)
    {
        if (check_thread_id) {
            Threads::AssertOnThread(m_owner_thread);
        }

        if (HasUpdatesPending()) {
            std::lock_guard guard(m_update_mutex);

            m_items_pending_removal.Clear();
            m_items_pending_addition.Clear();
        }

        m_owned_items.Clear();
    }

    /*! Only use from the owner thread! */
    DynArray<Handle<T>> &GetItems()
        { return m_owned_items; }
    
    /*! Only use from the owner thread! */
    const DynArray<Handle<T>> &GetItems() const
        { return m_owned_items; }

    /*! Only iterate it on the owner thread! */
    HYP_DEF_STL_ITERATOR(m_owned_items);

private:
    ThreadName m_owner_thread;
    DynArray<Handle<T>> m_owned_items;
    DynArray<Handle<T>> m_items_pending_addition;
    DynArray<typename Handle<T>::ID> m_items_pending_removal;
    std::atomic_bool m_updates_pending;
    std::mutex m_update_mutex;
};

struct ParticleSpawnerParams
{
    SizeType max_particles;
    Vector3 origin;
    Float origin_radius;
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

    void Init(Engine *engine);
    void Record(Engine *engine, CommandBuffer *command_buffer);

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

    void Init(Engine *engine);

    // called in render thread, updates particles using compute shader
    void UpdateParticles(Engine *engine, Frame *frame);

    void Render(Engine *engine, Frame *frame);

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