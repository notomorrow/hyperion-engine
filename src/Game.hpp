/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_HPP
#define HYPERION_GAME_HPP

#include <GameCounter.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/resource/Resource.hpp>

#include <core/utilities/Optional.hpp>

#include <core/Defines.hpp>

#include <system/AppContext.hpp>

#include <input/InputManager.hpp>

#include <scene/Scene.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

class Engine;
class GameThread;
class UISubsystem;
class RenderScene;

struct ManagedGameInfo
{
    String assembly_name;
    String class_name;
};

class Game
{
    friend class GameThread;
    friend class Engine;

public:
    HYP_API Game();
    HYP_API Game(Optional<ManagedGameInfo> managed_game_info);
    HYP_API virtual ~Game();

    const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    const RC<AppContextBase>& GetAppContext() const
    {
        return m_app_context;
    }

    void SetAppContext(const RC<AppContextBase>& app_context)
    {
        m_app_context = app_context;
    }

    GameThread* GetGameThread() const
    {
        return m_game_thread.Get();
    }

    HYP_API virtual void Init();
    HYP_API virtual void Update(GameCounter::TickUnit delta) final;
    HYP_API virtual void Teardown();

    HYP_API virtual void HandleEvent(SystemEvent&& event) final;
    HYP_API virtual void PushEvent(SystemEvent&& event) final;
    HYP_API virtual void OnInputEvent(const SystemEvent& event);

protected:
    virtual void Logic(GameCounter::TickUnit delta) = 0;

    const RC<UISubsystem>& GetUISubsystem() const
    {
        return m_ui_subsystem;
    }

    RC<AppContextBase> m_app_context;

    Handle<Scene> m_scene;

    RC<UISubsystem> m_ui_subsystem;

    RC<dotnet::Assembly> m_managed_assembly;
    dotnet::Object* m_managed_game_object;

private:
    void Init_Internal();

    void RequestStop();

    bool m_is_init;

    Optional<ManagedGameInfo> m_managed_game_info;

    UniquePtr<GameThread> m_game_thread;

    RenderScene* m_render_scene;
    ResourceHandle m_render_scene_handle;
};

} // namespace hyperion

#endif