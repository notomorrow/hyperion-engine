/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/lightmapper/LightmapperSubsystem.hpp>
#include <rendering/lightmapper/Lightmapper.hpp>

#include <rendering/backend/RenderConfig.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region LightmapperSubsystem

LightmapperSubsystem::LightmapperSubsystem()
{
}

void LightmapperSubsystem::Initialize()
{
    Threads::AssertOnThread(g_game_thread);
}

void LightmapperSubsystem::Shutdown()
{
    Threads::AssertOnThread(g_game_thread);

    m_lightmappers.Clear();
}

void LightmapperSubsystem::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(g_game_thread);

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        if (it->IsCompleted())
        {
            it = m_tasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    Array<ID<Scene>> lightmappers_to_remove;

    for (auto& it : m_lightmappers)
    {
        it.second->Update(delta);

        if (it.second->IsComplete())
        {
            lightmappers_to_remove.PushBack(it.first);
        }
    }

    for (ID<Scene> scene_id : lightmappers_to_remove)
    {
        m_lightmappers.Erase(scene_id);
    }
}

Task<void>* LightmapperSubsystem::GenerateLightmaps(const Handle<Scene>& scene, const BoundingBox& aabb)
{
    Threads::AssertOnThread(g_game_thread);

    if (!scene.IsValid())
    {
        return nullptr;
    }

    if (!scene->IsForegroundScene())
    {
        return nullptr;
    }

    if (!aabb.IsValid() || !aabb.IsFinite())
    {
        HYP_LOG(Rendering, Error, "Invalid AABB provided for lightmapper in Scene {}", scene->GetID().Value());

        return nullptr;
    }

    auto it = m_lightmappers.Find(scene.GetID());

    if (it != m_lightmappers.End())
    {
        // already running

        return nullptr;
    }

    UniquePtr<Lightmapper> lightmapper = MakeUnique<Lightmapper>(LightmapperConfig::FromConfig(), scene, aabb);

    Task<void>& task = m_tasks.EmplaceBack();

    lightmapper->OnComplete
        .Bind([promise = task.Promise()]()
            {
                promise->Fulfill();
            })
        .Detach();

    lightmapper->PerformLightmapping();

    m_lightmappers.Insert(scene.GetID(), std::move(lightmapper));

    return &task;
}

#pragma endregion LightmapperSubsystem

} // namespace hyperion