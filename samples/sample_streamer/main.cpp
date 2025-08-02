#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>
#include <Engine.hpp>

using namespace hyperion;

namespace hyperion {
HYP_DEFINE_LOG_CHANNEL(Core);
}

void HandleSignal(int signum)
{
    // Dump stack trace
    DebugLog(
        LogType::Warn,
        "Received signal %d\n",
        signum);

    Engine::GetInstance()->RequestStop();

    // Wait for the render loop to stop
    while (Engine::GetInstance()->IsRenderLoopActive())
        ;

    exit(signum);
}

int main(int argc, char** argv)
{
    signal(SIGINT, HandleSignal);

    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    if (!hyperion::InitializeEngine(argc, argv))
    {
        return 1;
    }

    Handle<HyperionEditor> editorInstance = CreateObject<HyperionEditor>();

    App::GetInstance().LaunchGame(editorInstance);

    return 0;
}
