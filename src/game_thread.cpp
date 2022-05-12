#include "game_thread.h"
#include "engine.h"
#include "game.h"
#include "game_counter.h"

namespace hyperion::v2 {

GameThread::GameThread()
    : Thread("GameThread")
{
}

void GameThread::operator()(Engine *engine, Game *game, SystemWindow *window)
{
    GameCounter counter;

    //game->Init(engine, window);

    while (engine->m_running) {
        counter.NextTick();

        game->Logic(engine, counter.delta);
    }

    game->Teardown(engine);
}

} // namespace hyperion::v2