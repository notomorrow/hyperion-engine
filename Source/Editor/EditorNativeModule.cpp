/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorProjectSettings.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/DetachedScene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/Systems/ScriptSystem.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/ObjectPool.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/ScriptObjectFunctions.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedClass.hpp>
#endif

#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

namespace {

using GetGameClassNameFn = const char* (*)();
using GetEngineVersionFn = void (*)(uint32*, uint32*, uint32*);

struct LoadedNativeModule
{
    FilePath modulePath;
    FilePath copyPath;
    String gameClassName;
    String error;
    Time loadedTimestamp;
    UIntPtr imageBegin = 0;
    UIntPtr imageEnd = 0;
    bool isLoaded = false;
    bool warnedRebuilt = false;
    bool hasPlayed = false;
    bool hasBindings = false;

    bool CanUnload() const
    {
        return imageEnd > imageBegin;
    }
};

struct RetiredNativeModule
{
    FilePath copyPath;
    UIntPtr imageBegin = 0;
    UIntPtr imageEnd = 0;
    Array<const Class*> classes;
    Array<ObjectContainerBase*> containers;
    Array<const ComponentInterface*> componentInterfaces;
    Time retiredTime;
    Time lastCollectTime;
    uint32 requiredCollection = 0;
    uint32 playsStoppedSinceRetired = 0;
    bool hasPlayed = false;
    bool warned = false;
};

#ifdef HYP_DOTNET
struct LoadedManagedModule
{
    FilePath modulePath;
    SharedPtr<dotnet::Assembly> assembly;
    SharedPtr<dotnet::ManagedClass> gameClass;
    String error;
    Time loadedTimestamp;
    bool needsRetarget = false;
    bool applyFailed = false;
};

static Array<LoadedManagedModule> s_loadedManagedModules;
#endif

static Array<LoadedNativeModule> s_loadedModules;
static Array<RetiredNativeModule> s_retiredModules;
static Mutex s_loadedModulesMutex;
static uint32 s_moduleCopyCounter = 0;

static Time s_lastModulePollTime;
static Time s_pendingModuleTimestamp;
static bool s_wasSimulating = false;

static AtomicVar<uint32> s_completedCollections { 0 };
static AtomicVar<bool> s_isCollecting { false };

static CVar<bool> s_cvAutoReloadGameModule { "Editor.AutoReloadGameModule", true };

static const char* GetModulePlatformDirectory()
{
#ifdef HYP_WINDOWS
    return "Windows";
#elif defined(HYP_MACOS)
    return "Mac";
#else
    return "Linux";
#endif
}

static void CollectManagedGarbage()
{
#ifdef HYP_DOTNET
    if (TriggerGCFunction triggerGc = DotNETHost::GetInstance().GetGlobalFunctions().triggerGcFunction)
    {
        triggerGc();
    }
#endif
}

static bool CanWaitForManagedGarbage()
{
#ifdef HYP_DOTNET
    return DotNETHost::GetInstance().GetGlobalFunctions().collectGarbageAndWaitFunction != nullptr;
#else
    return false;
#endif
}

// collects and waits for finalizers off the sim thread, since finalizers can need it
static void StartWaitingForManagedGarbage()
{
#ifdef HYP_DOTNET
    TriggerGCFunction collectAndWait = DotNETHost::GetInstance().GetGlobalFunctions().collectGarbageAndWaitFunction;

    if (collectAndWait == nullptr || s_isCollecting.Exchange(true, MemoryOrder::ACQUIRE_RELEASE))
    {
        return;
    }

    TaskSystem::GetInstance().Enqueue(
        [collectAndWait]()
        {
            collectAndWait();

            s_completedCollections.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
            s_isCollecting.Set(false, MemoryOrder::RELEASE);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
}

static LoadedNativeModule* FindLoadedModule(const FilePath& modulePath)
{
    for (LoadedNativeModule& loadedModule : s_loadedModules)
    {
        if (loadedModule.modulePath == modulePath)
        {
            return &loadedModule;
        }
    }

    return nullptr;
}

static bool PollModuleRebuilt(const FilePath& modulePath, Time loadedTimestamp)
{
    const Time now = Time::Now();

    if (!s_cvAutoReloadGameModule.Get() || int64(now - s_lastModulePollTime) < 1000)
    {
        return false;
    }

    s_lastModulePollTime = now;

    const Time timestamp = modulePath.Exists() ? modulePath.LastModifiedTimestamp() : Time();

    if (timestamp <= loadedTimestamp)
    {
        return false;
    }

    if (timestamp == s_pendingModuleTimestamp)
    {
        return true;
    }

    s_pendingModuleTimestamp = timestamp;

    return false;
}

static void EvictModuleTypeInfos(const RetiredNativeModule& retiredModule)
{
    Array<TypeId> typeIds;

    for (const Class* cls : retiredModule.classes)
    {
        typeIds.PushBack(cls->GetTypeId());
    }

    TypeInfo_EvictForLibrary(retiredModule.imageBegin, retiredModule.imageEnd, typeIds);
}

static RetiredNativeModule RetireModule(const LoadedNativeModule& loadedModule)
{
    RetiredNativeModule retiredModule;
    retiredModule.copyPath = loadedModule.copyPath;
    retiredModule.imageBegin = loadedModule.imageBegin;
    retiredModule.imageEnd = loadedModule.imageEnd;
    retiredModule.classes = ClassRegistry::GetInstance().UnregisterClassesInRange(loadedModule.imageBegin, loadedModule.imageEnd);
    retiredModule.containers = GetObjectContainerMap().Detach(retiredModule.classes);
    retiredModule.retiredTime = Time::Now();
    retiredModule.hasPlayed = loadedModule.hasPlayed;

    EvictModuleTypeInfos(retiredModule);

    CVarManager::GetInstance().UnregisterVarsInRange(loadedModule.imageBegin, loadedModule.imageEnd);

    retiredModule.componentInterfaces = ComponentInterfaceRegistry::GetInstance().UnregisterComponentsInRange(loadedModule.imageBegin, loadedModule.imageEnd);

    return retiredModule;
}

static void UnloadRetiredModule(RetiredNativeModule& retiredModule)
{
    // its code may have made more TypeInfos since it was retired
    EvictModuleTypeInfos(retiredModule);

    // the containers' destructors are in the module
    for (ObjectContainerBase* container : retiredModule.containers)
    {
        delete container;
    }

    retiredModule.containers.Clear();

#ifdef HYP_DOTNET
    // the game's C# bindings assembly stays loaded, and unloading it at shutdown would reach its classes in this module
    for (const Class* cls : retiredModule.classes)
    {
        if (SharedPtr<dotnet::ManagedClass> managedClass = cls->GetManagedClass(); managedClass && managedClass->GetClass() == cls)
        {
            managedClass->DetachClass();
        }

        cls->SetManagedClass(nullptr);
    }
#endif

    DynamicLibraryCache::GetInstance().UnloadLibrary(PlatformString(String(retiredModule.copyPath)));

    std::error_code errorCode;
    std::filesystem::remove(retiredModule.copyPath.Data(), errorCode);

    HYP_LOG(Editor, Info, "Unloaded previous game module build {}", retiredModule.copyPath);
}

static bool IsRetiredModuleClass(const Class* cls)
{
    for (const RetiredNativeModule& retiredModule : s_retiredModules)
    {
        if (retiredModule.classes.Contains(cls))
        {
            return true;
        }
    }

    return false;
}

static void UpdateRetiredModules()
{
    const Time now = Time::Now();

    // the world syncs its own scenes each update; nothing else syncs the detached scene, where removed nodes are kept for undo
    if (s_retiredModules.Any())
    {
        GetDetachedSceneForCurrentThread().GetEntityManager()->SyncRuntimeComponentTypes();
    }

    for (size_t index = s_retiredModules.Size(); index-- > 0;)
    {
        RetiredNativeModule& retiredModule = s_retiredModules[index];

        Array<const Class*> classesWithObjects;

        for (ObjectContainerBase* container : retiredModule.containers)
        {
            if (container->HasObjects())
            {
                classesWithObjects.PushBack(container->GetClass());
            }
        }

        // component containers still on its layouts call its code; they move to the new build's as their EntityManagers sync
        for (const ComponentInterface* componentInterface : retiredModule.componentInterfaces)
        {
            if (componentInterface->GetNumContainers() != 0)
            {
                classesWithObjects.PushBack(componentInterface->GetStruct());
            }
        }

        if (classesWithObjects.Any())
        {
            retiredModule.requiredCollection = 0;

            // managed wrappers hold their objects until they're collected
            if (int64(now - retiredModule.lastCollectTime) >= 2000)
            {
                CollectManagedGarbage();

                retiredModule.lastCollectTime = now;
            }

            if (!retiredModule.warned && int64(now - retiredModule.retiredTime) >= 30000)
            {
                String classNames;

                for (const Class* cls : classesWithObjects)
                {
                    if (classNames.Any())
                    {
                        classNames += ", ";
                    }

                    classNames += *cls->GetName();
                }

                HYP_LOG(Editor, Warning, "Previous game module build {} stays loaded while objects of {} are alive", retiredModule.copyPath, classNames);

                retiredModule.warned = true;
            }

            continue;
        }

        // what its code made during play can outlive the session, held by the editor until the next play
        if (retiredModule.hasPlayed && retiredModule.playsStoppedSinceRetired == 0)
        {
            continue;
        }

        // C# objects that are garbage but not finalized yet can still hold what its code made
        if (CanWaitForManagedGarbage())
        {
            if (retiredModule.requiredCollection == 0)
            {
                retiredModule.requiredCollection = s_completedCollections.Get(MemoryOrder::ACQUIRE) + (s_isCollecting.Get(MemoryOrder::ACQUIRE) ? 2 : 1);
            }

            if (s_completedCollections.Get(MemoryOrder::ACQUIRE) < retiredModule.requiredCollection)
            {
                StartWaitingForManagedGarbage();

                continue;
            }

            retiredModule.requiredCollection = 0;

            const bool shouldSkip = AnyOf(retiredModule.containers, &ObjectContainerBase::HasObjects)
                || AnyOf(retiredModule.componentInterfaces, [](const ComponentInterface* componentInterface)
                    {
                        return componentInterface->GetNumContainers() != 0;
                    });

            if (shouldSkip)
            {
                continue;
            }
        }

        UnloadRetiredModule(retiredModule);

        s_retiredModules.EraseAt(index);
    }
}

static void RemoveStaleModuleCopies(const FilePath& copyDirectory, const String& moduleStem)
{
    const std::string prefix = std::string(moduleStem.Data()) + ".";

    std::error_code errorCode;
    Array<std::filesystem::path> stalePaths;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(copyDirectory.Data(), errorCode))
    {
        if (entry.is_regular_file(errorCode) && entry.path().filename().string().rfind(prefix, 0) == 0)
        {
            stalePaths.PushBack(entry.path());
        }
    }

    // copies still loaded (by this editor or another) are locked and stay
    for (const std::filesystem::path& stalePath : stalePaths)
    {
        std::filesystem::remove(stalePath, errorCode);
    }
}

// Load a copy so the project can be rebuilt while the editor has the module loaded
static TResult<FilePath> CopyModule(const FilePath& modulePath, ANSIStringView copyDirectoryName)
{
    const FilePath copyDirectory = EngineGlobals::GetTempDirectory() / String(copyDirectoryName);

    if (!copyDirectory.Exists() && !copyDirectory.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create {} for the game module", copyDirectory);
    }

    const String moduleStem = FilePath(modulePath.Basename()).StripExtension();

    RemoveStaleModuleCopies(copyDirectory, moduleStem);

    const FilePath copyPath = copyDirectory / HYP_FORMAT("{}.{}.{}{}",
        moduleStem,
        uint64(Time::Now()),
        s_moduleCopyCounter++,
        modulePath.GetExtension().Any() ? String(".") + modulePath.GetExtension() : String());

    std::error_code errorCode;
    std::filesystem::copy_file(modulePath.Data(), copyPath.Data(), std::filesystem::copy_options::overwrite_existing, errorCode);

    if (errorCode)
    {
        return HYP_MAKE_ERROR(Error, "Failed to copy game module {} to {}: {}", modulePath, copyPath, errorCode.message().c_str());
    }

    return copyPath;
}

static FilePath GetModuleBindingsPath(const FilePath& modulePath)
{
    String moduleStem = FilePath(modulePath.Basename()).StripExtension();

#ifndef HYP_WINDOWS
    if (moduleStem.StartsWith("lib"))
    {
        moduleStem = String(moduleStem.Substr(3));
    }
#endif

    return modulePath.BasePath() / (moduleStem + ".Bindings.dll");
}

#ifdef HYP_DOTNET

static bool LoadModuleBindings(const FilePath& modulePath)
{
    const FilePath bindingsPath = GetModuleBindingsPath(modulePath);

    if (!bindingsPath.Exists() || !DotNETHost::GetInstance().IsInitialized())
    {
        return false;
    }

    TResult<FilePath> copyResult = CopyModule(bindingsPath, "NativeModuleBindings");

    if (copyResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to copy modules at path {}: {}", modulePath, copyResult.GetError().GetMessage());

        return false;
    }

    if (!DotNETHost::GetInstance().LoadAssembly(copyResult.GetValue().Data()))
    {
        HYP_LOG(Editor, Error, "Failed to load {}. Please rebuild the game", bindingsPath, modulePath);

        return false;
    }

    return true;
}
#endif

static LoadedNativeModule LoadModuleCopy(const FilePath& modulePath, const FilePath& copyPath, Time timestamp)
{
    LoadedNativeModule loadedModule;
    loadedModule.modulePath = modulePath;
    loadedModule.copyPath = copyPath;
    loadedModule.loadedTimestamp = timestamp;

    DynamicLibrary* library = DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(copyPath)));

