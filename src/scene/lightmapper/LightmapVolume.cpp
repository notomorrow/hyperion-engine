/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG
// #define HYP_DISABLE_VISIBILITY_CHECK

namespace hyperion {

LightmapVolume::LightmapVolume()
    : m_aabb(BoundingBox::Empty())
{
}
    
LightmapVolume::LightmapVolume(const BoundingBox &aabb)
    : m_aabb(aabb)
{
}

LightmapVolume::~LightmapVolume()
{
}
    
void LightmapVolume::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
    }));

    SetReady(true);
}

} // namespace hyperion
