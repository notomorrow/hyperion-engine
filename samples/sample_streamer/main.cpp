#include "SampleStreamer.hpp"

#include <editor/HyperionEditor.hpp>

#include <core/system/App.hpp>
#include <core/system/StackDump.hpp>
#include <core/system/ArgParse.hpp>

#include <core/containers/Bitset.hpp>

#include <core/logging/Logger.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/UUID.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/Format.hpp>

#include <HyperionEngine.hpp>

#include "asset/font_loaders/FontAtlasLoader.hpp"
#include "asset/ui_loaders/UILoader.hpp"

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
        signum
    );

    DebugLog(LogType::Debug, "%s\n", StackDump().ToString().Data());

    if (Engine::GetInstance()->m_stop_requested.Get(MemoryOrder::RELAXED)) {
        DebugLog(
            LogType::Warn,
            "Forcing stop\n"
        );

        fflush(stdout);

        exit(signum);

        return;
    }

    Engine::GetInstance()->RequestStop();

    // Wait for the render loop to stop
    while (Engine::GetInstance()->IsRenderLoopActive());

    exit(signum);
}

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    constexpr Tuple<int, float, double> tup { 1, 2.0f, 0.5 };
    constexpr auto x = tup.template GetElement< 2 >();
    DebugLog(LogType::Debug, "tup.getElement<1> = %f\n", x);

    constexpr auto concat_result = utilities::helpers::ConcatTuples(tup, Tuple< void*, float >{ (void*)(0x0), 32.0f });
    DebugLog(LogType::Debug, "Concat result: %d %f %f %p %f\n",
        concat_result.template GetElement< 0 >(),
        concat_result.template GetElement< 1 >(),
        concat_result.template GetElement< 2 >(),
        concat_result.template GetElement< 3 >(),
        concat_result.template GetElement< 4 >()
    );
    
    auto [a, b, c, d, e] = concat_result;

    constexpr auto get_result = get< 1 >(concat_result);

    constexpr auto parsed = detail::ParseTypeName< StaticString("struct hyperion::utilities::detail::PairImpl<10,10,class std::reference_wrapper<class hyperion::containers::detail::String<2> const >,class std::reference_wrapper<struct hyperion::EnqueuedAsset> >"), true >();
    DebugLog(LogType::Debug, "Parsed = %s\n", parsed.Data());
    HYP_BREAKPOINT;
    
    HyperionEditor editor;
    // SampleStreamer editor;
    App app;

    ArgParse arg_parse;
    arg_parse.Add("Headless", String::empty, ArgFlags::NONE, CommandLineArgumentType::BOOLEAN, false);    
    arg_parse.Add("Mode", "m", ArgFlags::NONE, Array<String> { "PrecompileShaders", "Streamer" }, String("Streamer"));

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        app.Launch(&editor, parse_result.result);
    } else {
        DebugLog(
            LogType::Error,
            "Failed to parse arguments!\n\t%s\n",
            parse_result.message.HasValue()
                ? parse_result.message->Data()
                : "<no message>"
        );

        return 1;
    }

    return 0;
}
