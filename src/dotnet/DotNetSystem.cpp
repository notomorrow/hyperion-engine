/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>

#include <core/io/ByteWriter.hpp>

#include <system/AppContext.hpp>
#include <core/cli/CommandLine.hpp>

#include <core/dll/DynamicLibrary.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/json/JSON.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <dotnet/Class.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

#include <HyperionEngine.hpp>

namespace hyperion {

enum class LoadAssemblyResult : int32
{
    UNKNOWN_ERROR = -100,
    VERSION_MISMATCH = -2,
    NOT_FOUND = -1,
    OK = 0
};

namespace dotnet {
class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual void Initialize(const FilePath& base_path) = 0;
    virtual RC<Assembly> LoadAssembly(const char* path) const = 0;
    virtual bool UnloadAssembly(ManagedGuid guid) const = 0;
    virtual bool IsCoreAssembly(ManagedGuid guid) const = 0;
    virtual bool IsCoreAssembly(const Assembly* assembly) const = 0;

    virtual void* GetDelegate(
        const TChar* assembly_path,
        const TChar* type_name,
        const TChar* method_name,
        const TChar* delegate_type_name) const = 0;
};

using InitializeAssemblyDelegate = int (*)(ManagedGuid*, Assembly*, const char*, int32);
using UnloadAssemblyDelegate = void (*)(ManagedGuid*, int32*);

static Optional<FilePath> FindAssemblyFilePath(const FilePath& base_path, const char* path)
{
    HYP_NAMED_SCOPE("Find .NET Assembly File Path");

    FilePath filepath = FilePath::Current() / path;

    if (!filepath.Exists())
    {
        HYP_LOG(DotNET, Warning, "Failed to load .NET assembly at path: {}. Trying next path...", filepath);

        filepath = base_path / path;
    }

    if (!filepath.Exists())
    {
        HYP_LOG(DotNET, Warning, "Failed to load .NET assembly at path: {}. Trying next path...", filepath);

        filepath = GetResourceDirectory() / "scripts" / "bin" / path;
    }

    if (!filepath.Exists())
    {
        HYP_LOG(DotNET, Error, "Failed to load .NET assembly at path: {}. Could not locate an assembly for {}.", filepath, path);

        return {};
    }

    return filepath;
}

#ifdef HYP_DOTNET
class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl()
        : m_dll(nullptr),
          m_initialize_assembly_fptr(nullptr),
          m_unload_assembly_fptr(nullptr),
          m_cxt(nullptr),
          m_init_fptr(nullptr),
          m_get_delegate_fptr(nullptr),
          m_close_fptr(nullptr)
    {
    }

    virtual ~DotNetImpl() override
    {
        if (!ShutdownDotNetRuntime())
        {
            HYP_LOG(DotNET, Error, "Failed to shutdown .NET runtime");
        }
    }

    FilePath GetDotNetPath() const
    {
        return GetResourceDirectory() / "data/dotnet";
    }

    FilePath GetLibraryPath() const
    {
        return GetDotNetPath() / "lib";
    }

    FilePath GetRuntimeConfigPath() const
    {
        return GetDotNetPath() / "runtimeconfig.json";
    }

