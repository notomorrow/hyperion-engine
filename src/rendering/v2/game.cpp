#include "game.h"
#include <rendering/v2/core/lib/small_vector.h>

#include <system/debug.h>

namespace hyperion::v2 {

Game::Game()
    : m_is_init(false)
{
    SmallVector<int, 5> sm;
    sm.PushBack(1);
    sm.PushBack(2);
    sm.PushBack(3);
    sm.PushBack(4);
    sm.PushBack(5);
    sm.PushBack(6);
    sm.PushBack(7);
    sm.PushBack(8);
}

Game::~Game()
{
    AssertExitMsg(
        !m_is_init,
        "Expected Game to have called Teardown() before destructor call"
    );
}

void Game::Init(Engine *engine, SystemWindow *window)
{
    m_is_init = true;
}

void Game::Teardown(Engine *engine)
{
    m_is_init = false;
}

} // namespace hyperion::v2