/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Pool/Pool.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>

namespace Hyperion {

static constexpr size_t GlimmerPoolBlockSize = 32 * 1024 * 1024; // 32 MiB -- BLAS / TLAS build data waiting on upload
static constexpr size_t GlimmerArenaSize = 1 * 1024 * 1024;      // 1 MiB

ENGINE_API extern Pool* g_glimmerPool;   // thread safe: the sim thread and the BLAS / TLAS build threads use it too
ENGINE_API extern Arena* g_glimmerArena; // render thread only, reset at the end of every frame

using GlimmerAllocator = AllocatorInstance<Pool, &g_glimmerPool>;
using GlimmerTempAllocator = AllocatorInstance<Arena, &g_glimmerArena>;

void InitGlimmerMemory();
void ShutdownGlimmerMemory();

} // namespace Hyperion
