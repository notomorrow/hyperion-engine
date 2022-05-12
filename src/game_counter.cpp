#include "game_counter.h"

#include <system/sdl_system.h>

namespace hyperion::v2 {

GameCounter::TimePoint GameCounter::Now()
{
    return std::chrono::steady_clock::now();
}

void GameCounter::NextTick()
{
    const auto current = Now();

    delta           = Interval(current);
    last_time_point = current;
}

} // namespace hyperion::v2