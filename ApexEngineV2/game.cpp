#include "game.h"

namespace apex {

Game::Game(const RenderWindow &window) 
    : m_input_manager(new InputManager),
      m_ui_manager(new UIManager(m_input_manager)),
      m_renderer(new Renderer(window))
{
}

Game::~Game()
{
    delete m_input_manager;
    delete m_ui_manager;
    delete m_renderer;
}

void Game::Update(double dt)
{
    m_ui_manager->Update(dt);

    Logic(dt);
}

} // namespace apex
