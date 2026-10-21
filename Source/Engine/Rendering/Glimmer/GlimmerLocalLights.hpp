/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

struct GlimmerLocalLightShaderData
{
    Vec4f positionRadius;
    Vec4f color;
    Vec4f spotDirection;
    Vec4f spotAngles;
};

static_assert(sizeof(GlimmerLocalLightShaderData) == 64);

/// GlimmerLocalLights in GlimmerLocalLights.hlsli
struct GlimmerLocalLightsShaderData
{
    Vec4u info; // x = number of lights
    GlimmerLocalLightShaderData lights[GlimmerMaxLocalLights];
};

class GlimmerLocalLights final
{
public:
    void Update(const GlimmerChannelState& state);

    HYP_FORCE_INLINE const GlimmerLocalLightsShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE uint32 GetGeneration() const
    {
        return m_generation;
    }

    bool GetChangesSince(uint32 seenGeneration, GlimmerSceneChanges& outChanges) const;

private:
    struct ChangeSet
    {
        uint32 generation = 0;
        GlimmerSceneChanges changes;
    };

    Array<GlimmerLocalLightState, GlimmerAllocator> m_lights;
    Array<ChangeSet, GlimmerAllocator> m_changeJournal;
    uint32 m_generation = 0;

    GlimmerLocalLightsShaderData m_shaderData {};
};

} // namespace Hyperion
