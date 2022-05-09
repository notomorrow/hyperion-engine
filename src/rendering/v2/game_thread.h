#ifndef HYPERION_V2_GAME_THREAD_H
#define HYPERION_V2_GAME_THREAD_H

#include <rendering/v2/core/containers.h>
#include "core/thread.h"

#define HYP_GAME_THREAD 1

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

class Engine;
class Game;

class GameThread : public Thread<Engine *, Game *, SystemWindow *> {
public:
    GameThread();

private:
    virtual void operator()(Engine *engine, Game *game, SystemWindow *window) override;
};

} // namespace hyperion::v2

#endif