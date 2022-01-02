#include "game.h"

namespace apex {

Game::Game(const RenderWindow &window) 
    : inputmgr(new InputManager),
      m_renderer(new Renderer()), 
      window(window)
{
}

Game::~Game()
{
    delete inputmgr;
    delete m_renderer;
}

} // namespace apex