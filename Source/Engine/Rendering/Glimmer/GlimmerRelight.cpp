/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr uint32 RelightTexelsPerFrame = 4096;
static constexpr size_t MaxPendingRects = 16;

static constexpr uint32 BurstPassTexels = GlimmerGroundLevels * GlimmerGroundResolution * GlimmerGroundResolution;

#pragma region GlimmerRelight

GlimmerRelight::GlimmerRelight()
    : m_groundGeneration(~0u),
      m_refreshLevel(0),
      m_refreshBudget(0),
      m_scheduledRefreshTexels(0),
      m_burstTexelsLeft(0),
      m_sunTexelsLeft(0),
      m_shaderData {}
{
}

GlimmerRelight::~GlimmerRelight()
{
    m_texture = Handle<Texture>();
}

const GpuImageViewRef& GlimmerRelight::GetImageView()
{
    if (!m_texture.IsValid())
    {
        m_texture = CreateGlimmerStorageTexture(
            TextureType::Texture2DArray,
            TextureFormat::RGBA16F,
            Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1),
            uint16(GlimmerGroundLevels * GlimmerRelightLayers),
            NAME("GlimmerRelight"));
    }

    return RI.textureViewCache->GetOrCreate(m_texture);
}

const GpuImageRef& GlimmerRelight::GetGpuImage() const
{
    AssertDebug(m_texture.IsValid());

    return m_texture->GetGpuImage();
}

void GlimmerRelight::AddPending(Level& level, const Rect& rect)
{
    const Rect clipped = Rect::Intersect(rect, GetGlimmerGroundWindow(level.windowOrigin));

    if (clipped.IsEmpty())
    {
        return;
    }

    if (level.pending.Size() >= MaxPendingRects)
    {
        level.pending.Clear();
        level.pending.PushBack(GetGlimmerGroundWindow(level.windowOrigin));

        return;
    }

    level.pending.PushBack(clipped);
}