    if (library == nullptr)
    {
        loadedModule.error = HYP_FORMAT("Failed to load game module {}", modulePath);

        return loadedModule;
    }

    loadedModule.isLoaded = true;

    library->GetImageRange(loadedModule.imageBegin, loadedModule.imageEnd);

    const GetGameClassNameFn getGameClassName = reinterpret_cast<GetGameClassNameFn>(library->GetFunction("HypGameModule_GetGameClassName"));
    const GetEngineVersionFn getEngineVersion = reinterpret_cast<GetEngineVersionFn>(library->GetFunction("HypGameModule_GetEngineVersion"));

    uint32 versionMajor = 0;
    uint32 versionMinor = 0;
    uint32 versionPatch = 0;

    if (getGameClassName == nullptr || getEngineVersion == nullptr)
    {
        loadedModule.error = HYP_FORMAT("Game module {} doesn't export HypGameModule_GetGameClassName / HypGameModule_GetEngineVersion", modulePath);
    }
    else if (getEngineVersion(&versionMajor, &versionMinor, &versionPatch);
        versionMajor != HYP_VERSION_MAJOR || versionMinor != HYP_VERSION_MINOR)
    {
        loadedModule.error = HYP_FORMAT("Game module {} was built for engine {}.{}.{} (this is {}.{}.{}). Rebuild the game.",
            modulePath, versionMajor, versionMinor, versionPatch, HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
    }
    else
    {
        loadedModule.gameClassName = getGameClassName();

        const Class* gameClass = GetClass(StringHash(loadedModule.gameClassName.Data()));

        if (gameClass == nullptr || !gameClass->IsDerivedFrom(Game::StaticClass()))
        {
            loadedModule.error = HYP_FORMAT("Game module {} names Game class '{}', which isn't a registered Game subclass", modulePath, loadedModule.gameClassName);
        }
        else
        {
#ifdef HYP_DOTNET
            loadedModule.hasBindings = LoadModuleBindings(modulePath);

            // The class has no C# counterpart, so the managed editor sees instances of it as plain Game objects
            if (gameClass->GetManagedClass() == nullptr)
            {
                gameClass->SetManagedClass(Game::StaticClass()->GetManagedClass());
            }
#endif
        }
    }

    if (loadedModule.error.Empty())
    {
        ComponentInterfaceRegistry::GetInstance().PublishModuleComponents();
    }

    // nothing was made from it yet, so a failed build can go right away and be retried once rebuilt
    if (loadedModule.error.Any() && loadedModule.CanUnload())
    {
        RetiredNativeModule failedModule = RetireModule(loadedModule);
        UnloadRetiredModule(failedModule);

        loadedModule.isLoaded = false;
    }

    return loadedModule;
}

static TResult<LoadedNativeModule*> LoadLatestModule(const FilePath& modulePath, const FilePath& projectDirectory)
{
    const Time timestamp = modulePath.LastModifiedTimestamp();

    LoadedNativeModule* currentModule = FindLoadedModule(modulePath);

    if (currentModule != nullptr)
    {
        if (timestamp <= currentModule->loadedTimestamp)
        {
            if (currentModule->hasBindings)
            {
                ScriptSystem::SetGameAssembly(projectDirectory, GetModuleBindingsPath(modulePath), /* isNewBuild */ false);
            }

            return currentModule;
        }

        if (currentModule->isLoaded && !currentModule->CanUnload())
        {
            if (!currentModule->warnedRebuilt)
            {
                HYP_LOG(Editor, Warning, "Game module {} was rebuilt since it was loaded, restart the editor to use the new build", modulePath);

                currentModule->warnedRebuilt = true;
            }

            return currentModule;
        }
    }

    TResult<FilePath> copyResult = CopyModule(modulePath, "NativeModules");

    if (copyResult.HasError())
    {
        // most likely still being written by the linker
        if (currentModule != nullptr && currentModule->isLoaded && currentModule->error.Empty())
        {
            HYP_LOG(Editor, Warning, "Using the previous build of game module {}: {}", modulePath, copyResult.GetError().GetMessage());

            return currentModule;
        }

        return copyResult.GetError();
    }

    const bool isReload = currentModule != nullptr && currentModule->isLoaded;

    if (currentModule != nullptr)
    {
        if (currentModule->isLoaded)
        {
            s_retiredModules.PushBack(RetireModule(*currentModule));
        }

        s_loadedModules.EraseAt(size_t(currentModule - s_loadedModules.Data()));
    }

    LoadedNativeModule& loadedModule = s_loadedModules.PushBack(LoadModuleCopy(modulePath, copyResult.GetValue(), timestamp));

    if (loadedModule.error.Any())
    {
        HYP_LOG(Editor, Error, "{}", loadedModule.error);
    }
    else
    {
        HYP_LOG(Editor, Info, "{} game module {} (Game class: {})", isReload ? "Reloaded" : "Loaded", modulePath, loadedModule.gameClassName);
    }

    if (loadedModule.hasBindings)
    {
        ScriptSystem::SetGameAssembly(projectDirectory, GetModuleBindingsPath(modulePath), /* isNewBuild */ true);
    }

    return &loadedModule;
}

static void ReplaceProjectGame(EditorProject* project, Handle<Game> oldGame, Handle<Game> newGame)
{
    newGame->SetIsServerGame(oldGame->IsServerGame());
    newGame->SetStartupWorldName(oldGame->GetStartupWorldName());
    newGame->SetAssetRegistry(oldGame->DetachAssetRegistry());

    if (const Handle<World>& world = project->GetWorld(); world.IsValid() && world->GetGame() == oldGame.Get())
    {
        world->SetGame(newGame.Get());
    }

    project->SetGame(newGame);

    oldGame->ReleaseManagedObject();
    oldGame.Reset();

    CollectManagedGarbage();
}

static void ReplaceRetiredProjectGame(EditorProject* project, const LoadedNativeModule& loadedModule)
{
    Handle<Game> oldGame = project->GetGame();

    if (!oldGame.IsValid() || !IsRetiredModuleClass(oldGame->InstanceClass()) || loadedModule.error.Any())
    {
        return;
    }

    if (oldGame->GetWorld().IsValid())
    {
        return;
    }

    Handle<Game> newGame = Game::CreateGame(StringHash(loadedModule.gameClassName.Data()));

    if (!newGame.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to create an instance of Game class '{}' from {}", loadedModule.gameClassName, loadedModule.modulePath);

        return;
    }

    ReplaceProjectGame(project, std::move(oldGame), std::move(newGame));
}

static Result ReloadNativeGameModule(EditorProject* project)
{
    const FilePath modulePath = GetNativeModulePath(project->GetFilePath());

    if (!modulePath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} isn't built", modulePath);
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    if (const LoadedNativeModule* currentModule = FindLoadedModule(modulePath);
        currentModule != nullptr && currentModule->isLoaded && modulePath.LastModifiedTimestamp() <= currentModule->loadedTimestamp)
    {
        HYP_LOG(Editor, Info, "Game module {} is up to date", modulePath);
    }

    TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, project->GetFilePath().BasePath());

