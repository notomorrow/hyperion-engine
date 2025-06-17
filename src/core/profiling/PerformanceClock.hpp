/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PERFORMANCE_CLOCK_HPP
#define HYPERION_PERFORMANCE_CLOCK_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace profiling {

class HYP_API PerformanceClock
{
public:
    static uint64 Now();

    /*! Get time since the given timestamp in microseconds */
    static uint64 TimeSince(uint64 microseconds);

    PerformanceClock();

    HYP_FORCE_INLINE uint64 Elapsed() const
    {
        return (m_endTimeUs == 0 ? Now() : m_endTimeUs) - m_startTimeUs;
    }

    HYP_FORCE_INLINE double ElapsedMs() const
    {
        return double(Elapsed()) / 1000.0;
    }

    void Start();
    void Stop();

private:
    uint64 m_startTimeUs;
    uint64 m_endTimeUs;
};

} // namespace profiling

using profiling::PerformanceClock;

} // namespace hyperion

#endif