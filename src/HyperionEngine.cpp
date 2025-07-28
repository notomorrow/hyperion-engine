/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <core/object/HypClassRegistry.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/cli/CommandLine.hpp>

#include <console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>

#include <streaming/StreamingManager.hpp>

#include <rendering/Material.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#endif

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

#include <scene/ComponentInterface.hpp>

#include <audio/AudioManager.hpp>

#include <core/Handle.hpp>

#include <Engine.hpp>
#include <Game.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

Handle<Engine> g_engine {};
Handle<AssetManager> g_assetManager {};
Handle<EditorState> g_editorState {};
ShaderManager* g_shaderManager = nullptr;
MaterialCache* g_materialSystem = nullptr;
SafeDeleter* g_safeDeleter = nullptr;
IRenderBackend* g_renderBackend = nullptr;
RenderGlobalState* g_renderGlobalState = nullptr;
ShaderCompiler* g_shaderCompiler = nullptr;

static CommandLineArguments g_commandLineArguments;

static bool InitializeCommandLineArguments(int argc, char** argv)
{
    CommandLineParser argParse { &DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parseResult = argParse.Parse(argc, argv);

    if (parseResult.HasError())
    {
        const Error& error = parseResult.GetError();

        HYP_LOG(Core, Error, "Failed to parse command line arguments!\n\t{}", error.GetMessage().Any() ? error.GetMessage() : "<no message>");

        g_commandLineArguments = CommandLineArguments();

        return false;
    }

    GlobalConfig config { "app" };

    if (json::JSONValue configArgs = config.Get("app.args"))
    {
        json::JSONString configArgsString = configArgs.ToString();
        Array<String> configArgsStringSplit = configArgsString.Split(' ');

        parseResult = argParse.Parse(g_commandLineArguments.GetCommand(), configArgsStringSplit);

        if (parseResult.HasError())
        {
            HYP_LOG(Core, Error, "Failed to parse config command line value \"{}\":\n\t{}", configArgsString, parseResult.GetError().GetMessage());

            return false;
        }

        g_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), *parseResult, g_commandLineArguments);
    }

    return true;
}

HYP_API const CommandLineArguments& GetCommandLineArguments()
{
    return g_commandLineArguments;
}

HYP_API const FilePath& GetExecutablePath()
{
    static FilePath executablePath = FilePath(g_commandLineArguments.GetCommand()).BasePath();

    return executablePath;
}

HYP_API const FilePath& GetResourceDirectory()
{
    static struct ResourceDirectoryData
    {
        FilePath path;

        ResourceDirectoryData()
        {
#ifdef HYP_DEBUG_MODE
            path = FilePath(HYP_ROOT_DIR) / "res";
#else
            path = GetExecutablePath() / "res";
#endif
            Assert(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: %s", path.Data());

            Assert(path.CanRead(), "Resource directory is not readable: %s", path.Data());
            Assert(path.CanWrite(), "Resource directory is not writable: %s", path.Data());
        }
    } resourceDirectoryData;

    return resourceDirectoryData.path;
}

HYP_API bool InitializeEngine(int argc, char** argv)
{
    Threads::SetCurrentThreadId(g_mainThread);

    InitializeNameRegistry();

    HypClassRegistry::GetInstance().Initialize();

    if (!InitializeCommandLineArguments(argc, argv))
    {
        return false;
    }

    const FilePath basePath = GetExecutablePath();

    dotnet::DotNetSystem::GetInstance().Initialize(basePath);
    ConsoleCommandManager::GetInstance().Initialize();
    AudioManager::GetInstance().Initialize();
    TaskSystem::GetInstance().Start();

#ifdef HYP_VULKAN
    g_renderBackend = new VulkanRenderBackend();
#else
#error Unsupported rendering backend
#endif

    g_engine = CreateObject<Engine>();

    g_assetManager = CreateObject<AssetManager>();
    InitObject(g_assetManager);

#ifdef HYP_EDITOR
    g_editorState = CreateObject<EditorState>();
    InitObject(g_editorState);
#endif

    g_shaderManager = new ShaderManager;
    g_materialSystem = new MaterialCache;
    g_safeDeleter = new SafeDeleter;

    g_shaderCompiler = new ShaderCompiler;
    if (!g_shaderCompiler->LoadShaderDefinitions())
    {
        HYP_LOG(Core, Error, "Failed to load shader definitions!");
    }

    ComponentInterfaceRegistry::GetInstance().Initialize();

    return true;
}

HYP_API void DestroyEngine()
{
    Threads::AssertOnThread(g_mainThread);

    Assert(
        g_engine != nullptr,
        "Hyperion not initialized!");

    g_engine->FinalizeStop();

    dotnet::DotNetSystem::GetInstance().Shutdown();
    ComponentInterfaceRegistry::GetInstance().Shutdown();
    ConsoleCommandManager::GetInstance().Shutdown();
    AudioManager::GetInstance().Shutdown();

    if (TaskSystem::GetInstance().IsRunning())
    {
        TaskSystem::GetInstance().Stop();
    }

    g_assetManager.Reset();
    g_editorState.Reset();

    delete g_shaderCompiler;
    g_shaderCompiler = nullptr;

    delete g_shaderManager;
    g_shaderManager = nullptr;

    delete g_materialSystem;
    g_materialSystem = nullptr;

    g_engine.Reset();

    delete g_renderBackend;
    g_renderBackend = nullptr;
}

} // namespace hyperion