    if (moduleResult.HasError())
    {
        return moduleResult.GetError();
    }

    if (moduleResult.GetValue()->error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", moduleResult.GetValue()->error);
    }

    ReplaceRetiredProjectGame(project, *moduleResult.GetValue());

    return {};
}

#ifdef HYP_DOTNET
static LoadedManagedModule* FindLoadedManagedModule(const FilePath& modulePath)
{
    for (LoadedManagedModule& loadedModule : s_loadedManagedModules)
    {
        if (loadedModule.modulePath == modulePath)
        {
            return &loadedModule;
        }
    }

    return nullptr;
}

static LoadedManagedModule LoadManagedModuleCopy(const FilePath& modulePath, const FilePath& copyPath, const String& gameClassName, Time timestamp)
{
    LoadedManagedModule loadedModule;
    loadedModule.modulePath = modulePath;
    loadedModule.loadedTimestamp = timestamp;
    loadedModule.assembly = DotNETHost::GetInstance().LoadAssembly(copyPath.Data());

    if (!loadedModule.assembly)
    {
        loadedModule.error = HYP_FORMAT("Failed to load game assembly {} (it may have been built for a different engine version). Rebuild the game.", modulePath);

        return loadedModule;
    }

    loadedModule.gameClass = loadedModule.assembly->FindClassByName(gameClassName.Data());

    if (!loadedModule.gameClass || !loadedModule.gameClass->HasParentClass("Game"))
    {
        loadedModule.gameClass.Reset();
        loadedModule.error = HYP_FORMAT("Game assembly {} has no Game subclass named '{}'", modulePath, gameClassName);
    }

    return loadedModule;
}

