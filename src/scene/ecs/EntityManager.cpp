/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>
#include <scene/Entity.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

// #define HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
#define HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PARALLEL

// if the number of systems in a group is less than this value, they will be executed sequentially
static const uint32 systems_execution_parallel_threshold = 3;

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
    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    buffer.commands.Push(std::move(command));
    m_count.Increment(1, MemoryOrder::RELEASE);
}

void EntityManagerCommandQueue::Execute(EntityManager &mgr, GameCounter::TickUnit delta)
{
    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);

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

#pragma region EntityManager

EntityToEntityManagerMap EntityManager::s_entity_to_entity_manager_map { };

EntityToEntityManagerMap &EntityManager::GetEntityToEntityManagerMap()
{
    return s_entity_to_entity_manager_map;
}

EntityManager::EntityManager(ThreadMask owner_thread_mask, Scene *scene)
    : m_owner_thread_mask(owner_thread_mask),
      m_scene(scene),
      m_command_queue(
          (owner_thread_mask & ThreadName::THREAD_GAME)
              ? ENTITY_MANAGER_COMMAND_QUEUE_POLICY_EXEC_ON_OWNER_THREAD
              : ENTITY_MANAGER_COMMAND_QUEUE_POLICY_DISCARD // discard commands if not on the game thread
      )
{
    AssertThrow(scene != nullptr);

    // add initial component containers
    for (ComponentInterfaceBase *component_interface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces()) {
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

ID<Entity> EntityManager::AddEntity()
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);
    
    const uint32 index = Handle<Entity>::GetContainer().NextIndex();
    const ID<Entity> entity = ID<Entity>::FromIndex(index);

    GetEntityToEntityManagerMap().Add(entity, this);

    return m_entities.AddEntity(Handle<Entity> { entity });
}

bool EntityManager::RemoveEntity(ID<Entity> entity)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (!entity.IsValid()) {
        return false;
    }

    auto it = m_entities.Find(entity);

    if (it == m_entities.End()) {
        return false;
    }

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(entity, it->second.components);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (auto component_info_pair_it = it->second.components.Begin(); component_info_pair_it != it->second.components.End();) {
        const TypeID component_type_id = component_info_pair_it->first;
        const ComponentID component_id = component_info_pair_it->second;

        auto component_container_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        AssertThrow(component_container_it->second->RemoveComponent(component_id));

        // Erase the component from the entity's component map,
        // advance the iterator
        component_info_pair_it = it->second.components.Erase(component_info_pair_it);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            // For each entity set that can contain this component type, update the entity set
            for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                auto entity_set_it = m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(entity);
            }
        }
    }
    
    GetEntityToEntityManagerMap().Remove(entity);

    m_entities.Erase(it);

    return true;
}

void EntityManager::MoveEntity(ID<Entity> entity, EntityManager &other)
{
    HYP_SCOPE;

    /// @TODO: Ensure that it is thread-safe to move an entity from one EntityManager to another
    // As the other EntityManager may be owned by a different thread.

    if (std::addressof(*this) == std::addressof(other)) {
        return;
    }

    // Threads::AssertOnThread(m_owner_thread_mask & other.m_owner_thread_mask);

    AssertThrow(entity.IsValid());

    // Threads::AssertOnThread(m_owner_thread_mask);

    const auto entity_it = m_entities.Find(entity);
    AssertThrowMsg(entity_it != m_entities.End(), "Entity does not exist");

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(entity, entity_it->second.components);

    const EntityData &entity_data = entity_it->second;

    other.m_entities.AddEntity(entity_it->first, EntityData { entity_data.handle });

    GetEntityToEntityManagerMap().Remap(entity, &other);

    const auto other_entity_it = other.m_entities.Find(entity);
    AssertThrow(other_entity_it != other.m_entities.End());

    TypeMap<ComponentID> new_component_ids;

    { // Critical section
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);
        Mutex::Guard other_entity_sets_guard(other.m_entity_sets_mutex);

        for (auto component_info_pair_it = entity_it->second.components.Begin(); component_info_pair_it != entity_it->second.components.End();) {
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

            other_entity_it->second.components.Set(component_type_id, *new_component_id);

            // Update iterator, erase the component from the entity's component map
            component_info_pair_it = entity_it->second.components.Erase(component_info_pair_it);

            // Update our entity sets to reflect the change
            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                    auto entity_set_it = m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }

            // Update other's entity sets to reflect the change
            auto other_component_entity_sets_it = other.m_component_entity_sets.Find(component_type_id);

            if (other_component_entity_sets_it != other.m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : other_component_entity_sets_it->second) {
                    auto entity_set_it = other.m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != other.m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }
        }

        m_entities.Erase(entity_it);
    }

    // Notify systems of the entity being added to the other EntityManager
    other.NotifySystemsOfEntityAdded(entity, new_component_ids);
}

void EntityManager::NotifySystemsOfEntityAdded(ID<Entity> entity, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;

    AssertThrow(entity.IsValid());

    Array<TypeID> component_type_ids;
    component_type_ids.Reserve(component_ids.Size());

    for (const auto &pair : component_ids) {
        component_type_ids.PushBack(pair.first);
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->IsEntityInitialized(entity)) {
                continue;
            }

            if (system_it.second->ActsOnComponents(component_type_ids, true)) {
                system_it.second->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(ID<Entity> entity, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;
    
    AssertThrow(entity.IsValid());

    Array<TypeID> component_type_ids;
    component_type_ids.Reserve(component_ids.Size());

    for (const auto &pair : component_ids) {
        component_type_ids.PushBack(pair.first);
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->ActsOnComponents(component_type_ids, true)) {
                system_it.second->OnEntityRemoved(entity);
            }
        }
    }
}

ComponentInterfaceBase *EntityManager::GetComponentInterface(TypeID type_id)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(type_id);
}

void EntityManager::BeginUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    
    if (m_command_queue.HasUpdates()) {
        m_command_queue.Execute(*this, delta);
    }

    // Process the execution groups in sequential order
    for (auto &system_execution_group : m_system_execution_groups) {
        system_execution_group.StartProcessing(delta);
    }
}

void EntityManager::EndUpdate()
{
    HYP_SCOPE;
    
    for (auto &system_execution_group : m_system_execution_groups) {
        system_execution_group.FinishProcessing();
    }
}

void SystemExecutionGroup::StartProcessing(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    AssertThrow(m_tasks.Empty());

    m_tasks.Reserve(m_systems.Size());

    for (auto &it : m_systems) {
        m_tasks.PushBack(TaskSystem::GetInstance().Enqueue([system = it.second.Get(), delta]
        {
            HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

            system->Process(delta);
        }));
    }
}

void SystemExecutionGroup::FinishProcessing()
{
    for (Task<void> &task : m_tasks) {
        task.Await();
    }

    m_tasks.Clear();
}

#pragma endregion EntityManager

} // namespace hyperion