    virtual void Initialize(const FilePath& base_path) override
    {
        m_base_path = base_path;

        // ensure the mono directories exists
        FileSystem::MkDir(GetDotNetPath().Data());
        FileSystem::MkDir(GetLibraryPath().Data());

        InitRuntimeConfig();

        // Load the .NET Core runtime
        if (!LoadHostFxr())
        {
            HYP_THROW("Could not initialize .NET runtime: Failed to load hostfxr");
        }

        if (!InitDotNetRuntime())
        {
            HYP_THROW("Could not initialize .NET runtime: Failed to initialize runtime");
        }

        const Optional<FilePath> interop_assembly_path = FindAssemblyFilePath(m_base_path, "HyperionInterop.dll");

        if (!interop_assembly_path.HasValue())
        {
            HYP_THROW("Could not initialize .NET runtime: Could not locate HyperionInterop.dll!");
        }

        PlatformString interop_assembly_path_platform;

#ifdef HYP_WINDOWS
        interop_assembly_path_platform = interop_assembly_path->ToWide();
#else
        interop_assembly_path_platform = *interop_assembly_path;
#endif

        m_initialize_assembly_fptr = (InitializeAssemblyDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("InitializeAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        AssertThrowMsg(
            m_initialize_assembly_fptr != nullptr,
            "InitializeAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled.");

        m_unload_assembly_fptr = (UnloadAssemblyDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("UnloadAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        AssertThrowMsg(
            m_unload_assembly_fptr != nullptr,
            "UnloadAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled.");

        static const FixedArray<Pair<String, FilePath>, 3> core_assemblies = {
            Pair<String, FilePath> { "interop", *interop_assembly_path },
            Pair<String, FilePath> { "core", FindAssemblyFilePath(m_base_path, "HyperionCore.dll").GetOr([]() -> FilePath
                                                 {
                                                     HYP_FAIL("Failed to get HyperionCore.dll");
                                                     return {};
                                                 }) },
            Pair<String, FilePath> { "runtime", FindAssemblyFilePath(m_base_path, "HyperionRuntime.dll").GetOr([]() -> FilePath
                                                    {
                                                        HYP_FAIL("Failed to get HyperionRuntime.dll");
                                                        return {};
                                                    }) }
        };

        for (const Pair<String, FilePath>& entry : core_assemblies)
        {
            RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(AssemblyFlags::CORE_ASSEMBLY);

            auto it = m_core_assemblies.Insert(entry.first, assembly).first;

            // Initialize all core assemblies
            int result = m_initialize_assembly_fptr(
                &assembly->GetGuid(),
                assembly.Get(),
                entry.second.Data(),
                /* is_core_assembly */ 1);

            if (result != int(LoadAssemblyResult::OK))
            {
                HYP_FAIL("Failed to load core assembly %s: Got error code %d", entry.first.Data(), result);
            }
        }
    }

    virtual RC<Assembly> LoadAssembly(const char* path) const override
    {
        RC<Assembly> assembly = MakeRefCountedPtr<Assembly>();

        Optional<FilePath> filepath = FindAssemblyFilePath(m_base_path, path);

        if (!filepath.HasValue())
        {
            return nullptr;
        }

        int result = m_initialize_assembly_fptr(
            &assembly->GetGuid(),
            assembly.Get(),
            filepath->Data(),
            /* is_core_assembly */ 0);

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Got error code {}", path, result);

            return nullptr;
        }

        return assembly;
    }

    virtual bool UnloadAssembly(ManagedGuid assembly_guid) const override
    {
        if (IsCoreAssembly(assembly_guid))
        {
            return false;
        }

        HYP_LOG(DotNET, Info, "Unloading assembly...");

        int32 result;
        m_unload_assembly_fptr(&assembly_guid, &result);

        return bool(result);
    }

    virtual bool IsCoreAssembly(ManagedGuid assembly_guid) const
    {
        if (!assembly_guid.IsValid())
        {
            return false;
        }

        auto it = m_core_assemblies.FindIf([assembly_guid](const auto& it)
            {
                return Memory::MemCmp(&it.second->GetGuid(), &assembly_guid, sizeof(ManagedGuid)) == 0;
            });

        return it != m_core_assemblies.End();
    }

    virtual bool IsCoreAssembly(const Assembly* assembly) const
    {
        if (!assembly)
        {
            return false;
        }

        return IsCoreAssembly(assembly->GetGuid());
    }

    virtual void* GetDelegate(
        const TChar* assembly_path,
        const TChar* type_name,
        const TChar* method_name,
        const TChar* delegate_type_name) const override
    {
        if (!m_cxt)
        {
            HYP_THROW("Failed to get delegate: .NET runtime not initialized");
        }

        // Get the delegate for the managed function
        void* load_assembly_and_get_function_pointer_fptr = nullptr;

        if (m_get_delegate_fptr(m_cxt, hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer_fptr) != 0)
        {
            HYP_LOG(DotNET, Error, "Failed to get delegate: Failed to get function pointer");

            return nullptr;
        }

        HYP_LOG(DotNET, Info, "Loading .NET assembly: {}\tType Name: {}\tMethod Name: {}", assembly_path, type_name, method_name);

        void* delegate_ptr = nullptr;

        auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fptr;
        bool result = load_assembly_and_get_function_pointer(assembly_path, type_name, method_name, delegate_type_name, nullptr, &delegate_ptr) == 0;

        if (!result)
        {
            HYP_LOG(DotNET, Error, "Failed to get delegate: Failed to load assembly and get function pointer");

            return nullptr;
        }

        return delegate_ptr;
    }

private:
    void InitRuntimeConfig()
    {
        const FilePath filepath = GetRuntimeConfigPath();

        const FilePath current_path = FilePath::Current();

        Array<json::JSONValue> probing_paths;

        probing_paths.PushBack(FilePath::Relative(GetLibraryPath(), current_path));
        probing_paths.PushBack(FilePath::Relative(m_base_path, current_path));

        const json::JSONValue runtime_config_json(json::JSONObject {
            { "runtimeOptions", json::JSONObject({ { "tfm", "net8.0" }, { "framework", json::JSONObject({ { "name", "Microsoft.NETCore.App" }, { "version", "8.0.1" } }) }, { "additionalProbingPaths", json::JSONArray(probing_paths) } }) } });

        String str = runtime_config_json.ToString(true);

        FileByteWriter writer(filepath.Data());
        writer.WriteString(str);
        writer.Close();
    }

    bool LoadHostFxr()
    {
        // Pre-allocate a large buffer for the path to hostfxr
        char_t buffer[2048];
        size_t buffer_size = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
        if (rc != 0)
        {
            return false;
        }

        HYP_LOG(DotNET, Debug, "Loading hostfxr from: {}", &buffer[0]);

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary::Load(buffer);

        if (!m_dll)
        {
            return false;
        }

        m_init_fptr = (hostfxr_initialize_for_runtime_config_fn)m_dll->GetFunction("hostfxr_initialize_for_runtime_config");
        m_get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)m_dll->GetFunction("hostfxr_get_runtime_delegate");
        m_close_fptr = (hostfxr_close_fn)m_dll->GetFunction("hostfxr_close");

        HYP_LOG(DotNET, Debug, "Loaded hostfxr functions");

        return m_init_fptr && m_get_delegate_fptr && m_close_fptr;
    }

