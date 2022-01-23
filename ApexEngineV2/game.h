#ifndef GAME_H
#define GAME_H

#include "render_window.h"
#include "rendering/renderer.h"
#include "input_manager.h"
#include "rendering/ui/ui_manager.h"

namespace hyperion {
class Game {
public:
    Game(const RenderWindow &window);
    virtual ~Game();

    inline InputManager *GetInputManager() const { return m_input_manager; }
    inline UIManager *GetUIManager() const { return m_ui_manager; }
    inline Renderer *GetRenderer() { return m_renderer; }
    inline const Renderer *GetRenderer() const { return m_renderer; }

    virtual void Initialize() = 0;
    void Update(double dt);
    virtual void Logic(double dt) = 0;
    virtual void Render() = 0;

protected:
    InputManager *m_input_manager;
    UIManager *m_ui_manager;
    Renderer * const m_renderer;
};
} // namespace hyperion

#endif
