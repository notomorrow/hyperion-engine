#include "game.h"

#include <iostream>
#include <GLFW/glfw3.h>

namespace apex {

Game::Game(const RenderWindow &window) 
    : inputmgr(new InputManager), 
      window(window)
{
}

Game::~Game()
{
    delete inputmgr;
}

} // namespace apex