    bool InitDotNetRuntime()
    {
        AssertThrow(m_cxt == nullptr);

        HYP_LOG(DotNET, Debug, "Initializing .NET runtime");

        PlatformString runtime_config_path;

#ifdef HYP_WINDOWS
        runtime_config_path = GetRuntimeConfigPath().ToWide();

        std::wcout << L".NET Runtime path = " << runtime_config_path.Data() << L"\n";
#else
        runtime_config_path = GetRuntimeConfigPath();
#endif

        if (m_init_fptr(runtime_config_path.Data(), nullptr, &m_cxt) != 0)
        {
            HYP_LOG(DotNET, Error, "Failed to initialize .NET runtime");

            return false;
        }

        HYP_LOG(DotNET, Debug, "Initialized .NET runtime");

        return true;
    }

    bool ShutdownDotNetRuntime()
    {
        AssertThrow(m_cxt != nullptr);

        HYP_LOG(DotNET, Debug, "Shutting down .NET runtime");

        m_close_fptr(m_cxt);
        m_cxt = nullptr;

        HYP_LOG(DotNET, Debug, "Shut down .NET runtime");

        return true;
    }

    FilePath m_base_path;

    UniquePtr<DynamicLibrary> m_dll;

    HashMap<String, RC<Assembly>> m_core_assemblies;

    InitializeAssemblyDelegate m_initialize_assembly_fptr;
    UnloadAssemblyDelegate m_unload_assembly_fptr;

    hostfxr_handle m_cxt;
    hostfxr_initialize_for_runtime_config_fn m_init_fptr;
    hostfxr_get_runtime_delegate_fn m_get_delegate_fptr;
    hostfxr_close_fn m_close_fptr;
};

#else

class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl() = default;
    virtual ~DotNetImpl() override = default;

    virtual void Initialize(const FilePath& base_path) override
    {
    }

    virtual RC<Assembly> LoadAssembly(const char* path) const override
    {
        return nullptr;
    }

    virtual bool UnloadAssembly(ManagedGuid guid) const override
    {
        return false;
    }

    virtual bool IsCoreAssembly(ManagedGuid guid) const override
    {
        return false;
    }

    virtual bool IsCoreAssembly(const Assembly* assembly) const override
    {
        return false;
    }

    virtual void* GetDelegate(
        const TChar* assembly_path,
        const TChar* type_name,
        const TChar* method_name,
        const TChar* delegate_type_name) const override
    {
        return nullptr;
    }
};

#endif

DotNetSystem& DotNetSystem::GetInstance()
{
    static DotNetSystem instance;

    return instance;
}

DotNetSystem::DotNetSystem()
    : m_is_initialized(false)
{
}

DotNetSystem::~DotNetSystem() = default;

bool DotNetSystem::EnsureInitialized() const
{
    if (!IsEnabled())
    {
        HYP_LOG(DotNET, Error, "DotNetSystem not enabled, cannot load/unload assemblies");

        return false;
    }

    if (!IsInitialized())
    {
        HYP_LOG(DotNET, Error, "DotNetSystem not initialized, call Initialize() before attempting to load/unload assemblies");

        return false;
    }

    AssertThrow(m_impl != nullptr);

    return true;
}

RC<Assembly> DotNetSystem::LoadAssembly(const char* path) const
{
    if (!EnsureInitialized())
    {
        return nullptr;
    }

    HYP_NAMED_SCOPE("Load .NET Assembly");

    return m_impl->LoadAssembly(path);
}

bool DotNetSystem::UnloadAssembly(ManagedGuid guid) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Unload .NET Assembly");

    return m_impl->UnloadAssembly(guid);
}

bool DotNetSystem::IsCoreAssembly(const Assembly* assembly) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Check if .NET Assembly is Core Assembly");

    return m_impl->IsCoreAssembly(assembly);
}

bool DotNetSystem::IsEnabled() const
{
#ifndef HYP_DOTNET
    return false;
#else
    return true;
#endif
}

bool DotNetSystem::IsInitialized() const
{
    return m_is_initialized;
}

void DotNetSystem::Initialize(const FilePath& base_path)
{
    if (!IsEnabled())
    {
        return;
    }

    if (IsInitialized())
    {
        return;
    }

    HYP_NAMED_SCOPE("Initialize .NET System");

    AssertThrow(m_impl == nullptr);

    m_impl = MakeRefCountedPtr<DotNetImpl>();
    m_impl->Initialize(base_path);

    m_is_initialized = true;
}

void DotNetSystem::Shutdown()
{
    if (!IsEnabled())
    {
        return;
    }

    if (!IsInitialized())
    {
        return;
    }

    HYP_NAMED_SCOPE("Shutdown .NET System");

    m_impl.Reset();

    m_is_initialized = false;
}

} // namespace dotnet
} // namespace hyperion
