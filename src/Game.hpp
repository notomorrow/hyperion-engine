#ifndef HYPERION_V2_GAME_H
#define HYPERION_V2_GAME_H

#include "GameCounter.hpp"
#include <input/InputManager.hpp>
#include <scene/Scene.hpp>

namespace hyperion {

class SystemWindow;
class SystemEvent;

namespace renderer {

class Frame;

} // namespace renderer

} // namespace hyperion

namespace hyperion::v2 {

using renderer::Frame;

class Engine;

class Game
{
    friend class GameThread;

public:
    Game();
    virtual ~Game();

    virtual void Init(Engine *engine, SystemWindow *window) final;
    virtual void Teardown(Engine *engine);

    virtual void InitRender(Engine *engine);
    virtual void InitGame(Engine *engine);

    virtual void OnFrameBegin(Engine *engine, Frame *frame) = 0;
    virtual void OnFrameEnd(Engine *engine, Frame *frame) = 0;
    virtual void Logic(Engine *engine, GameCounter::TickUnit delta) = 0;

    virtual void HandleEvent(Engine *engine, SystemEvent &event) final;
    virtual void OnInputEvent(Engine *engine, const SystemEvent &event);

protected:
    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    InputManager *GetInputManager() const
        { return m_input_manager; }

    InputManager *m_input_manager;
    Handle<Scene> m_scene;

private:
    bool m_is_init;
};

} // namespace hyperion::v2

#endif