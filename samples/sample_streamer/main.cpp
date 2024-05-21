#include "SampleStreamer.hpp"

#include <editor/HyperionEditor.hpp>

#include <core/system/App.hpp>
#include <core/system/StackDump.hpp>
#include <core/system/ArgParse.hpp>

#include <HyperionEngine.hpp>

#include "asset/font_loaders/FontAtlasLoader.hpp"
#include "asset/ui_loaders/UILoader.hpp"

using namespace hyperion;

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

#if 1


// strip "class " or "struct " from beginning StaticString
template <auto Str>
constexpr auto StripClassOrStruct()
{
    constexpr auto class_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("class ") > >();
    constexpr auto struct_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("struct ") > >();

    if constexpr (class_index != -1 && (struct_index == -1 || class_index <= struct_index)) {
        return containers::helpers::Substr<Str, class_index + 6, Str.Size()>::value; // 6 = length of "class "
    } else if constexpr (struct_index != -1 && (class_index == -1 || struct_index <= class_index)) {
        return containers::helpers::Substr<Str, struct_index + 7, Str.Size()>::value; // 7 = length of "struct "
    } else {
        return Str;
    }
}

template <auto Str>
constexpr auto StripNestedClassOrStruct()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();
    
    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {
        constexpr auto before_left_arrow = StripNestedClassOrStruct< containers::helpers::Substr< Str, 0, left_arrow_index >::value >();
        constexpr auto inner_left_arrow = StripNestedClassOrStruct< containers::helpers::Substr <Str, left_arrow_index + 1, right_arrow_index >::value >();

        constexpr auto inner_split = SplitStaticString<inner_left_arrow, ','>();

        return ((before_left_arrow.template Concat< StaticString("<")>()).template Concat< inner_left_arrow >()).template Concat<StaticString(">")>();
    } else {
        return StripClassOrStruct< Str >();
    }
}

// constexpr functions to strip namespaces from StaticString

struct StripNamespaceTransformer
{
    static constexpr char delimiter = ',';

    template <auto String>
    static constexpr auto Transform()
    {
        constexpr auto trimmed = containers::helpers::Trim< String >::value;
        constexpr auto last_index = trimmed.template FindLast< containers::detail::IntegerSequenceFromString< StaticString("::") > >();

        if constexpr (last_index == -1) {
            return detail::StripClassOrStruct< containers::helpers::Trim< String >::value >();
        } else {
            return detail::StripClassOrStruct< containers::helpers::Substr< containers::helpers::Trim< String >::value, last_index + 2, SizeType(-1) >::value >();
        }
    }
};

template <auto Str>
constexpr auto StripNamespace()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();

    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {
        return containers::helpers::Concat<
            StripNamespace< containers::helpers::Substr< Str, 0, left_arrow_index>::value >(),
            StaticString("<"),
            StripNamespace< containers::helpers::Substr< Str, left_arrow_index + 1, right_arrow_index>::value >(),
            StaticString(">")
        >::value;
    } else {
        return containers::helpers::TransformSplit< StripNamespaceTransformer, Str >::value;
    }
}

#endif



struct MyStaticStringTransformer
{
    static constexpr char delimiter = ',';

    template <auto String>
    static constexpr auto Transform()
    {
        return containers::helpers::Trim< String >::value; // string;//.Trim();
    }
};

int main(int argc, char **argv)
{
    signal(SIGINT, HandleSignal);
    
    // handle fatal crashes
    signal(SIGSEGV, HandleSignal);

    auto font_atlas_type_id = TypeID::ForType<RC<FontAtlas>>();
    auto font_atlas_type_name = TypeNameWithoutNamespace<RC<FontAtlas>>();
    auto ui_object_type_id = TypeID::ForType<RC<UIObject>>();
    auto ui_object_type_name = TypeNameWithoutNamespace<RC<UIObject>>();

    // constexpr auto tuple_type = containers::helpers::Split<StaticString("apple,orange,pear,plum,peach"), ','>::value;
    // constexpr auto concated = containers::helpers::Concat< StaticString("apple"), StaticString("blueberry"), StaticString("pear"), StaticString("banana") >::value;
    // constexpr auto s = containers::helpers::Substr< StaticString("blueberry"), 3, 5 >::value;

    // constexpr auto split_type_2 = containers::helpers::TransformParts<',', MyStaticStringTransformer>(StaticString("apple, orange,  pear, plum, peach"))::value;
    constexpr auto split_type_2 = containers::helpers::TransformParts<MyStaticStringTransformer, StaticString(" apple  "), StaticString(" orange "), StaticString("   pear"), StaticString("\nplum")>::value;
    constexpr auto split_type_3 = containers::helpers::TransformSplit<MyStaticStringTransformer, StaticString("apple  ,   orange, pear,  plum,   peach,")>::value;

    constexpr auto split_indices = containers::helpers::GetSplitIndices< StaticString("class hyperion::RefCountedPtr"), ',' >::value;

    constexpr auto stripped_ns = TypeNameWithoutNamespace< UTF16String >();
    DebugLog(LogType::Debug, "stripped ns: %s\n", stripped_ns.Data());

    constexpr auto trim_test = containers::helpers::Trim< StaticString("   blah   ") >::value;
    constexpr auto x = StaticString("   blah   ").FindTrimLastIndex_Right_Impl();
    DebugLog(LogType::Debug, "Trim test: %s\tlength: %u\n", trim_test.Data(), trim_test.Size());

    // constexpr auto tup_concated = containers::helpers::Concat<std::get<0>(tuple_type), std::get<1>(tuple_type), std::get<2>(tuple_type), std::get<3>(tuple_type)>::value;

    // constexpr auto rejoined = RejoinStaticStrings<StaticString("apple,orange,pear,plum,peach")>();

        //std::tuple_cat(std::tuple { StaticString("hello") }, std::tuple { StaticString("world!") });
    // DebugLog(LogType::Debug, "Tuple [0] = %s\n", std::get<0>(tuple_type).Data());
    DebugLog(LogType::Debug, "split_type_2 = %s\n", split_type_2.Data());//std::get<2>(split_type_2).Data());
    DebugLog(LogType::Debug, "split_type_3 = %s\n", split_type_3.Data());//std::get<2>(split_type_2).Data());
    //DebugLog(LogType::Debug, "Joined = %s\n", std::get<0>(joined).Data());
    // DebugLog(LogType::Debug, "concated = %s\n", concated.Data());
    // DebugLog(LogType::Debug, "tup_concated = %s\n", tup_concated.Data());
    // DebugLog(LogType::Debug, "rejoined = %s\n", rejoined.Data());
    //DebugLog(LogType::Debug, "Tuple [1] = %s\n", std::get<1>(tuple_type).Data());

    AssertThrow(font_atlas_type_id != ui_object_type_id);
    DebugLog(LogType::Debug, "Font atlas type name : %s\n", font_atlas_type_name.Data());
    DebugLog(LogType::Debug, "UI type name : %s\n", ui_object_type_name.Data());

    // HYP_BREAKPOINT;

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
