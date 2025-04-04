/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypData.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
static const double g_system_execution_group_lag_spike_threshold = 50.0;
static const uint32 g_entity_manager_command_queue_warning_size = 8192;

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION

#pragma region EntityManagerCommandQueue

EntityManagerCommandQueue::EntityManagerCommandQueue(EnumFlags<EntityManagerCommandQueueFlags> flags)
    : m_flags(flags)
{
}

void EntityManagerCommandQueue::AwaitEmpty()
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    while (m_count.Get(MemoryOrder::ACQUIRE)) {
        m_condition_variable.wait(guard);
    }
}

void EntityManagerCommandQueue::Push(EntityManagerCommandProc &&command)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    buffer.commands.Push(std::move(command));
    m_count.Increment(1, MemoryOrder::RELEASE);

    if (buffer.commands.Size() >= g_entity_manager_command_queue_warning_size) {
        HYP_LOG(ECS, Warning, "EntityManager command queue has grown past warning threshold size of ({} >= {}). This may indicate FlushCommandQueue() is not being called which would be a memory leak.",
            buffer.commands.Size(), g_entity_manager_command_queue_warning_size);

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif
    }
}

void EntityManagerCommandQueue::Execute(EntityManager &mgr, GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);

    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    if (!m_count.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    std::unique_lock<std::mutex> guard(buffer.mutex);

    // Swap the buffer index to allow new commands to be added to buffer while executing
    const uint32 next_buffer_index = (current_buffer_index + 1) % m_command_buffers.Size();

    m_buffer_index.Set(next_buffer_index, MemoryOrder::RELEASE);

    while (buffer.commands.Any()) {
        buffer.commands.Pop()(mgr, delta);
    }

    // Update count to be the number of commands in the next buffer (0 unless one of the commands added more commands to the queue)
    m_count.Set(uint32(m_command_buffers[next_buffer_index].commands.Size()), MemoryOrder::RELEASE);
}

#pragma endregion EntityManagerCommandQueue

#pragma region EntityToEntityManagerMap

Task<bool> EntityToEntityManagerMap::PerformActionWithEntity(ID<Entity> id, void(*callback)(EntityManager *entity_manager, ID<Entity> id))
{
    HYP_SCOPE;

    Task<bool> task;

    m_mutex.Lock();

    const auto it = m_map.FindAs(id);

    if (it == m_map.End()) {
        m_mutex.Unlock();

        task.Fulfill(false);

        return task;
    }

    EntityManager *entity_manager = it->second;

    auto Impl = [id, callback, task_executor = task.Initialize()](EntityManager &entity_manager, GameCounter::TickUnit delta)
    {
        AssertThrow(entity_manager.HasEntity(id));

        callback(&entity_manager, id);

        task_executor->Fulfill(true);
    };
    
    if (entity_manager->GetOwnerThreadMask() & Threads::CurrentThreadID().GetValue()) {
        // Mutex must not be locked when callback() is called - the callback could perform
        // actions that directly or undirectly require re-locking the mutex.
        // Instead, unlock the mutex once we are sure the EntityManager will not be deleted before using it
        // (either by ensuring we're on the same thread that owns the EntityManager, or by enqueuing the command)
        m_mutex.Unlock();

        Impl(*entity_manager, 0.0f);
    } else {
        entity_manager->PushCommand(Impl);

        m_mutex.Unlock();
    }

    return task;
}

#pragma endregion EntityToEntityManagerMap

#pragma region EntityManager

EntityToEntityManagerMap EntityManager::s_entity_to_entity_manager_map { };

EntityToEntityManagerMap &EntityManager::GetEntityToEntityManagerMap()
{
    return s_entity_to_entity_manager_map;
}

bool EntityManager::IsValidComponentType(TypeID component_type_id)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id) != nullptr;
}

bool EntityManager::IsEntityTagComponent(TypeID component_type_id)
{
    const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

    if (!component_interface) {
        return false;
    }

    return component_interface->IsEntityTag();
}

