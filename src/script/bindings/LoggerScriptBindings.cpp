#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/StringUtil.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMString.hpp>

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(HypScript);

namespace script {
namespace bindings {

static String FormatString(const String &str, const Array<vm::Value> &args)
{
    String result;

    int unnumbered_arg_index = 0;

    for (SizeType i = 0; i < str.Size(); ++i) {
        if (str[i] == '{') {
            if (i + 1 < str.Size() && str[i + 1] == '{') {
                result += '{';
                ++i;
            } else {
                SizeType j = i + 1;

                while (j < str.Size() && str[j] != '}') {
                    ++j;
                }

                if (j >= str.Size()) {
                    break;
                }

                String index_str = str.Substr(i + 1, j);

                int index = 0;

                if (index_str.Any()) {
                    if (!StringUtil::Parse<int>(index_str, &index)) {
                        i = j;

                        continue;
                    }
                } else {
                    index = unnumbered_arg_index++;
                }

                if (index < args.Size()) {
                    result += args[index].ToString().GetString();
                }

                i = j;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

static struct LoggerScriptBindings : ScriptBindingsBase
{
    LoggerScriptBindings()
        : ScriptBindingsBase(TypeId::ForType<Logger>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {
        context.Class<Logger>("Logger")
            .StaticMember("DEBUG", "uint", vm::Value { vm::Value::U32, { .u32 = logging::Debug().value }})
            .StaticMember("INFO", "uint", vm::Value { vm::Value::U32, { .u32 = logging::Info().value } })
            .StaticMember("WARN", "uint", vm::Value { vm::Value::U32, { .u32 = logging::Warning().value } })
            .StaticMember("ERROR", "uint", vm::Value { vm::Value::U32, { .u32 = logging::Error().value } })
            .StaticMethod(
                "log",
                "function< int, Class, uint, String, varargs<any> >",
                [](sdk::Params params)
                {
                    HYP_SCRIPT_CHECK_ARGS(>=, 3);

                    vm::Number log_level_num;
                    if (!params.args[1]->GetSignedOrUnsigned(&log_level_num)) {
                        HYP_SCRIPT_THROW(vm::Exception ("log() expects a number as the first argument"));
                    }

                    const uint64 log_level = (log_level_num.flags & vm::Number::FLAG_SIGNED)
                        ? uint64(log_level_num.i)
                        : log_level_num.u;

                    vm::VMString *format_string_ptr = nullptr;
                    if (!params.args[2]->GetPointer<vm::VMString>(&format_string_ptr)) {
                        HYP_SCRIPT_THROW(vm::Exception ("log() expects a string as the first argument"));
                    }

                    Array<vm::Value> args_array;
                    args_array.Reserve(params.nargs - 3);

                    for (SizeType i = 3; i < params.nargs; ++i) {
                        args_array.PushBack(*params.args[i]);
                    }

                    String formatted_string = FormatString(
                        format_string_ptr->GetString(),
                        args_array
                    );

                    switch (log_level)
                    {
                    case logging::Debug().value:
                        LogDynamic<logging::Debug(), HYP_MAKE_CONST_ARG(&g_logChannel_HypScript)>(Logger::GetInstance(), formatted_string.Data());
                        break;
                    case logging::Info().value:
                        LogDynamic<logging::Info(), HYP_MAKE_CONST_ARG(&g_logChannel_HypScript)>(Logger::GetInstance(), formatted_string.Data());
                        break;
                    case logging::Warning().value:
                        LogDynamic<logging::Warning(), HYP_MAKE_CONST_ARG(&g_logChannel_HypScript)>(Logger::GetInstance(), formatted_string.Data());
                        break;
                    case logging::Error().value:
                        LogDynamic<logging::Error(), HYP_MAKE_CONST_ARG(&g_logChannel_HypScript)>(Logger::GetInstance(), formatted_string.Data());
                        break;
                    case logging::Fatal().value:
                        LogDynamic<logging::Fatal(), HYP_MAKE_CONST_ARG(&g_logChannel_HypScript)>(Logger::GetInstance(), formatted_string.Data());
                        break;
                    default:
                        break;
                    }

                    HYP_SCRIPT_RETURN_INT32(params.nargs - 3);
                }
            )
            .StaticMethod(
                "print",
                "function< void, Class, String >",
                CxxFn<
                    void, const void *, const String &,
                    [](const void *self, const String &str) -> void
                    {
                        std::printf("%s", str.Data());
                    }
                >
            )
            .Build();
    }

} logger_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion