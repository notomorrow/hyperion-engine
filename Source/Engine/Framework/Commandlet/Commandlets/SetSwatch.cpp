#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/Game.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

class SetSwatch final : public CommandletBase
{
    HYP_OBJECT_BODY(SetSwatch);

public:
    virtual ~SetSwatch() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "swatch", "", "Name of the Swatch to set as active on the current World",
                CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String swatchName = args["swatch"].ToString();

        if (swatchName.Empty())
        {
            return HYP_MAKE_ERROR(Error, "No swatch name provided");
        }

        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [swatchName]()
            {
                Handle<World> world;

#ifdef HYP_EDITOR
                if (g_editorState.IsValid())
                {
                    if (Handle<EditorProject> project = g_editorState->GetCurrentProject(); project.IsValid())
                    {
                        world = project->GetWorld();
                    }
                }
#endif // HYP_EDITOR

                if (!world.IsValid())
                {
                    if (g_gameInstance == nullptr)
                    {
                        HYP_LOG(Console, Error, "SetSwatch: no active game instance");

                        return;
                    }

                    world = g_gameInstance->GetWorld();
                }

                if (!world.IsValid())
                {
                    HYP_LOG(Console, Error, "SetSwatch: no active world");

                    return;
                }

                const Name name = CreateNameFromDynamicString(ANSIString(swatchName));

                if (!world->TryGetSwatch(name).IsValid())
                {
                    HYP_LOG(Console, Error, "SetSwatch: swatch '{}' not found on World '{}'; available swatches: [{}]",
                        name, world->GetName(), String::Join(world->GetSwatchNames(), ", "));

                    return;
                }

                world->SetActiveSwatch(name);

                HYP_LOG(Console, Info, "SetSwatch: active swatch set to '{}'", name);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return {};
    }
};

ENGINE_API const Class* g_clsSetSwatch = nullptr;

const Class* SetSwatch::StaticClass()
{
    return g_clsSetSwatch;
}

// clang-format off

HYP_BEGIN_CLASS(SetSwatch, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "setswatch"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(SetSwatch);

} // namespace Hyperion