EntityManager::EntityManager(ThreadMask owner_thread_mask, Scene *scene, EnumFlags<EntityManagerFlags> flags)
    : m_owner_thread_mask(owner_thread_mask),
      m_world(scene != nullptr ? scene->GetWorld() : nullptr),
      m_scene(scene),
      m_flags(flags),
      m_command_queue(Threads::IsThreadInMask(g_game_thread, owner_thread_mask) ? EntityManagerCommandQueueFlags::EXEC_COMMANDS : EntityManagerCommandQueueFlags::NONE),
      m_root_synchronous_execution_group(nullptr),
      m_is_initialized(false)
{
    AssertThrow(scene != nullptr);

    // add initial component containers
    for (const IComponentInterface *component_interface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces()) {
        AssertThrow(component_interface != nullptr);

        ComponentContainerFactoryBase *component_container_factory = component_interface->GetComponentContainerFactory();
        AssertThrow(component_container_factory != nullptr);

        UniquePtr<ComponentContainerBase> component_container = component_container_factory->Create();
        AssertThrow(component_container != nullptr);

        m_containers.Set(component_interface->GetTypeID(), std::move(component_container));
    }
}

EntityManager::~EntityManager()
{
    GetEntityToEntityManagerMap().RemoveEntityManager(this);
}

void EntityManager::Initialize()
{
    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_READ(m_entities_data_race_detector);

    AssertThrow(!m_is_initialized);

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            for (auto entities_it = m_entities.Begin(); entities_it != m_entities.End(); ++entities_it) {
                const Handle<Entity> entity = entities_it->first.Lock();

                if (!entity.IsValid()) {
                    HYP_LOG(ECS, Warning, "Entity with ID #{} is expired or invalid", entities_it->first.GetID().Value());

                    continue;
                }

                EntityData &entity_data = entities_it->second;

                const TypeMap<ComponentID> &component_ids = entity_data.components;

                if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                    { // critical section
                        Mutex::Guard guard(m_system_entity_map_mutex);

                        auto system_entity_it = m_system_entity_map.Find(system_it.second.Get());

                        // Check if the system already has this entity initialized
                        if (system_entity_it != m_system_entity_map.End() && (system_entity_it->second.FindAs(entity) != system_entity_it->second.End())) {
                            continue;
                        }

                        m_system_entity_map[system_it.second.Get()].Insert(entity);
                    }

                    system_it.second->OnEntityAdded(entity);
                }
            }
        }
    }

    m_is_initialized = true;
}

void EntityManager::SetWorld(World *world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (world == m_world) {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    if (m_is_initialized) {
        for (SystemExecutionGroup &group : m_system_execution_groups) {
            for (auto &system_it : group.GetSystems()) {
                system_it.second->SetWorld(world);
            }
        }
    }

    m_world = world;
}

Handle<Entity> EntityManager::AddEntity()
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);
        
    ObjectContainer<Entity> &container = ObjectPool::GetObjectContainerHolder().GetOrCreate<Entity>();
    
    HypObjectMemory<Entity> *memory = container.Allocate();
    memory->Construct();
    
    Handle<Entity> entity { memory };

    GetEntityToEntityManagerMap().Add(entity.GetID(), this);

    m_entities.AddEntity(entity);

    return entity;
}

bool EntityManager::RemoveEntity(ID<Entity> id)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!id.IsValid()) {
        return false;
    }

    auto entities_it = m_entities.Find(id);

    if (entities_it == m_entities.End()) {
        return false;
    }

    EntityData &entity_data = entities_it->second;

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(id, entity_data.components);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (auto component_info_pair_it = entity_data.components.Begin(); component_info_pair_it != entity_data.components.End();) {
        const TypeID component_type_id = component_info_pair_it->first;
        const ComponentID component_id = component_info_pair_it->second;

        auto component_container_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        AssertThrow(component_container_it->second->RemoveComponent(component_id));

        // Erase the component from the entity's component map,
        // advance the iterator
        component_info_pair_it = entity_data.components.Erase(component_info_pair_it);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            // For each entity set that can contain this component type, update the entity set
            for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);
                
                entity_set.OnEntityUpdated(id);
            }
        }
    }
    
    GetEntityToEntityManagerMap().Remove(id);

    m_entities.Erase(entities_it);

    return true;
}

