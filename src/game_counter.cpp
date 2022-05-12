#include "game_counter.h"

#include <system/sdl_system.h>

namespace hyperion::v2 {

void GameCounter::NextTick()
{
    using namespace std::chrono;

    const auto current = steady_clock::now();

    delta           = duration_cast<duration<TickUnit, std::ratio<1>>>(current - last_time_point).count();
    last_time_point = current;
}

} // namespace hyperion::v2