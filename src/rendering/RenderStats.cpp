/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderStats.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region SuppressRenderStatsScope

SuppressRenderStatsScope::SuppressRenderStatsScope()
{
    RenderApi_SuppressRenderStats();
}

SuppressRenderStatsScope::~SuppressRenderStatsScope()
{
    RenderApi_UnsuppressRenderStats();
}

#pragma endregion SuppressRenderStatsScope

#pragma region RenderStatsCalculator

void RenderStatsCalculator::AddCounts(const RenderStatsCounts& counts)
{
#if defined(HYP_ENABLE_RENDER_STATS) && defined(HYP_ENABLE_RENDER_STATS_COUNTERS)
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_suppressCount > 0)
    {
        return;
    }

    for (uint32 i = 0; i < ERS_MAX; ++i)
    {
        m_counts.counts[i] += counts.counts[i];
    }
#endif
}

void RenderStatsCalculator::AddSample(double delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_suppressCount > 0)
    {
        return;
    }

    const uint32 sampleIndex = m_numSamples++;

    m_samples[sampleIndex % maxSamples] = delta;
}

void RenderStatsCalculator::Advance(RenderStats& renderStats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    m_counter.NextTick();
    m_deltaAccum += m_counter.delta;

    const bool resetMinMax = m_deltaAccum >= 1.0;

    AddSample(m_counter.delta);

    RenderStats newRenderStats;
    newRenderStats.framesPerSecond = CalculateFramesPerSecond();
    newRenderStats.millisecondsPerFrame = m_counter.delta * 1000.0;
    newRenderStats.millisecondsPerFrameAvg = CalculateMillisecondsPerFrame();
    newRenderStats.millisecondsPerFrameMax = resetMinMax
        ? newRenderStats.millisecondsPerFrame
        : MathUtil::Max(renderStats.millisecondsPerFrameMax, newRenderStats.millisecondsPerFrame);
    newRenderStats.millisecondsPerFrameMin = resetMinMax
        ? newRenderStats.millisecondsPerFrame
        : MathUtil::Min(renderStats.millisecondsPerFrameMin, newRenderStats.millisecondsPerFrame);
    newRenderStats.counts = m_counts;

    renderStats = newRenderStats;

    if (resetMinMax)
    {
        m_deltaAccum = 0.0;
    }

    Memory::MemSet(m_counts.counts, 0, sizeof(m_counts.counts[0]) * ERS_MAX);
}

double RenderStatsCalculator::CalculateFramesPerSecond() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_numSamples == 0)
    {
        return 0.0;
    }

    const uint32 count = m_numSamples < maxSamples ? m_numSamples : maxSamples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i)
    {
        total += 1.0 / m_samples[i];
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilonD);
}

double RenderStatsCalculator::CalculateMillisecondsPerFrame() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_numSamples == 0)
    {
        return 0.0;
    }

    const uint32 count = m_numSamples < maxSamples ? m_numSamples : maxSamples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i)
    {
        total += m_samples[i] * 1000.0;
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilonD);
}

#pragma endregion RenderStatsCalculator

} // namespace hyperion
