#ifndef HYPERION_V2_GAME_COUNTER_H
#define HYPERION_V2_GAME_COUNTER_H

#include <chrono>

namespace hyperion::v2 {

struct GameCounter {
    using TickUnit   = float;
    using TimePoint  = std::chrono::steady_clock::time_point;

    TimePoint last_time_point;
    TickUnit  delta{};

    void NextTick();
    static TimePoint Now();

    TickUnit Interval(TimePoint end_time_point) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnit, std::ratio<1>>>(end_time_point - last_time_point).count();
    }
};

struct LockstepGameCounter : GameCounter {
    TickUnit target_interval;

    LockstepGameCounter(TickUnit target_interval)
        : GameCounter(),
          target_interval(target_interval)
    {
    }

    bool Waiting()
    {
        return Interval(Now()) < target_interval;
    }
};

} // namespace hyperion::v2

#endif