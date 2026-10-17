/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>

namespace Hyperion {

class GlimmerSurfaceCache;
class GlimmerSpanCache;
class GlimmerBrickWorld;

// NOTE must match GlimmerGroundGI.hlsli
static constexpr uint32 GlimmerGroundGIRays = 8;
static constexpr uint32 GlimmerGroundGITexels = GlimmerGroundResolution * GlimmerGroundResolution * GlimmerGroundLevels;

/// brick GI on the ground: each texel of the ground clipmap's windows traces slots as a card's tile does, gathers what arrives from them,
/// and keeps what it sends on, lit with the sun behind a shadow ray. Ground pixels read what arrives, slots that land on the ground what is sent
class GlimmerGroundGI final
{
public:
    GlimmerGroundGI();

    GlimmerGroundGI(const GlimmerGroundGI& other) = delete;
    GlimmerGroundGI& operator=(const GlimmerGroundGI& other) = delete;

    ~GlimmerGroundGI();

    void Update(
        Frame* frame,
        const GlimmerChannelState& state,
        const GlimmerSurfaceCache& surfaceCache,
        const GlimmerSpanCache& spanCache,
        const GlimmerBrickWorld* brickWorld,
        const GlimmerLightShaderData& light);

    void Reset();

    // any level lit
    bool IsReady() const;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    // the lit windows, as GlimmerRelightParams
    HYP_FORCE_INLINE const GlimmerRelightShaderData& GetShaderData() const
    {
        return m_scheduler.GetShaderData();
    }

private:
    void Dispatch(Frame* frame, const GlimmerRelightDispatch& dispatch, const GlimmerSurfaceCache& surfaceCache, const GlimmerSpanCache& spanCache, const GlimmerBrickWorld* brickWorld);

    GlimmerRelight m_scheduler;
    GpuBufferRef m_buffer;

    GlimmerLightShaderData m_light;
    GlimmerLightShaderData m_relitLight;
    float m_relitIntensity;
    bool m_hasRelitLight;
};

} // namespace Hyperion