// builds of an assembly stay loaded until the editor exits, since dynamic classes can't be unregistered
static TResult<LoadedManagedModule*> LoadLatestManagedModule(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetManagedModulePath(projectFilepath);
    const Time timestamp = modulePath.LastModifiedTimestamp();

    LoadedManagedModule* currentModule = FindLoadedManagedModule(modulePath);

    if (currentModule != nullptr && timestamp <= currentModule->loadedTimestamp)
    {
        ScriptSystem::SetGameAssembly(projectFilepath.BasePath(), modulePath, /* isNewBuild */ false);

        return currentModule;
    }

    TResult<FilePath> copyResult = CopyModule(modulePath, "ManagedModules");

    if (copyResult.HasError())
    {
        // most likely still being written by the build
        if (currentModule != nullptr && currentModule->error.Empty())
        {
            HYP_LOG(Editor, Warning, "Using the previous build of game assembly {}: {}", modulePath, copyResult.GetError().GetMessage());

            return currentModule;
        }

        return copyResult.GetError();
    }

    const String gameClassName = GetNativeProjectName(projectFilepath) + "Game";

    LoadedManagedModule loadedModule = LoadManagedModuleCopy(modulePath, copyResult.GetValue(), gameClassName, timestamp);

    if (loadedModule.error.Any())
    {
        HYP_LOG(Editor, Error, "{}", loadedModule.error);
    }
    else
    {
        HYP_LOG(Editor, Info, "{} game assembly {} (Game class: {})", currentModule != nullptr ? "Reloaded" : "Loaded", modulePath, gameClassName);

        ScriptSystem::SetGameAssembly(projectFilepath.BasePath(), modulePath, /* isNewBuild */ true);
    }

    if (currentModule == nullptr)
    {
        return &s_loadedManagedModules.PushBack(std::move(loadedModule));
    }

    loadedModule.needsRetarget = loadedModule.error.Empty();

    *currentModule = std::move(loadedModule);

    return currentModule;
}

