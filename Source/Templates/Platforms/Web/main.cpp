#include <Core/Logging/Logger.hpp>
#include <Core/Utilities/Format.hpp>

#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/Game.hpp>
#include <Framework/Threads/SimThread.hpp>

#include <HyperionEngine.hpp>

#include <emscripten.h>
#include <emscripten/wasmfs.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace Hyperion;

#ifdef HYP_WEB_GAME_MODULE
extern "C" const char* HypGameModule_GetGameClassName();
#endif

#ifdef HYP_WEB_MANAGED
extern "C"
{
void HypManaged_Initialize();
Hyperion::Game* HypManaged_CreateGame();

int32_t SystemJS_RandomBytes(uint8_t* buffer, int32_t length)
{
    for (int32_t offset = 0; offset < length; offset += 256)
    {
        const int32_t chunk = length - offset < 256 ? length - offset : 256;

        if (getentropy(buffer + offset, size_t(chunk)) != 0)
        {
            return -1;
        }
    }

    return 0;
}

}
#endif

#ifndef HYP_WEB_NODE_PACKAGE_DIR

EM_JS_DEPS(HypWeb_PackageUrl, "$stringToNewUTF8");

EM_JS(char*, HypWeb_GetPackageUrl, (), {
    return stringToNewUTF8(new URL('Cache', self.location.href).href);
});

EM_JS_DEPS(HypWeb_CacheFiles, "$UTF8ToString");

static bool CopyPageCacheFile(const char* name, const char* path)
{
    const double size = MAIN_THREAD_EM_ASM_DOUBLE({
        const files = Module['hypCacheFiles'];
        const data = files ? files[UTF8ToString($0)] : undefined;

        return data ? data.byteLength : -1;
    }, name);

    if (size < 0.0)
    {
        return false;
    }

    const int file = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);

    if (file < 0)
    {
        return false;
    }

    static constexpr size_t chunkSize = 4u * 1024u * 1024u;

    const size_t totalSize = size_t(size);
    void* buffer = std::malloc(chunkSize);
    bool isCopied = buffer != nullptr;

    for (size_t offset = 0; isCopied && offset < totalSize; offset += chunkSize)
    {
        const size_t count = totalSize - offset < chunkSize ? totalSize - offset : chunkSize;

        MAIN_THREAD_EM_ASM({
            const data = Module['hypCacheFiles'][UTF8ToString($0)];

            HEAPU8.set(data.subarray($2, $2 + $3), $1);
        }, name, buffer, offset, count);

        isCopied = write(file, buffer, count) == ssize_t(count);
    }

    std::free(buffer);
    close(file);

    MAIN_THREAD_EM_ASM({
        delete Module['hypCacheFiles'][UTF8ToString($0)];
    }, name);

    if (!isCopied)
    {
        unlink(path);
    }

    return isCopied;
}

#endif

static void MountPackage()
{
#ifdef HYP_WEB_NODE_PACKAGE_DIR
    backend_t backend = wasmfs_create_node_backend(HYP_WEB_NODE_PACKAGE_DIR);
    wasmfs_create_directory("/hyperion", 0777, backend);
#else

    // Config and Content are preloaded
    chmod("/hyperion", 0777); // preloaded directories come out read-only
    mkdir("/hyperion/Cache", 0777);

    char* packageUrl = HypWeb_GetPackageUrl();
    backend_t backend = wasmfs_create_fetch_backend(packageUrl, 1u << 30);
    free(packageUrl);
    wasmfs_create_directory("/package", 0777, backend);

    close(wasmfs_create_file("/package/index.txt", 0444, backend));

    FILE* indexFile = fopen("/package/index.txt", "r");

    if (indexFile == nullptr)
    {
        fprintf(stderr, "No Cache/index.txt, the cooked package is missing\n");

        return;
    }

    char name[256];

    while (fgets(name, sizeof(name), indexFile) != nullptr)
    {
        name[strcspn(name, "\r\n")] = '\0';

        if (name[0] == '\0')
        {
            continue;
        }

        char sourcePath[512];
        char linkPath[512];
        snprintf(sourcePath, sizeof(sourcePath), "/package/%s", name);
        snprintf(linkPath, sizeof(linkPath), "/hyperion/Cache/%s", name);

        if (CopyPageCacheFile(name, linkPath))
        {
            continue;
        }

        close(wasmfs_create_file(sourcePath, 0444, backend));
        symlink(sourcePath, linkPath);
    }

    fclose(indexFile);
#endif
}

#ifndef HYP_WEB_NODE_PACKAGE_DIR

static void PrefetchCacheFiles()
{
    std::thread([]()
        {
            static constexpr size_t readSize = 4u * 1024u * 1024u;

            void* buffer = std::malloc(readSize);

            for (const char* path : { "/hyperion/Cache/Meshes.bin", "/hyperion/Cache/Textures.bin" })
            {
                const int file = open(path, O_RDONLY);

                if (file < 0)
                {
                    continue;
                }

                while (read(file, buffer, readSize) > 0)
                {
                }

                close(file);
            }

            std::free(buffer);
        })
        .detach();
}
#endif

