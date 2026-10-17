/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerGroundGI.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerGroundGI("Rendering/GPU/Glimmer/GroundGI");

static constexpr uint32 GroundGIGroupSize = 64;
// NOTE must match GlimmerGroundGI.hlsli: the slots, then what arrives and what is sent, then the canopy's top, bottom and sun
static constexpr uint32 GroundGIWords = GlimmerGroundGITexels * (GlimmerGroundGIRays + 5);
static constexpr float GroundGIRelightChange = 0.05f;

struct GlimmerGroundGIConstants
{
    GlimmerBrickShaderData bricks;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerRelightShaderData lit;
    GlimmerLightShaderData light;
    Vec4i rect;   // xy = first texel of the dispatch, zw = past its last
    Vec4u info;   // x = level, y = 1 when the spans can be traced
    Vec4f params; // x = foliage extinction, y = how far a ray looks, z = albedo where the ground's isn't known, w = Rendering.Glimmer.Intensity
};

#pragma region GlimmerGroundGI

GlimmerGroundGI::GlimmerGroundGI()
    : m_relitIntensity(0.0f),
      m_hasRelitLight(false)
{
}

GlimmerGroundGI::~GlimmerGroundGI()
{
    EnqueueDeletion(std::move(m_buffer));
}

void GlimmerGroundGI::Reset()
{
    m_scheduler.Invalidate();
    m_hasRelitLight = false;
}

bool GlimmerGroundGI::IsReady() const
{
    if (!m_buffer.IsValid())
    {
        return false;
    }

    const GlimmerRelightShaderData& shaderData = m_scheduler.GetShaderData();

    for (uint32 level = 0; level < GlimmerGroundLevels; level++)
    {
        if (shaderData.levels[level].z != 0)
        {
            return true;
        }
    }

    return false;
}

void GlimmerGroundGI::Update(
    Frame* frame,
    const GlimmerChannelState& state,
    const GlimmerSurfaceCache& surfaceCache,
    const GlimmerSpanCache& spanCache,
    const GlimmerBrickWorld* brickWorld,
    const GlimmerLightShaderData& light)
{
    HYP_SCOPE;

    if (!surfaceCache.GetGroundImageView().IsValid() || !surfaceCache.GetGroundAlbedoImageView().IsValid())
    {
        return;
    }

    if (!m_buffer.IsValid())
    {
        m_buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, size_t(GroundGIWords) * sizeof(uint32), alignof(uint32));
        Check(m_buffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
        m_buffer->SetDebugName(NAME("GlimmerGroundGI"));
#endif
    }

    m_light = light;

    m_scheduler.Schedule(state, surfaceCache, spanCache);

    // a change of sun or sky lights every texel again, keeping the old light meanwhile
    const float intensity = g_cvGlimmerIntensity.Get();

    if (!m_hasRelitLight || intensity != m_relitIntensity || GetGlimmerLightChange(m_relitLight, light) > GroundGIRelightChange)
    {
        if (m_hasRelitLight)
        {
            m_scheduler.RelightAll();
        }

        m_relitLight = light;
        m_relitIntensity = intensity;
        m_hasRelitLight = true;
    }

    const Array<GlimmerRelightDispatch, GlimmerAllocator>& dispatches = m_scheduler.GetDispatches();

    if (dispatches.Any())
    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerGroundGI);

        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_buffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const GlimmerRelightDispatch& dispatch : dispatches)
        {
            Dispatch(frame, dispatch, surfaceCache, spanCache, brickWorld);
        }

        cr << InsertBarrier(m_buffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    m_scheduler.OnDispatched();
}

void GlimmerGroundGI::Dispatch(Frame* frame, const GlimmerRelightDispatch& dispatch, const GlimmerSurfaceCache& surfaceCache, const GlimmerSpanCache& spanCache, const GlimmerBrickWorld* brickWorld)
{
    CommandRecorder& cr = frame->cr;

    const GlimmerTexelRect& rect = dispatch.rect;
    const uint32 numTexels = uint32((rect.max.x - rect.min.x) * (rect.max.y - rect.min.y));

    if (numTexels == 0)
    {
        return;
    }

    const bool hasBricks = brickWorld && brickWorld->IsReady() && brickWorld->IsLit() && brickWorld->GetCrossingLightBuffer().IsValid();
    const bool hasSpans = spanCache.GetSpansBuffer().IsValid() && spanCache.GetHeightBoundsBuffer().IsValid();

    GlimmerGroundGIConstants constants {};

    if (hasBricks)
    {
        constants.bricks = brickWorld->GetShaderData();
    }

    constants.ground = surfaceCache.GetGroundShaderData();

    if (hasSpans)
    {
        constants.spans = spanCache.GetShaderData();
    }

    constants.lit = m_scheduler.GetShaderData();
    constants.light = m_light;
    constants.rect = Vec4i(rect.min.x, rect.min.y, rect.max.x, rect.max.y);
    constants.info = Vec4u(dispatch.level, hasSpans ? 1u : 0u, 0u, 0u);
    constants.params = Vec4f(GetGlimmerFoliageExtinction(), GlimmerMaxRayDistance, GlimmerDefaultGroundAlbedo, MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f));

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerGroundGI")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    if (hasBricks)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, brickWorld->GetTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, brickWorld->GetIndexBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, brickWorld->GetCrossingsBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, brickWorld->GetMaterialBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, brickWorld->GetCrossingLightBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        // HACK: any structured buffer will do, as the zeroed brick constants keep them from being read
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }

    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache.GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, surfaceCache.GetGroundAlbedoImageView());

    if (hasSpans)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, spanCache.GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, spanCache.GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }

    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundGIBuffer"_sh, m_buffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

    // a dispatch is a few rows of a window at most, well within one row of groups
    cr << DispatchCompute(Vec3u((numTexels + GroundGIGroupSize - 1) / GroundGIGroupSize, 1, 1));
}

#pragma endregion GlimmerGroundGI

} // namespace Hyperion
