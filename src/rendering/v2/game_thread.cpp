#include "game_thread.h"
#include "engine.h"
#include "game.h"
#include "game_counter.h"

namespace hyperion::v2 {

GameThread::GameThread()
    : Thread("GameThread")
{
}

void GameThread::operator()(Engine *engine, Game *game)
{
    GameCounter counter;

    while (engine->m_running) {
        counter.NextTick();

        game->Logic(engine, counter.delta);
    }
}

} // namespace hyperion::v2