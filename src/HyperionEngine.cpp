/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <scene/Material.hpp>

#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/threading/Threads.hpp>

#include <system/MessageBox.hpp>

#include <rendering/SafeDeleter.hpp>

#include <Game.hpp>

namespace hyperion {

HYP_API void InitializeAppContext(const RC<AppContext> &app_context, Game *game)
{
    AssertThrow(app_context != nullptr);

    AssertThrow(game != nullptr);
    game->SetAppContext(app_context);

    Threads::AssertOnThread(g_main_thread);

    AssertThrowMsg(g_engine.IsValid(), "Engine is null!");

    g_engine->Initialize(app_context);
}

HYP_API void InitializeEngine(const FilePath &base_path)
{
    Threads::SetCurrentThreadID(g_main_thread);
    
    HypClassRegistry::GetInstance().Initialize();

    dotnet::DotNetSystem::GetInstance().Initialize(base_path);

    g_engine = CreateObject<Engine>();
    InitObject(g_engine);

    g_asset_manager = CreateObject<AssetManager>();
    InitObject(g_asset_manager);

    g_shader_manager = new ShaderManager;
    g_material_system = new MaterialCache;
    g_safe_deleter = new SafeDeleter;

    ComponentInterfaceRegistry::GetInstance().Initialize();
}

HYP_API void DestroyEngine()
{
    Threads::AssertOnThread(g_main_thread);

    AssertThrowMsg(
        g_engine != nullptr,
        "Hyperion not initialized!"
    );

    g_engine->FinalizeStop();

    dotnet::DotNetSystem::GetInstance().Shutdown();
    ComponentInterfaceRegistry::GetInstance().Shutdown();

    g_asset_manager.Reset();

    delete g_shader_manager;
    g_shader_manager = nullptr;

    delete g_material_system;
    g_material_system = nullptr;

    g_engine.Reset();
}

} // namespace hyperion