void EntityManager::MoveEntity(const Handle<Entity> &entity, EntityManager &other)
{
    HYP_SCOPE;

    if (std::addressof(*this) == std::addressof(other)) {
        return;
    }

    HYP_MT_CHECK_RW(m_entities_data_race_detector);
    HYP_MT_CHECK_RW(other.m_entities_data_race_detector);

    // we could be on either entity manager owner thread
    // @TODO Use appropriate mutexes to protect the entity sets
    Threads::AssertOnThread(m_owner_thread_mask | other.m_owner_thread_mask);

    AssertThrow(entity.IsValid());

    const auto entities_it = m_entities.Find(entity);
    AssertThrowMsg(entities_it != m_entities.End(), "Entity does not exist");

    EntityData &entity_data = entities_it->second;

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(entity, entity_data.components);

    other.m_entities.AddEntity(entity);
    
    GetEntityToEntityManagerMap().Remap(entity, &other);

    const auto other_entities_it = other.m_entities.Find(entity);
    AssertThrow(other_entities_it != other.m_entities.End());

    EntityData &other_entity_data = other_entities_it->second;

    TypeMap<ComponentID> new_component_ids;

    { // Critical section
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);
        Mutex::Guard other_entity_sets_guard(other.m_entity_sets_mutex);

        for (auto component_info_pair_it = entity_data.components.Begin(); component_info_pair_it != entity_data.components.End();) {
            const TypeID component_type_id = component_info_pair_it->first;
            const ComponentID component_id = component_info_pair_it->second;

            auto component_container_it = m_containers.Find(component_type_id);
            AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
            AssertThrowMsg(component_container_it->second->HasComponent(component_id), "Component does not exist in component container");

            auto other_component_container_it = other.m_containers.Find(component_type_id);

            if (other_component_container_it == other.m_containers.End()) {
                auto insert_result = other.m_containers.Set(component_type_id, component_container_it->second->GetFactory()->Create());
                AssertThrowMsg(insert_result.second, "Failed to insert component container");

                other_component_container_it = insert_result.first;
            }

            Optional<ComponentID> new_component_id = component_container_it->second->MoveComponent(component_id, *other_component_container_it->second);
            AssertThrowMsg(new_component_id.HasValue(), "Failed to move component");

            new_component_ids.Set(component_type_id, *new_component_id);

            other_entity_data.components.Set(component_type_id, *new_component_id);

            // Update iterator, erase the component from the entity's component map
            component_info_pair_it = entity_data.components.Erase(component_info_pair_it);

            // Update our entity sets to reflect the change
            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                    EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

                    entity_set.OnEntityUpdated(entity);
                }
            }

            // Update other's entity sets to reflect the change
            auto other_component_entity_sets_it = other.m_component_entity_sets.Find(component_type_id);

            if (other_component_entity_sets_it != other.m_component_entity_sets.End()) {
                for (TypeID entity_set_type_id : other_component_entity_sets_it->second) {
                    EntitySetBase &entity_set = *other.m_entity_sets.At(entity_set_type_id);

                    entity_set.OnEntityUpdated(entity);
                }
            }
        }

        m_entities.Erase(entities_it);
    }

    // Notify systems of the entity being added to the other EntityManager
    other.NotifySystemsOfEntityAdded(entity, new_component_ids);
}

void EntityManager::AddComponent(ID<Entity> entity_id, AnyRef component)
{
    const TypeID component_type_id = component.GetTypeID();

    EnsureValidComponentType(component_type_id);

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_READ(m_entities_data_race_detector);

    AssertThrowMsg(entity_id.IsValid(), "Invalid entity ID");

    EntityData &entity_data = m_entities.GetEntityData(entity_id);

    TypeMap<ComponentID> component_ids;

    // Update the EntityData
    auto component_it = entity_data.FindComponent(component_type_id);
    // @TODO: Replace the component if it already exists
    AssertThrowMsg(component_it == entity_data.components.End(), "Entity already has component of type %u", component_type_id.Value());

    ComponentContainerBase *container = TryGetContainer(component_type_id);
    AssertThrowMsg(container != nullptr, "Component container does not exist for component type %u", component_type_id.Value());

    const ComponentID component_id = container->AddComponent(component);
    entity_data.components.Set(component.GetTypeID(), component_id);

    { // Lock the entity sets mutex
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

        // Update entity sets
        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

                entity_set.OnEntityUpdated(entity_id);
            }
        }
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entity_id, entity_data.components);
}

