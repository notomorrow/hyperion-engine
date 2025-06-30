/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_RENDER_STATS_HPP
#define HYPERION_ENGINE_RENDER_STATS_HPP

#include <core/containers/FixedArray.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

#include <cfloat>

#define HYP_ENABLE_RENDER_STATS
#define HYP_ENABLE_RENDER_STATS_COUNTERS

namespace hyperion {

enum EngineRenderStatsCountType : uint32
{
    ERS_DRAW_CALLS = 0,
    ERS_INSTANCED_DRAW_CALLS,
    ERS_TRIANGLES,
    ERS_RENDER_GROUPS,
    ERS_VIEWS,
    ERS_SCENES,
    ERS_LIGHTS,
    ERS_LIGHTMAP_VOLUMES,
    ERS_ENV_PROBES,
    ERS_ENV_GRIDS,

    ERS_MAX
};

static_assert(ERS_MAX <= 16, "EngineRenderStatsCountType must not exceed 16 types");

struct EngineRenderStatsCounts
{
    uint32 counts[16] = { 0 };

    HYP_FORCE_INLINE constexpr uint32& operator[](EngineRenderStatsCountType type)
    {
        return counts[type];
    }

    HYP_FORCE_INLINE constexpr uint32 operator[](EngineRenderStatsCountType type) const
    {
        return counts[type];
    }
};

HYP_STRUCT()
struct EngineRenderStats
{
    double framesPerSecond = 0.0;
    double millisecondsPerFrame = 0.0;
    double millisecondsPerFrameAvg = 0.0;
    double millisecondsPerFrameMax = 0.0;
    double millisecondsPerFrameMin = DBL_MAX;
    EngineRenderStatsCounts counts;
};

struct HYP_API SuppressEngineRenderStatsScope
{
    SuppressEngineRenderStatsScope();
    ~SuppressEngineRenderStatsScope();
};

class EngineRenderStatsCalculator
{
    static constexpr uint32 maxSamples = 512;

public:
    friend struct SuppressEngineRenderStatsScope;

    void AddCounts(const EngineRenderStatsCounts& counts);
    void Advance(EngineRenderStats& renderStats);

private:
    HYP_FORCE_INLINE void Suppress()
    {
        m_suppressCount++;
    }

    HYP_FORCE_INLINE void Unsuppress()
    {
        if (m_suppressCount > 0)
        {
            m_suppressCount--;
        }
    }

    double CalculateFramesPerSecond() const;
    double CalculateMillisecondsPerFrame() const;

    void AddSample(double delta);

    GameCounter m_counter;
    double m_deltaAccum = 0.0;
    FixedArray<double, maxSamples> m_samples { 0.0 };
    uint32 m_numSamples = 0;
    EngineRenderStatsCounts m_counts;

    int m_suppressCount = 0;
};

} // namespace hyperion

#endif // HYPERION_ENGINE_RENDER_STATS_HPP