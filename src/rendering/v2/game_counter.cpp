#include "game_counter.h"

#include <system/sdl_system.h>

namespace hyperion::v2 {

void GameCounter::NextTick()
{
    last_tick = tick;
    tick      = SDL_GetPerformanceCounter();
    delta     = (tick - last_tick) / TickUnit(SDL_GetPerformanceFrequency());
}

} // namespace hyperion::v2