/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_COUNTER_HPP
#define HYPERION_GAME_COUNTER_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

#include <chrono>

namespace hyperion {

struct GameCounter
{
    using Clock = std::chrono::high_resolution_clock;

    using TickUnit = float;
    using TickUnitHighPrec = double;
    using TimePoint = Clock::time_point;

    TimePoint lastTimePoint = Now();
    TickUnit delta {};

    HYP_FORCE_INLINE static TimePoint Now()
    {
        return Clock::now();
    }

    void NextTick()
    {
        const TimePoint current = Now();

        delta = Interval(current);
        lastTimePoint = current;
    }

    TickUnit Interval(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnit, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }

    TickUnitHighPrec IntervalHighPrec(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnitHighPrec, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }
};

struct LockstepGameCounter : GameCounter
{
    TickUnit targetInterval;
    TickUnit padding;

    LockstepGameCounter(TickUnit targetInterval, TickUnit padding = TickUnit(0.0))
        : GameCounter(),
          targetInterval(targetInterval),
          padding(padding)
    {
    }

    HYP_FORCE_INLINE bool Waiting() const
    {
        return Interval(Now()) < targetInterval - padding;
    }
};

} // namespace hyperion

#endif