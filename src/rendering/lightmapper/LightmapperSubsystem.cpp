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

void LightmapperSubsystem::OnAddedToWorld()
{
    Threads::AssertOnThread(g_gameThread);
}

void LightmapperSubsystem::OnRemovedFromWorld()
{
    Threads::AssertOnThread(g_gameThread);

    m_lightmappers.Clear();
}

void LightmapperSubsystem::Update(float delta)
{
    Threads::AssertOnThread(g_gameThread);

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

    Array<ObjId<Scene>> lightmappersToRemove;

    for (auto& it : m_lightmappers)
    {
        it.second->Update(delta);

        if (it.second->IsComplete())
        {
            lightmappersToRemove.PushBack(it.first);
        }
    }

    for (ObjId<Scene> sceneId : lightmappersToRemove)
    {
        m_lightmappers.Erase(sceneId);
    }
}

Task<void>* LightmapperSubsystem::GenerateLightmaps(const Handle<Scene>& scene, const BoundingBox& aabb)
{
    Threads::AssertOnThread(g_gameThread);

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
        HYP_LOG(Rendering, Error, "Invalid AABB provided for lightmapper in Scene {}", scene->Id());

        return nullptr;
    }

    auto it = m_lightmappers.Find(scene.Id());

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

    m_lightmappers.Insert(scene.Id(), std::move(lightmapper));

    return &task;
}

#pragma endregion LightmapperSubsystem

} // namespace hyperion