static TResult<Handle<Game>> CreateManagedGame(const LoadedManagedModule& loadedModule)
{
    if (loadedModule.error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", loadedModule.error);
    }

    Handle<Game> game;

    {
        GlobalContextScope scope(ObjectInitializerContext { Game::StaticClass(), ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        game = MakeHandle<Game>();
    }

    ScriptObjectResource* scriptObjectResource = ScriptObjectFunctions::CreateScriptObjectResource_DotNet(game.Get(), loadedModule.gameClass);

    if (scriptObjectResource == nullptr || ScriptObjectFunctions::GetManagedObject(scriptObjectResource) == nullptr)
    {
        if (scriptObjectResource != nullptr)
        {
            ScriptObjectFunctions::DestroyScriptObjectResource(scriptObjectResource);
        }

        return HYP_MAKE_ERROR(Error, "Failed to create an instance of the Game class from {}", loadedModule.modulePath);
    }

    game->SetScriptObjectResource(scriptObjectResource);
    scriptObjectResource->AddReader();

    return game;
}

static bool IsCurrentManagedGame(Game* game, const LoadedManagedModule& loadedModule)
{
    ScriptObjectResource* scriptObjectResource = game->GetScriptObjectResource();

    return scriptObjectResource != nullptr && scriptObjectResource->GetManagedClass() == loadedModule.gameClass;
}

// moves the project's objects and Game onto the latest build of the assembly
static Result ApplyManagedModule(EditorProject* project, LoadedManagedModule& loadedModule)
{
    if (loadedModule.error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", loadedModule.error);
    }

    if (loadedModule.needsRetarget)
    {
        if (const Handle<World>& world = project->GetWorld(); world.IsValid())
        {
            ScriptSystem::RetargetReloadedObjects(*world);
        }

        loadedModule.needsRetarget = false;
    }

    Handle<Game> oldGame = project->GetGame();

    if (oldGame.IsValid() && (IsCurrentManagedGame(oldGame.Get(), loadedModule) || oldGame->GetWorld().IsValid()))
    {
        return {};
    }

    TResult<Handle<Game>> gameResult = CreateManagedGame(loadedModule);

    if (gameResult.HasError())
    {
        return gameResult.GetError();
    }

    if (!oldGame.IsValid())
    {
        project->SetGame(gameResult.GetValue());

        return {};
    }

    ReplaceProjectGame(project, std::move(oldGame), gameResult.GetValue());

    return {};
}
#endif // HYP_DOTNET

static Result ReloadManagedGameModule(EditorProject* project)
{
    const FilePath modulePath = GetManagedModulePath(project->GetFilePath());

    if (!modulePath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} isn't built", modulePath);
    }

#ifdef HYP_DOTNET
    if (!DotNETHost::GetInstance().IsInitialized())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} needs the .NET runtime", modulePath);
    }

    if (const Handle<Game>& game = project->GetGame(); game.IsValid() && game->GetWorld().IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module while the game is running");
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(project->GetFilePath());

    if (moduleResult.HasError())
    {
        return moduleResult.GetError();
    }

    LoadedManagedModule& loadedModule = *moduleResult.GetValue();

    if (!loadedModule.needsRetarget && project->GetGame().IsValid() && IsCurrentManagedGame(project->GetGame().Get(), loadedModule))
    {
        HYP_LOG(Editor, Info, "Game assembly {} is up to date", modulePath);

        return {};
    }

    Result result = ApplyManagedModule(project, loadedModule);

    loadedModule.applyFailed = result.HasError();

    return result;
#else
    return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} needs the .NET runtime", modulePath);
#endif
}

} // namespace

EDITOR_API String GetNativeProjectName(const FilePath& projectFilepath)
{
    return ReadProjectModuleName(projectFilepath);
}

