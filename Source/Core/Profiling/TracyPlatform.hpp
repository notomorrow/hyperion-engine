/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

// Passed to Tracy as TRACY_PLATFORM_HEADER. Tracy's bundled rpmalloc uses initial-exec TLS, which marks hyperion-core
// STATIC_TLS and stops it from being dlopen'd (e.g. by the C# editor), so route Tracy's allocations to malloc instead.

#include <stdlib.h>

#define TRACY_HAS_CUSTOM_ALLOCATOR

namespace tracy {

inline void PlatformAllocatorInit()
{
}

inline void PlatformAllocatorThreadInit()
{
}

inline void PlatformAllocatorThreadFinalize()
{
}

inline void PlatformAllocatorFinalize()
{
}

inline void* PlatformMalloc(size_t size)
{
    return malloc(size);
}

inline void* PlatformRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

inline void PlatformFree(void* ptr)
{
    free(ptr);
}

} // namespace tracy
