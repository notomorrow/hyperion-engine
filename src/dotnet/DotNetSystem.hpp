/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_DOT_NET_SYSTEM_HPP
#define HYPERION_DOTNET_DOT_NET_SYSTEM_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <core/filesystem/FilePath.hpp>

#include <Types.hpp>

#include <dotnet/Assembly.hpp>

namespace hyperion {

namespace dotnet {

struct ObjectReference;
class Class;

class DotNetImplBase;
class DotNetImpl;

using AddObjectToCacheFunction = void (*)(void* ptr, Class** out_class_object_ptr, ObjectReference* out_object_reference, int8 is_weak);
using SetKeepAliveFunction = void (*)(ObjectReference* object_reference, int32* keep_alive);
using TriggerGCFunction = void (*)();
using GetAssemblyPointerFunction = void (*)(ObjectReference* assembly_object_reference, Assembly** out_assembly_ptr);

class DotNetSystem
{
public:
    struct GlobalFunctions
    {
        AddObjectToCacheFunction add_object_to_cache_function = nullptr;
        SetKeepAliveFunction set_keep_alive_function = nullptr;
        TriggerGCFunction trigger_gc_function = nullptr;
        GetAssemblyPointerFunction get_assembly_pointer_function = nullptr;
    };

    static DotNetSystem& GetInstance();

    DotNetSystem();

    DotNetSystem(const DotNetSystem&) = delete;
    DotNetSystem& operator=(const DotNetSystem&) = delete;
    DotNetSystem(DotNetSystem&&) noexcept = delete;
    DotNetSystem& operator=(DotNetSystem&&) noexcept = delete;
    ~DotNetSystem();

    HYP_FORCE_INLINE GlobalFunctions& GetGlobalFunctions()
    {
        return m_global_functions;
    }

    HYP_FORCE_INLINE const GlobalFunctions& GetGlobalFunctions() const
    {
        return m_global_functions;
    }

    RC<Assembly> LoadAssembly(const char* path) const;
    bool UnloadAssembly(ManagedGuid guid) const;
    bool IsCoreAssembly(const Assembly* assembly) const;

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize(const FilePath& base_path);
    void Shutdown();

private:
    bool EnsureInitialized() const;

    bool m_is_initialized;
    RC<DotNetImplBase> m_impl;

    GlobalFunctions m_global_functions;
};

} // namespace dotnet
} // namespace hyperion

#endif