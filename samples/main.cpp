#include "SampleStreamer.hpp"

#include <HyperionEngine.hpp>
#include <core/lib/AtomicVar.hpp>

using namespace hyperion;

void HandleSignal(int signum)
{
    if (g_engine->m_stop_requested.Get(MemoryOrder::RELAXED)) {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n"
        );

        fflush(stdout);

        exit(signum);

        return;
    }

    g_engine->RequestStop();

    while (g_engine->IsRenderLoopActive());

    exit(signum);
}

int main()
{
    signal(SIGINT, HandleSignal);
    
    RC<Application> application(new SDLApplication("My Application"));
    application->SetCurrentWindow(application->CreateSystemWindow({
        "Hyperion Engine",
        1024,
        1024,
        true
    }));
    
    hyperion::InitializeApplication(application);

    Game *my_game = new SampleStreamer(application);
    g_engine->InitializeGame(my_game);

    UInt num_frames = 0;
    float delta_time_accum = 0.0f;
    GameCounter counter;

    SystemEvent event;

    // AssertThrow(server.Start());

     while (g_engine->IsRenderLoopActive()) {
        // input manager stuff
        while (application->PollEvent(event)) {
            my_game->HandleEvent(std::move(event));
        }

        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (num_frames >= 250) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / Float(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }
        
        g_engine->RenderNextFrame(my_game);
    }

    delete my_game;

    return 0;
}