bool EntityManager::RemoveComponent(TypeID component_type_id, ID<Entity> entity_id)
{
    HYP_SCOPE;

    EnsureValidComponentType(component_type_id);

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_READ(m_entities_data_race_detector);

    if (!entity_id.IsValid()) {
        return false;
    }

    EntityData &entity_data = m_entities.GetEntityData(entity_id);

    auto component_it = entity_data.FindComponent(component_type_id);
    if (component_it == entity_data.components.End()) {
        return false;
    }

    const ComponentID component_id = component_it->second;

    // Notify systems that entity is being removed from them
    TypeMap<ComponentID> removed_component_ids;
    removed_component_ids.Set(component_type_id, component_id);
    
    NotifySystemsOfEntityRemoved(entity_id, removed_component_ids);
    
    ComponentContainerBase *container = TryGetContainer(component_type_id);

    if (!container) {
        return false;
    }

    if (!container->RemoveComponent(component_id)) {
        return false;
    }

    entity_data.components.Erase(component_it);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

    if (component_entity_sets_it != m_component_entity_sets.End()) {
        for (TypeID entity_set_type_id : component_entity_sets_it->second) {
            EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

            entity_set.OnEntityUpdated(entity_id);
        }
    }

    return true;
}

bool EntityManager::HasTag(ID<Entity> entity_id, EntityTag tag) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (!entity_id.IsValid()) {
        return false;
    }

    const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface) {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    return HasComponent(component_type_id, entity_id);
}

void EntityManager::AddTag(ID<Entity> entity_id, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (!entity_id.IsValid()) {
        return;
    }

    const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface) {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    if (HasComponent(component_type_id, entity_id)) {
        return;
    }

    ComponentContainerBase *container = TryGetContainer(component_type_id);
    AssertThrowMsg(container != nullptr, "Component container does not exist for component type %u", component_type_id.Value());

    HypData component_hyp_data;

    if (!component_interface->CreateInstance(component_hyp_data)) {
        HYP_LOG(ECS, Error, "Failed to create EntityTagComponent for EntityTag {}", tag);

        return;
    }

    AddComponent(entity_id, component_hyp_data.ToRef());

    /*const ComponentID component_id = container->AddComponent(component_hyp_data.ToRef());

    EntityData &entity_data = m_entities.GetEntityData(entity_id);
    entity_data.components.Set(component_type_id, component_id);

    { // Lock the entity sets mutex
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

        // Update entity sets
        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

                entity_set.OnEntityUpdated(entity_id);
            }
        }
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entity_id, entity_data.components);*/
}

bool EntityManager::RemoveTag(ID<Entity> entity_id, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (!entity_id.IsValid()) {
        return false;
    }

    const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface) {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    return RemoveComponent(component_type_id, entity_id);
}

void EntityManager::NotifySystemsOfEntityAdded(ID<Entity> entity_id, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!entity_id.IsValid()) {
        return;
    }

    // If the EntityManager is initialized, notify systems of the entity being added
    // otherwise, the systems will be notified when the EntityManager is initialized
    if (!m_is_initialized) {
        return;
    }

    Handle<Entity> entity { entity_id };

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                { // critical section
                    Mutex::Guard guard(m_system_entity_map_mutex);

                    auto system_entity_it = m_system_entity_map.Find(system_it.second.Get());

                    // Check if the system already has this entity initialized
                    if (system_entity_it != m_system_entity_map.End() && (system_entity_it->second.FindAs(entity) != system_entity_it->second.End())) {
                        continue;
                    }

                    m_system_entity_map[system_it.second.Get()].Insert(entity);
                }

                system_it.second->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(ID<Entity> entity, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!entity.IsValid()) {
        return;
    }

    if (!m_is_initialized) {
        return;
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                { // critical section
                    Mutex::Guard guard(m_system_entity_map_mutex);

                    auto system_entity_it = m_system_entity_map.Find(system_it.second.Get());

                    if (system_entity_it == m_system_entity_map.End()) {
                        continue;
                    }

                    auto entity_it = system_entity_it->second.FindAs(entity);

                    if (entity_it == system_entity_it->second.End()) {
                        continue;
                    }

                    system_entity_it->second.Erase(entity_it);
                }

                system_it.second->OnEntityRemoved(entity);
            }
        }
    }
}