EDITOR_API FilePath GetNativeModulePath(const FilePath& projectFilepath)
{
#ifdef HYP_WINDOWS
    const String fileName = GetNativeProjectName(projectFilepath) + "Game.dll";
#elif defined(HYP_MACOS)
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.dylib";
#else
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.so";
#endif

    return projectFilepath.BasePath() / "Binaries" / GetModulePlatformDirectory() / fileName;
}

EDITOR_API FilePath GetNativeBuildScriptPath(const FilePath& projectFilepath)
{
#ifdef HYP_WINDOWS
    static constexpr const char* BuildScriptName = "Build.bat";
#else
    static constexpr const char* BuildScriptName = "Build.sh";
#endif

    return projectFilepath.BasePath() / "Build" / GetModulePlatformDirectory() / BuildScriptName;
}

EDITOR_API TResult<Handle<Game>> CreateGameFromNativeModule(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetNativeModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, projectFilepath.BasePath());

    if (moduleResult.HasError())
    {
        return moduleResult.GetError();
    }

    LoadedNativeModule* loadedModulePtr = moduleResult.GetValue();

    if (loadedModulePtr->error.Any())
    {
        return HYP_MAKE_ERROR(Error, "{}", loadedModulePtr->error);
    }

    Handle<Game> game = Game::CreateGame(StringHash(loadedModulePtr->gameClassName.Data()));

    if (!game.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create an instance of Game class '{}' from {}", loadedModulePtr->gameClassName, modulePath);
    }

    return game;
}

EDITOR_API Result ReloadGameModule(EditorSubsystem* subsystem)
{
    const Handle<EditorProject>& project = subsystem->GetCurrentProject();

    if (!project.IsValid() || !project->IsSaved())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: no saved project is open");
    }

    if (subsystem->IsSimulating())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module while simulating, it's reloaded when play starts");
    }

    return IsManagedProject(project->GetFilePath())
        ? ReloadManagedGameModule(project.Get())
        : ReloadNativeGameModule(project.Get());
}

EDITOR_API void UpdateGameModules(EditorSubsystem* subsystem)
{
    Mutex::Guard guard(s_loadedModulesMutex);

    const Handle<EditorProject>& project = subsystem->GetCurrentProject();

    const bool isSimulating = subsystem->IsSimulating();

    if (isSimulating && project.IsValid() && project->IsSaved())
    {
        if (LoadedNativeModule* playedModule = FindLoadedModule(GetNativeModulePath(project->GetFilePath())))
        {
            playedModule->hasPlayed = true;
        }
    }

    if (s_wasSimulating && !isSimulating)
    {
        for (RetiredNativeModule& retiredModule : s_retiredModules)
        {
            retiredModule.playsStoppedSinceRetired++;
        }
    }

    s_wasSimulating = isSimulating;

    if (project.IsValid() && project->IsSaved() && !isSimulating)
    {
        const FilePath modulePath = GetNativeModulePath(project->GetFilePath());

        LoadedNativeModule* loadedModule = FindLoadedModule(modulePath);

        if (loadedModule != nullptr && !loadedModule->warnedRebuilt && PollModuleRebuilt(modulePath, loadedModule->loadedTimestamp))
        {
            TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, project->GetFilePath().BasePath());

            loadedModule = moduleResult.HasError() ? nullptr : moduleResult.GetValue();
        }

        if (loadedModule != nullptr)
        {
            ReplaceRetiredProjectGame(project.Get(), *loadedModule);
        }

#ifdef HYP_DOTNET
        LoadedManagedModule* managedModule = FindLoadedManagedModule(GetManagedModulePath(project->GetFilePath()));

        if (managedModule != nullptr && PollModuleRebuilt(managedModule->modulePath, managedModule->loadedTimestamp))
        {
            TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(project->GetFilePath());

            managedModule = moduleResult.HasError() ? nullptr : moduleResult.GetValue();
        }

        if (managedModule != nullptr && managedModule->error.Empty() && !managedModule->applyFailed)
        {
            if (Result result = ApplyManagedModule(project.Get(), *managedModule); result.HasError())
            {
                HYP_LOG(Editor, Error, "{}", result.GetError().GetMessage());

                managedModule->applyFailed = true;
            }
        }
#endif
    }

    UpdateRetiredModules();
}

EDITOR_API FilePath GetManagedProjectFilePath(const FilePath& projectFilepath)
{
    return projectFilepath.BasePath() / "Source" / (GetNativeProjectName(projectFilepath) + ".csproj");
}

EDITOR_API bool IsManagedProject(const FilePath& projectFilepath)
{
    return GetManagedProjectFilePath(projectFilepath).Exists();
}

EDITOR_API FilePath GetManagedModulePath(const FilePath& projectFilepath)
{
    return projectFilepath.BasePath() / "Binaries" / GetModulePlatformDirectory() / (GetNativeProjectName(projectFilepath) + ".dll");
}

EDITOR_API Result WriteManagedProjectProps(const FilePath& projectFilepath)
{
    const FilePath propsPath = projectFilepath.BasePath() / "Source" / "Hyperion.Local.props";

    const String binDir = String(CoreApi::GetExecutablePath()).ReplaceAll("\\", "/");
    const String baseDir = String(CoreApi::GetBaseDirectory()).ReplaceAll("\\", "/");

    const String text = String("<Project>\n  <PropertyGroup>\n")
        + "    <HyperionBinDir>" + binDir + "/</HyperionBinDir>\n"
        + "    <HyperionBaseDir>" + baseDir + "/</HyperionBaseDir>\n"
        + "    <HyperionSdkDir>" + baseDir + "/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
        + "  </PropertyGroup>\n</Project>\n";

    FileByteWriter writer { propsPath };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", propsPath);
    }

    writer.Write(text.Data(), text.Size());
    writer.Close();

    return {};
}

