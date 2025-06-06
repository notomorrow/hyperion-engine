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

    TimePoint last_time_point = Now();
    TickUnit delta {};

    HYP_FORCE_INLINE static TimePoint Now()
    {
        return Clock::now();
    }

    void NextTick()
    {
        const TimePoint current = Now();

        delta = Interval(current);
        last_time_point = current;
    }

    TickUnit Interval(TimePoint end_time_point) const
    {
        using namespace std::chrono;

        return duration_cast<duration<TickUnit, std::ratio<1>>>(end_time_point - last_time_point).count();
    }

    TickUnitHighPrec IntervalHighPrec(TimePoint end_time_point) const
    {
        using namespace std::chrono;

        return duration_cast<duration<TickUnitHighPrec, std::ratio<1>>>(end_time_point - last_time_point).count();
    }
};

struct LockstepGameCounter : GameCounter
{
    TickUnit target_interval;
    TickUnit padding;

    LockstepGameCounter(TickUnit target_interval, TickUnit padding = TickUnit(0.0))
        : GameCounter(),
          target_interval(target_interval),
          padding(padding)
    {
    }

    HYP_FORCE_INLINE bool Waiting() const
    {
        return Interval(Now()) < target_interval - padding;
    }
};

} // namespace hyperion

#endif