void EntityManager::BeginAsyncUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);

    m_root_synchronous_execution_group = nullptr;
    
    TaskBatch *root_task_batch = nullptr;
    TaskBatch *last_task_batch = nullptr;

    // Prepare task dependencies
    for (SizeType index = 0; index < m_system_execution_groups.Size(); index++) {
        SystemExecutionGroup &system_execution_group = m_system_execution_groups[index];

        TaskBatch *current_task_batch = system_execution_group.GetTaskBatch();

        current_task_batch->ResetState();

        // Add tasks to batches before kickoff
        system_execution_group.StartProcessing(delta);

        if (system_execution_group.RequiresGameThread()) {
            if (m_root_synchronous_execution_group != nullptr) {
                m_root_synchronous_execution_group->GetTaskBatch()->next_batch = current_task_batch;
            } else {
                m_root_synchronous_execution_group = &system_execution_group;
            }

            continue;
        }

        if (!root_task_batch) {
            root_task_batch = current_task_batch;
        }

        if (last_task_batch != nullptr) {
            if (current_task_batch->num_enqueued > 0) {
                last_task_batch->next_batch = current_task_batch;
                last_task_batch = current_task_batch;
            }
        } else {
            last_task_batch = current_task_batch;
        }
    }

    // Kickoff first task
    if (root_task_batch != nullptr && (root_task_batch->num_enqueued > 0 || root_task_batch->next_batch != nullptr)) {
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
        TaskSystem::GetInstance().EnqueueBatch(root_task_batch);
#endif
    }
}

void EntityManager::EndAsyncUpdate()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);

    for (SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
        if (system_execution_group.RequiresGameThread()) {
            continue;
        }

        system_execution_group.FinishProcessing();
    }

    if (m_root_synchronous_execution_group != nullptr) {
        m_root_synchronous_execution_group->FinishProcessing(/* execute_blocking */ true);
        
        m_root_synchronous_execution_group = nullptr;
    }

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
    for (SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
        const PerformanceClock &performance_clock = system_execution_group.GetPerformanceClock();
        const double elapsed_time_ms = performance_clock.Elapsed() / 1000.0;

        if (elapsed_time_ms >= g_system_execution_group_lag_spike_threshold) {
            HYP_LOG(ECS, Warning, "SystemExecutionGroup spike detected: {} ms", elapsed_time_ms);

            for (const auto &it : system_execution_group.GetPerformanceClocks()) {
                HYP_LOG(ECS, Debug, "\tSystem {} performance: {}", it.first->GetName(), it.second.Elapsed() / 1000.0);
            }
        }
    }
#endif
}

void EntityManager::FlushCommandQueue(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);
    
    if (m_command_queue.HasUpdates()) {
        m_command_queue.Execute(*this, delta);
    }
}

bool EntityManager::IsEntityInitializedForSystem(SystemBase *system, ID<Entity> entity_id) const
{
    HYP_SCOPE;

    if (!system) {
        return false;
    }

    Mutex::Guard guard(m_system_entity_map_mutex);

    const auto it = m_system_entity_map.Find(system);

    if (it == m_system_entity_map.End()) {
        return false;
    }

    return it->second.FindAs(entity_id) != it->second.End();
}

#pragma endregion EntityManager

#pragma region SystemExecutionGroup

SystemExecutionGroup::SystemExecutionGroup(bool requires_game_thread)
    : m_requires_game_thread(requires_game_thread),
      m_task_batch(MakeUnique<TaskBatch>())
{
}

SystemExecutionGroup::~SystemExecutionGroup()
{
}

void SystemExecutionGroup::StartProcessing(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
    m_performance_clock.Start();

    for (auto &it : m_systems) {
        SystemBase *system = it.second.Get();

        m_performance_clocks.Set(system, PerformanceClock());
    }
#endif

    for (auto &it : m_systems) {
        SystemBase *system = it.second.Get();

        StaticMessage debug_name;
        debug_name.value = system->GetName();

        m_task_batch->AddTask([this, system, delta]
        {
            HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
            PerformanceClock &performance_clock = m_performance_clocks[system];
            performance_clock.Start();
#endif

            system->Process(delta);

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
            performance_clock.Stop();
#endif
        }, debug_name);
    }
}

void SystemExecutionGroup::FinishProcessing(bool execute_blocking)
{
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
    if (execute_blocking) {
        m_task_batch->ExecuteBlocking(/* execute_dependent_batches */ true);
    } else {
        m_task_batch->AwaitCompletion();
    }
#else
    m_task_batch->ExecuteBlocking(/* execute_dependent_batches */ true);
#endif

    for (auto &it : m_systems) {
        SystemBase *system = it.second.Get();

        if (system->m_after_process_procs.Any()) {
            for (auto &proc : system->m_after_process_procs) {
                proc();
            }

            system->m_after_process_procs.Clear();
        }
    }

#ifdef HYP_DEBUG_MODE
    m_performance_clock.Stop();
#endif
}

#pragma endregion SystemExecutionGroup

} // namespace hyperion
