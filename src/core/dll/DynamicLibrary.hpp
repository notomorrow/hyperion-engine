/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>
#include <core/containers/String.hpp>

namespace hyperion {

class DynamicLibrary
{
public:
    static UniquePtr<DynamicLibrary> Load(const PlatformString& path);

    DynamicLibrary(const PlatformString& path, void* handle);
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&&) noexcept = delete;
    DynamicLibrary& operator=(DynamicLibrary&&) noexcept = delete;
    ~DynamicLibrary();

    const PlatformString& GetPath() const
    {
        return m_path;
    }

    void* GetFunction(const char* name) const;

private:
    PlatformString m_path;
    void* m_handle;
};

} // namespace hyperion