extern "C" int Hyp_ExecuteConsoleCommand(int argc, const char** argv);
extern "C" void Hyp_GetAllCVarNames(void* callback, void* userData);
extern "C" void Hyp_GetAllCommandletNames(void* callback, void* userData);

extern "C" {

// The page's console calls this, on the page's own thread, with each line typed into it.
EMSCRIPTEN_KEEPALIVE void HypWeb_ExecuteConsoleLine(const char* line)
{
    if (g_simThreadInstance == nullptr)
    {
        return;
    }

    g_simThreadInstance->GetScheduler().Enqueue(
        [text = String(line).Trimmed()]()
        {
            Array<String> args = text.Split(' ');

            if (args.Empty())
            {
                return;
            }

            Array<const char*> argsCharV = MapToArray(args, [](const String& str)
                {
                    return str.Data();
                });

            if (Hyp_ExecuteConsoleCommand(int(args.Size()), argsCharV.Data()) != 0)
            {
                fprintf(stderr, "`%s` is not a console variable or command, or its value is not valid\n", text.Data());
            }
            else
            {
                printf("ok\n");
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

EMSCRIPTEN_KEEPALIVE const char* HypWeb_GetConsoleNames()
{
    static String s_names;

    if (s_names.Empty())
    {
        void (*appendName)(const char*, void*) = [](const char* name, void* userData)
        {
            String& names = *static_cast<String*>(userData);
            names.Append(name);
            names.Append("\n");
        };

        Hyp_GetAllCVarNames(reinterpret_cast<void*>(appendName), &s_names);
        Hyp_GetAllCommandletNames(reinterpret_cast<void*>(appendName), &s_names);
    }

    return s_names.Data();
}

EMSCRIPTEN_KEEPALIVE const char* HypWeb_GetFrameStats()
{
    static String s_stats;

    const Handle<EngineStats>& engineStats = EngineStats::GetInstance();

    if (!engineStats.IsValid())
    {
        return "";
    }

    const auto average = [&engineStats](ANSIStringView path) -> float
    {
        EngineStatBase* stat = engineStats->GetStat(path);

        return stat != nullptr ? engineStats->GetCurrentSnapshot()[*stat].avg : -1.0f;
    };

    s_stats = HYP_FORMAT("{} {} {} {} {} {}",
        engineStats->GetFps(),
        engineStats->GetMsPerFrame(),
        average("Rendering/CPU/RenderThread"),
        average("Sim/CPU/SimThread"),
        average("Rendering/GPU/FrameTime"),
        average("Rendering/GPU/ShadowMaps"));

    return s_stats.Data();
}

} // extern "C"

int main(int argc, char** argv)
{
    MountPackage();

#ifndef HYP_WEB_NODE_PACKAGE_DIR
    PrefetchCacheFiles();
#endif

    const char* defaultArguments[] = {
        "hyperion-sample",
        "--basedir=/hyperion",
        "--cachedir=/hyperion/Cache",
        "--contentdir=/hyperion/Content",
        "--singleplayer=true",
#ifndef HYP_WEB_NODE_PACKAGE_DIR
        // the thread running main() owns the canvas
        "--RenderOnMainThread=true"
#endif
    };

    // The page passes each ?set=Name=Value as console variables to set before the game starts.
    Array<String> startupVariables;

    for (int index = 1; index < argc; index++)
    {
        startupVariables.PushBack(argv[index]);
    }

    argc = int(sizeof(defaultArguments) / sizeof(defaultArguments[0]));
    argv = const_cast<char**>(defaultArguments);

#ifdef HYP_WEB_MANAGED
    HypManaged_Initialize();
#endif

    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    for (const String& startupVariable : startupVariables)
    {
        Array<String> parts = startupVariable.Split('=');

        if (parts.Size() != 2)
        {
            fprintf(stderr, "`%s` is not Name=Value\n", startupVariable.Data());

            continue;
        }

        const char* arguments[] = { parts[0].Data(), parts[1].Data() };

        if (Hyp_ExecuteConsoleCommand(2, arguments) != 0)
        {
            fprintf(stderr, "`%s` is not a console variable, or its value is not valid\n", startupVariable.Data());
        }
    }

    EngineGlobals::GetBlobStorage()->Lock(EngineGlobals::GetCacheDirectory(), /* readOnly */ true);

#if defined(HYP_WEB_MANAGED) && !defined(HYP_WEB_GAME_MODULE)
    Game* managedGame = HypManaged_CreateGame();

    if (!managedGame)
    {
        HYP_FAIL("Failed to create the C# game {}", HYP_WEB_GAME_CLASS);
    }

    Hyp_SetGame(managedGame);
#else
#ifdef HYP_WEB_GAME_MODULE
    // the project's game code names its game class
    const char* gameClassName = HypGameModule_GetGameClassName();
#else
    const char* gameClassName = HYP_WEB_GAME_CLASS;
#endif

    Handle<Game> game = Game::CreateGame(StringHash(gameClassName));

    if (!game.IsValid())
    {
        HYP_FAIL("Failed to create the game {}", gameClassName);
    }

    Hyp_SetGame(game.Get());
#endif

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}
