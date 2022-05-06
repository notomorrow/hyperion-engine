#include "game.h"
#include <rendering/v2/core/lib/small_vector.h>

#include <system/debug.h>

namespace hyperion::v2 {

Game::Game()
    : m_is_init(false)
{
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