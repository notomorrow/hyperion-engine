#include "game_thread.h"
#include "engine.h"
#include "game.h"
#include "game_counter.h"

namespace hyperion::v2 {

static constexpr float game_thread_target_ticks_per_second = 60.0f;

GameThread::GameThread()
    : Thread("GameThread")
{
}

void GameThread::operator()(Engine *engine, Game *game, SystemWindow *window)
{
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);

    //game->Init(engine, window);

    while (engine->m_running) {
        while (counter.Waiting()) {
            /* wait */
        }

        counter.NextTick();

        game->Logic(engine, counter.delta);
    }

    game->Teardown(engine);
}

} // namespace hyperion::v2