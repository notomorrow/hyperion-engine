#ifndef HYPERION_V2_GAME_H
#define HYPERION_V2_GAME_H

#include "game_counter.h"

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

class Engine;

class Game {
    friend class GameThread;

public:
    Game();
    virtual ~Game();

    virtual void Init(Engine *engine, SystemWindow *window);
    virtual void Teardown(Engine *engine);

    virtual void PreRender(Engine *engine) = 0;
    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) = 0;

protected:

private:
    bool m_is_init;
};

} // namespace hyperion::v2

#endif