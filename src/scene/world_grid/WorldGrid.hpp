/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_HPP
#define HYPERION_WORLD_GRID_HPP

#include <core/Handle.hpp>

#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/Queue.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/Mutex.hpp>

#include <math/Vector2.hpp>

#include <GameCounter.hpp>
#include <HashCode.hpp>

namespace hyperion {

class Scene;
class EntityManager;
class WorldGridPlugin;

enum class WorldGridPatchState
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

struct WorldGridPatchNeighbor
{
    Vec2i   coord { 0, 0 };

    Vec2f GetCenter() const { return Vec2f(coord) - 0.5f; }
};

struct WorldGridPatchUpdate
{
    Vec2i               coord;
    WorldGridPatchState state;
};

struct WorldGridPatchInfo
{
    Vec3i                                   extent;
    Vec2i                                   coord { 0, 0 };
    Vec3f                                   scale { 1.0f };
    WorldGridPatchState                     state { WorldGridPatchState::UNLOADED };
    float                                   unload_timer { 0.0f };
    FixedArray<WorldGridPatchNeighbor, 8>   neighbors { };

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(extent);
        hash_code.Add(coord);
        hash_code.Add(scale);
        hash_code.Add(state);

        return hash_code;
    }
};

class HYP_API WorldGridPatch
{
public:
    WorldGridPatch(const WorldGridPatchInfo &patch_info);
    virtual ~WorldGridPatch();

    HYP_FORCE_INLINE
    const WorldGridPatchInfo &GetPatchInfo() const
        { return m_patch_info; }

    virtual void InitializeEntity(const Handle<Scene> &scene, ID<Entity> entity) = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;

protected:
    WorldGridPatchInfo  m_patch_info;
};

struct WorldGridPatchGenerationQueue
{
    Mutex                               mutex;
    Queue<UniquePtr<WorldGridPatch>>    queue;
    AtomicVar<bool>                     has_updates;
};

struct WorldGridState
{
    FlatMap<Vec2i, Task<void>>              patch_generation_tasks;
    Queue<UniquePtr<WorldGridPatch>>        patch_generation_queue_owned;
    WorldGridPatchGenerationQueue           patch_generation_queue_shared;

    FlatSet<Vec2i>                          queued_neighbors;
    Queue<WorldGridPatchUpdate>             patch_update_queue;
    FlatMap<Vec2i, ID<Entity>>              patch_entities;
    mutable Mutex                           patch_entities_mutex;

    void AddPatchEntity(ID<Entity> entity, const Vec2i &coord)
    {
        Mutex::Guard guard(patch_entities_mutex);

        patch_entities.Insert(coord, entity);
    }

    bool RemovePatchEntity(ID<Entity> entity)
    {
        Mutex::Guard guard(patch_entities_mutex);

        for (auto it = patch_entities.Begin(); it != patch_entities.End(); ++it) {
            if (it->second == entity) {
                patch_entities.Erase(it);

                return true;
            }
        }

        return false;
    }

    bool RemovePatchEntity(const Vec2i &coord)
    {
        Mutex::Guard guard(patch_entities_mutex);

        return patch_entities.Erase(coord);
    }

    ID<Entity> GetPatchEntity(const Vec2i &coord) const
    {
        Mutex::Guard guard(patch_entities_mutex);

        const auto it = patch_entities.Find(coord);

        if (it != patch_entities.End()) {
            return it->second;
        }

        return {};
    }
};

struct WorldGridParams
{
    Vec3i   patch_size { 32, 32, 32 };
    Vec3f   offset { 0.0f, 0.0f, 0.0f };
    Vec3f   scale { 1.0f, 1.0f, 1.0f };
    float   max_distance = 2.0f;

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(patch_size);
        hc.Add(offset);
        hc.Add(scale);
        hc.Add(max_distance);
        return hc;
    }
};

class HYP_API WorldGrid
{
public:
    WorldGrid();
    WorldGrid(const WorldGridParams &params, const Handle<Scene> &scene);
    WorldGrid(const WorldGrid &other)               = delete;
    WorldGrid &operator=(const WorldGrid &other)    = delete;
    WorldGrid(WorldGrid &&other)                    = delete;
    WorldGrid &operator=(WorldGrid &&other)         = delete;
    ~WorldGrid();

    HYP_FORCE_INLINE
    const WorldGridParams &GetParams() const
        { return m_params; }

    HYP_FORCE_INLINE
    const WeakHandle<Scene> &GetScene() const
        { return m_scene; }

    HYP_FORCE_INLINE
    const WorldGridState &GetState() const
        { return m_state; }

    void AddPlugin(int priority, RC<WorldGridPlugin> &&plugin);
    RC<WorldGridPlugin> GetPlugin(int priority) const;

    void Initialize();
    void Shutdown();
    void Update(GameCounter::TickUnit delta);

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_params);
        return hc;
    }

private:
    RC<WorldGridPlugin> GetMainPlugin() const;

    const WorldGridParams               m_params;
    WeakHandle<Scene>                   m_scene;

    WorldGridState                      m_state;

    FlatMap<int, RC<WorldGridPlugin>>   m_plugins;

    bool                                m_is_initialized;
};

} // namespace hyperion

#endif
