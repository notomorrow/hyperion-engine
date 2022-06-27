#ifndef HYPERION_V2_GAME_THREAD_H
#define HYPERION_V2_GAME_THREAD_H

#include "game_counter.h"

#include <core/thread.h>
#include <core/scheduler.h>
#include <core/containers.h>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

class Engine;
class Game;

class GameThread : public Thread<Scheduler<ScheduledFunction<void, GameCounter::TickUnit>>,
                                 Engine *, Game *, SystemWindow *>
{
public:
    GameThread();

private:
    virtual void operator()(Engine *engine, Game *game, SystemWindow *window) override;
};

} // namespace hyperion::v2

#endif