void GlimmerRelight::Schedule(
    const GlimmerChannelState& state,
    const GlimmerSurfaceCache& surfaceCache,
    const GlimmerSpanCache& spanCache,
    EnumFlags<GlimmerLightingChangeFlags> lightingChanges)
{
    HYP_SCOPE;

    if (state.groundGeneration != m_groundGeneration)
    {
        m_groundGeneration = state.groundGeneration;

        for (Level& level : m_levels)
        {
            level = Level {};
        }

        m_dispatches.Clear();
    }

    // what the last schedule asked for and nothing got to dispatch is still owed
    for (const GlimmerRelightDispatch& dispatch : m_dispatches)
    {
        Level& level = m_levels[dispatch.level];

        if (level.hasWindow)
        {
            AddPending(level, dispatch.rect);
        }
    }

    m_dispatches.Clear();

    int32 budget = RelightTexelsPerFrame;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;
        const Rect window = GetGlimmerGroundWindow(windowOrigin);

        const bool isJump = !level.hasWindow
            || MathUtil::Abs(windowOrigin.x - level.windowOrigin.x) >= int32(GlimmerGroundResolution)
            || MathUtil::Abs(windowOrigin.y - level.windowOrigin.y) >= int32(GlimmerGroundResolution);

        if (isJump)
        {
            level.hasWindow = true;
            level.windowOrigin = windowOrigin;
            level.isLit = false;
            level.isFilling = true;
            level.refreshRow = window.min.y;
            level.pending.Clear();
            level.pending.PushBack(window);
        }
        else if (windowOrigin != level.windowOrigin)
        {
            Rect columns;
            Rect rows;
            GetGlimmerScrolledRects(GetGlimmerGroundWindow(level.windowOrigin), window, columns, rows);

            level.windowOrigin = windowOrigin;

            for (Rect& pendingRect : level.pending)
            {
                pendingRect = Rect::Intersect(pendingRect, window);
            }

            for (const Rect& scrolled : { columns, rows })
            {
                if (!scrolled.IsEmpty())
                {
                    m_dispatches.PushBack(GlimmerRelightDispatch { levelIndex, scrolled });
                    budget -= scrolled.Area();
                }
            }
        }

        const Rect& uploadedRect = surfaceCache.GetUploadedRect(levelIndex);

        if (!uploadedRect.IsEmpty())
        {
            AddPending(level, Rect { uploadedRect.min - Vec2i(1, 1), uploadedRect.max + Vec2i(1, 1) });
        }

        AddPending(level, spanCache.GetFilledRect(levelIndex));
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels && budget > 0; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        while (level.pending.Any() && budget > 0)
        {
            Rect& rect = level.pending.Front();

            if (rect.IsEmpty())
            {
                level.pending.PopFront();

                continue;
            }

            const int32 width = rect.max.x - rect.min.x;
            const int32 rows = MathUtil::Clamp(budget / MathUtil::Max(width, 1), 1, rect.max.y - rect.min.y);

            m_dispatches.PushBack(GlimmerRelightDispatch { levelIndex, Rect { rect.min, Vec2i(rect.max.x, rect.min.y + rows) } });

            budget -= width * rows;
            rect.min.y += rows;
        }
    }

    const bool runOnChange = g_cvGlimmerRunOnChange.Get();
    const int32 rowTexels = int32(GlimmerGroundResolution);

    if (runOnChange)
    {
        if (lightingChanges)
        {
            m_burstTexelsLeft = int32(GlimmerLightingBurstPasses * BurstPassTexels);
        }

        // the sun's visibility is right again after one pass
        if (lightingChanges & GlimmerLightingChangeFlags::SunDirection)
        {
            m_sunTexelsLeft = int32(BurstPassTexels);
        }

        const int32 rate = MathUtil::Max(m_burstTexelsLeft > 0 ? g_cvGlimmerRelightTexelsBurst.Get() : g_cvGlimmerRelightTexels.Get(), 0);

        m_refreshBudget = MathUtil::Min(m_refreshBudget + rate, MathUtil::Max(rate, rowTexels) * 2);

        budget = m_refreshBudget >= rowTexels
            ? m_refreshBudget
            : 0;
    }

    const int32 refreshBudget = budget;

    for (uint32 attempt = 0; attempt < GlimmerGroundLevels && budget > 0; attempt++)
    {
        Level& level = m_levels[m_refreshLevel];

        if (level.pending.Any() || !level.hasWindow)
        {
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;

            continue;
        }

        const Rect window = GetGlimmerGroundWindow(level.windowOrigin);

        // rows that scrolled out are skipped; the ones that scrolled in were lit as they did
        level.refreshRow = MathUtil::Max(level.refreshRow, window.min.y);

        if (level.refreshRow >= window.max.y)
        {
            level.refreshRow = window.min.y;
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;

            continue;
        }

        const int32 rows = MathUtil::Clamp(budget / int32(GlimmerGroundResolution), 1, window.max.y - level.refreshRow);

        m_dispatches.PushBack(GlimmerRelightDispatch { m_refreshLevel, Rect { Vec2i(window.min.x, level.refreshRow), Vec2i(window.max.x, level.refreshRow + rows) } });

        budget -= rows * int32(GlimmerGroundResolution);

        level.refreshRow += rows;

        if (level.refreshRow >= window.max.y)
        {
            level.refreshRow = window.min.y;
            m_refreshLevel = (m_refreshLevel + 1) % GlimmerGroundLevels;
        }

        attempt = 0;
    }

    if (runOnChange)
    {
        // the bursts only move on once these have been dispatched
        m_scheduledRefreshTexels = refreshBudget - budget;

        m_refreshBudget -= m_scheduledRefreshTexels;
    }
    else
    {
        m_scheduledRefreshTexels = 0;
        m_burstTexelsLeft = 0;
        m_sunTexelsLeft = 0;
    }
}

bool GlimmerRelight::IsBursting() const
{
    return m_burstTexelsLeft > 0;
}

bool GlimmerRelight::IsBurstRelightingSun() const
{
    return m_sunTexelsLeft > 0;
}

void GlimmerRelight::OnDispatched()
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        Level& level = m_levels[levelIndex];

        if (level.isFilling && level.pending.Empty())
        {
            level.isFilling = false;
            level.isLit = true;
        }

        m_shaderData.levels[levelIndex] = Vec4i(level.windowOrigin.x, level.windowOrigin.y, level.isLit ? 1 : 0, 0);
    }

    m_burstTexelsLeft = MathUtil::Max(m_burstTexelsLeft - m_scheduledRefreshTexels, 0);
    m_sunTexelsLeft = MathUtil::Max(m_sunTexelsLeft - m_scheduledRefreshTexels, 0);
    m_scheduledRefreshTexels = 0;

    m_dispatches.Clear();
}

void GlimmerRelight::Invalidate()
{
    for (Level& level : m_levels)
    {
        level = Level {};
    }

    m_dispatches.Clear();
    m_shaderData = GlimmerRelightShaderData {};

    m_refreshBudget = 0;
    m_scheduledRefreshTexels = 0;
    m_burstTexelsLeft = 0;
    m_sunTexelsLeft = 0;
}

#pragma endregion GlimmerRelight

} // namespace Hyperion
