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
};

} // namespace hyperion::v2

#endif