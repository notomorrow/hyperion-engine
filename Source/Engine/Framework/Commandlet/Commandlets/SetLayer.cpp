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
#include <Scene/Layer.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);

class SetLayer final : public CommandletBase
{
    HYP_OBJECT_BODY(SetLayer);

public:
    virtual ~SetLayer() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "layer", "", "Name of the Layer to enable / disable on the current World",
                CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING);

            s_definitions.Add(
                "enable", "", "1 / true to enable, 0 / false to disable. If not provided, the Layer's state is toggled.",
                CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN);
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String layerName = args["layer"].ToString();

        if (layerName.Empty())
        {
            return HYP_MAKE_ERROR(Error, "No layer name provided");
        }

        const bool hasEnableValue = args.Contains("enable");
        const bool enableValue = args["enable"].ToBool();

        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [layerName, hasEnableValue, enableValue]()
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
                        HYP_LOG(Console, Error, "SetLayer: no active game instance");

                        return;
                    }

                    world = g_gameInstance->GetWorld();
                }

                if (!world.IsValid())
                {
                    HYP_LOG(Console, Error, "SetLayer: no active world");

                    return;
                }

                const Name name = CreateNameFromDynamicString(ANSIString(layerName));

                if (!world->TryGetLayer(name).IsValid())
                {
                    HYP_LOG(Console, Error, "SetLayer: layer '{}' not found on World '{}'; available layers: [{}]",
                        name, world->GetName(), String::Join(world->GetLayerNames(), ", "));

                    return;
                }

                const bool layerActive = hasEnableValue
                    ? enableValue
                    : !world->IsLayerActive(name);

                world->SetLayerActive(name, layerActive);

                HYP_LOG(Console, Info, "SetLayer: layer '{}' {} on World '{}'",
                    name, layerActive ? "enabled" : "disabled", world->GetName());
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return {};
    }
};

ENGINE_API const Class* g_clsSetLayer = nullptr;

const Class* SetLayer::StaticClass()
{
    return g_clsSetLayer;
}

// clang-format off

HYP_BEGIN_CLASS(SetLayer, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "setlayer"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(SetLayer);

} // namespace Hyperion