EDITOR_API TResult<Handle<Game>> CreateGameFromManagedModule(const FilePath& projectFilepath)
{
#ifdef HYP_DOTNET
    // without a .NET runtime (a commandlet such as the cook), the project keeps the Game its file describes
    if (!IsManagedProject(projectFilepath) || !DotNETHost::GetInstance().IsInitialized())
    {
        return Handle<Game>();
    }

    const FilePath modulePath = GetManagedModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    Mutex::Guard guard(s_loadedModulesMutex);

    TResult<LoadedManagedModule*> moduleResult = LoadLatestManagedModule(projectFilepath);

    if (moduleResult.HasError())
    {
        return moduleResult.GetError();
    }

    return CreateManagedGame(*moduleResult.GetValue());
#else
    return Handle<Game>();
#endif
}

namespace {

struct GameProjectFile
{
    FilePath templatePath;
    FilePath filepath;
};

Array<GameProjectFile> GetGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language, const String& name)
{
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String className = name + "Game";

    Array<GameProjectFile> files;

    if (language == GameProjectLanguage::Managed)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/ManagedGame";

        files.PushBack({ templateDir / "Game.csproj.in", sourceDir / (name + ".csproj") });
        files.PushBack({ templateDir / "Game.cs.in", sourceDir / (className + ".cs") });
        files.PushBack({ templateDir / "Program.cs.in", sourceDir / "Program.cs" });
        files.PushBack({ templateDir / "Game.slnx.in", projectFilepath.BasePath() / (name + ".slnx") });
    }
    else if (language == GameProjectLanguage::Native)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/NativeGame";

        files.PushBack({ templateDir / "CMakeLists.txt.in", sourceDir / "CMakeLists.txt" });
        files.PushBack({ templateDir / "Game" / "Game.hpp.in", sourceDir / "Game" / (className + ".hpp") });
        files.PushBack({ templateDir / "Game" / "Game.cpp.in", sourceDir / "Game" / (className + ".cpp") });
        files.PushBack({ templateDir / "Launcher" / "main.cpp.in", sourceDir / "Launcher" / "main.cpp" });
    }

    return files;
}

bool ReadTextFile(const FilePath& filepath, String& outText)
{
    if (!filepath.Exists())
    {
        return false;
    }

    FileByteReader reader { filepath };

    if (reader.Eof())
    {
        return false;
    }

    const ByteBuffer buffer = reader.Read();
    reader.Close();

    outText = String(buffer.ToByteView());

    return true;
}

bool RenderGameProjectTemplate(const FilePath& templatePath, const FilePath& projectFilepath, const String& name, String& outText)
{
    String templateText;

    if (!ReadTextFile(templatePath, templateText))
    {
        return false;
    }

    String contentDir = "../../";

    if (TResult<String> contentSubfolderResult = ReadProjectContentSubfolder(projectFilepath); contentSubfolderResult.HasValue() && contentSubfolderResult.GetValue().Any())
    {
        contentDir += contentSubfolderResult.GetValue() + "/";
    }

    outText = templateText.ReplaceAll("@NAME@", name).ReplaceAll("@CLASS@", name + "Game").ReplaceAll("@CONTENT_DIR@", contentDir);

    return true;
}

bool IsGameProjectScratchDirectory(const std::filesystem::path& path)
{
    static const char* const s_scratchDirectories[] = { "obj", "bin", ".vs", ".vscode", ".idea" };

    const std::string directoryName = path.filename().string();

    for (const char* scratchDirectory : s_scratchDirectories)
    {
        if (directoryName == scratchDirectory)
        {
            return true;
        }
    }

    return false;
}

} // namespace

EDITOR_API GameProjectLanguage GetGameProjectLanguage(const FilePath& projectFilepath)
{
    if (!(projectFilepath.BasePath() / "Source").Exists())
    {
        return GameProjectLanguage::None;
    }

    return IsManagedProject(projectFilepath) ? GameProjectLanguage::Managed : GameProjectLanguage::Native;
}

EDITOR_API bool IsGameProjectUnmodified(const FilePath& projectFilepath)
{
    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);

    if (language == GameProjectLanguage::None)
    {
        return false;
    }

    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String name = GetNativeProjectName(projectFilepath);

    std::error_code errorCode;
    Array<std::filesystem::path> generatedPaths;

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        String expectedText;
        String actualText;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, name, expectedText)
            || !ReadTextFile(file.filepath, actualText)
            || expectedText != actualText)
        {
            return false;
        }

        generatedPaths.PushBack(std::filesystem::weakly_canonical(file.filepath.Data(), errorCode));
    }

    generatedPaths.PushBack(std::filesystem::weakly_canonical((sourceDir / "Hyperion.Local.props").Data(), errorCode));
    generatedPaths.PushBack(std::filesystem::weakly_canonical((sourceDir / "CMakeUserPresets.json").Data(), errorCode));

    // anything the user added next to the generated files counts as their work too
    for (std::filesystem::recursive_directory_iterator it(sourceDir.Data(), errorCode), end; !errorCode && it != end; it.increment(errorCode))
    {
        if (it->is_directory(errorCode))
        {
            if (IsGameProjectScratchDirectory(it->path()))
            {
                it.disable_recursion_pending();
            }

            continue;
        }

        const std::filesystem::path path = std::filesystem::weakly_canonical(it->path(), errorCode);

        bool isGenerated = false;

        for (const std::filesystem::path& generatedPath : generatedPaths)
        {
            isGenerated |= (path == generatedPath);
        }

        if (!isGenerated)
        {
            return false;
        }
    }

    return !errorCode;
}

