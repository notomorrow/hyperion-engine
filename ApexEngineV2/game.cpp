#include "game.h"

#include <iostream>
#include <GLFW/glfw3.h>

namespace apex {

Game::Game(const RenderWindow &window) 
    : window(window)
{
    inputmgr = new InputManager();
}

Game::~Game()
{
    delete inputmgr;
}

InputManager *Game::GetInputManager() const
{
    return inputmgr;
}

RenderWindow &Game::GetWindow()
{
    return window;
}
} // namespace apex