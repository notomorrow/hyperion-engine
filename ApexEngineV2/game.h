#ifndef GAME_H
#define GAME_H

#include "render_window.h"
#include "rendering/renderer.h"
#include "input_manager.h"

namespace apex {
class Game {
public:
    Game(const RenderWindow &window);
    virtual ~Game();

    inline InputManager *GetInputManager() const { return inputmgr; }
    inline RenderWindow &GetWindow() { return window; }
    inline Renderer *GetRenderer() { return m_renderer; }
    inline const Renderer *GetRenderer() const { return m_renderer; }

    virtual void Initialize() = 0;
    virtual void Logic(double dt) = 0;
    virtual void Render() = 0;

protected:
    InputManager *inputmgr;
    RenderWindow window;
    Renderer * const m_renderer;
};
} // namespace apex

#endif