EDITOR_API Result GenerateGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language)
{
    if (language == GameProjectLanguage::None)
    {
        return HYP_MAKE_ERROR(Error, "No game project language given");
    }

    // before any file is written, Source/ existing changes what the project's name resolves to
    const String name = GetNativeProjectName(projectFilepath);

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        String text;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, name, text))
        {
            return HYP_MAKE_ERROR(Error, "Template {} is missing", file.templatePath);
        }

        const FilePath directory = file.filepath.BasePath();

        if (!directory.Exists() && !directory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create directory {}", directory);
        }

        FileByteWriter writer { file.filepath };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", file.filepath);
        }

        writer.Write(text.Data(), text.Size());
        writer.Close();
    }

    if (language == GameProjectLanguage::Managed)
    {
        return WriteManagedProjectProps(projectFilepath);
    }

    return {};
}

EDITOR_API Result RemoveGameProjectFiles(const FilePath& projectFilepath)
{
    if (!IsGameProjectUnmodified(projectFilepath))
    {
        return HYP_MAKE_ERROR(Error, "The game project has been modified, not removing it");
    }

    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String name = GetNativeProjectName(projectFilepath);

    // a module built from the removed sources would still be picked up for play
    const FilePath modulePath = language == GameProjectLanguage::Managed
        ? GetManagedModulePath(projectFilepath)
        : GetNativeModulePath(projectFilepath);

    if (!sourceDir.RemoveRecursively())
    {
        return HYP_MAKE_ERROR(Error, "Failed to remove {}", sourceDir);
    }

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        if (file.filepath.Exists())
        {
            file.filepath.Remove();
        }
    }

    if (modulePath.Exists())
    {
        modulePath.Remove();
    }

    return {};
}

EDITOR_API Result RenameGameProjectFiles(const FilePath& projectFilepath, const String& oldName, const String& newName)
{
    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);

    if (language == GameProjectLanguage::None)
    {
        return HYP_MAKE_ERROR(Error, "{} has no game code to rename", projectFilepath);
    }

    const Array<GameProjectFile> oldFiles = GetGameProjectFiles(projectFilepath, language, oldName);
    const Array<GameProjectFile> newFiles = GetGameProjectFiles(projectFilepath, language, newName);

    for (size_t index = 0; index < oldFiles.Size(); index++)
    {
        if (oldFiles[index].filepath != newFiles[index].filepath && oldFiles[index].filepath.Exists() && newFiles[index].filepath.Exists())
        {
            return HYP_MAKE_ERROR(Error, "Can't rename {} to {}, it already exists", oldFiles[index].filepath, newFiles[index].filepath);
        }
    }

    for (size_t index = 0; index < oldFiles.Size(); index++)
    {
        if (oldFiles[index].filepath == newFiles[index].filepath || !oldFiles[index].filepath.Exists())
        {
            continue;
        }

        std::error_code errorCode;
        std::filesystem::rename(oldFiles[index].filepath.Data(), newFiles[index].filepath.Data(), errorCode);

        if (errorCode)
        {
            return HYP_MAKE_ERROR(Error, "Failed to rename {} to {}: {}", oldFiles[index].filepath, newFiles[index].filepath, errorCode.message().c_str());
        }
    }

    // g_cls<Name>Game is the reflection global of the Game class
    const Pair<String, String> classReplacements[] = {
        { String("g_cls") + oldName + "Game", String("g_cls") + newName + "Game" },
        { oldName + "Game", newName + "Game" }
    };

    // the bare name is only the build's: targets, assembly and project file names
    const Pair<String, String> buildReplacements[] = {
        { oldName + "Game", newName + "Game" },
        { oldName, newName }
    };

    static const char* const s_codeExtensions[] = { ".cpp", ".cc", ".c", ".hpp", ".h", ".inl", ".cs" };
    static const char* const s_buildExtensions[] = { ".csproj", ".props", ".targets", ".txt", ".cmake", ".slnx", ".sln" };

    Array<std::filesystem::path> codeFiles;
    std::error_code errorCode;

    const FilePath sourceDir = projectFilepath.BasePath() / "Source";

    for (std::filesystem::recursive_directory_iterator it(sourceDir.Data(), errorCode), end; !errorCode && it != end; it.increment(errorCode))
    {
        if (it->is_directory(errorCode))
        {
            if (IsGameProjectScratchDirectory(it->path()))
            {
                it.disable_recursion_pending();
            }

            continue;
        }

        codeFiles.PushBack(it->path());
    }

    for (const GameProjectFile& file : newFiles)
    {
        if (file.filepath.BasePath() == projectFilepath.BasePath() && file.filepath.Exists())
        {
            codeFiles.PushBack(std::filesystem::path(file.filepath.Data()));
        }
    }

    for (const std::filesystem::path& codeFile : codeFiles)
    {
        const std::string extension = codeFile.extension().string();

        const auto hasExtension = [&extension](Span<const char* const> extensions)
        {
            return std::find_if(extensions.Begin(), extensions.End(), [&extension](const char* candidate)
                       {
                           return extension == candidate;
                       })
                != extensions.End();
        };

        Span<const Pair<String, String>> replacements;

        if (hasExtension(s_codeExtensions))
        {
            replacements = classReplacements;
        }
        else if (hasExtension(s_buildExtensions))
        {
            replacements = buildReplacements;
        }
        else
        {
            continue;
        }

        const FilePath filepath(codeFile.string().c_str());

        String text;

        if (!ReadTextFile(filepath, text))
        {
            continue;
        }

        String renamedText = text;

        for (const Pair<String, String>& replacement : replacements)
        {
            renamedText = StringUtil::ReplaceIdentifier(renamedText, replacement.first, replacement.second);
        }

        if (renamedText == text)
        {
            continue;
        }

        FileByteWriter writer { filepath };

        if (!writer.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", filepath);
        }

        writer.Write(renamedText.Data(), renamedText.Size());
        writer.Close();
    }

    return {};
}

} // namespace Hyperion
