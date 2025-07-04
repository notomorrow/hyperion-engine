#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/backend/RenderBackend.hpp>

#include <Game.hpp>
#include <GameThread.hpp>

#include <Engine.hpp>

#include <EngineGlobals.hpp>
#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace sys {

App& App::GetInstance()
{
    static App instance;
    return instance;
}

App::App()
{
}

App::~App()
{
}

void App::LaunchGame(const Handle<Game>& game)
{
    Threads::AssertOnThread(g_mainThread);

    Assert(game.IsValid());

    Handle<AppContextBase> appContext;

#ifdef HYP_SDL
    appContext = CreateObject<SDLAppContext>("Hyperion", GetCommandLineArguments());
#else
    HYP_FAIL("AppContext not implemented for this platform");
#endif

    appContext->SetGame(game);

    Vec2i resolution = { 1280, 720 };

    EnumFlags<WindowFlags> windowFlags = WindowFlags::HIGH_DPI;

    if (GetCommandLineArguments()["Headless"].ToBool())
    {
        windowFlags |= WindowFlags::HEADLESS;
    }

    if (GetCommandLineArguments()["ResX"].IsNumber())
    {
        resolution.x = GetCommandLineArguments()["ResX"].ToInt32();
    }

    if (GetCommandLineArguments()["ResY"].IsNumber())
    {
        resolution.y = GetCommandLineArguments()["ResY"].ToInt32();
    }

    if (!(windowFlags & WindowFlags::HEADLESS))
    {
        HYP_LOG(Core, Info, "Running in windowed mode: {}x{}", resolution.x, resolution.y);

        appContext->SetMainWindow(appContext->CreateSystemWindow({ "Hyperion Engine", resolution, windowFlags }));
    }
    else
    {
        HYP_LOG(Core, Info, "Running in headless mode");
    }

    Assert(g_renderBackend != nullptr);
    HYPERION_ASSERT_RESULT(g_renderBackend->Initialize(*appContext));

    RenderObjectDeleter<Platform::current>::Initialize();

    g_renderGlobalState = new RenderGlobalState();

    g_engine->SetAppContext(appContext);
    InitObject(g_engine);

    m_gameThread = MakeUnique<GameThread>(appContext);
    m_gameThread->SetGame(game);
    m_gameThread->Start();

    // Loop blocks the main thread until the game is done.
    Assert(g_engine->StartRenderLoop());

    delete g_renderGlobalState;
    g_renderGlobalState = nullptr;

    RenderObjectDeleter<Platform::current>::RemoveAllNow(/* force */ true);

    HYPERION_ASSERT_RESULT(g_renderBackend->Destroy());
}

} // namespace sys
} // namespace hyperion