#ifndef HYPERION_V2_GAME_COUNTER_H
#define HYPERION_V2_GAME_COUNTER_H

namespace hyperion::v2 {

struct GameCounter {
    using TickUnit = float;

    TickUnit last_tick{};
    TickUnit tick{};
    TickUnit delta{};

    void NextTick();
};

} // namespace hyperion::v2

#endif