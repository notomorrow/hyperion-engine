/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerMemory.hpp>

namespace Hyperion {

ENGINE_API Pool* g_glimmerPool;
ENGINE_API Arena* g_glimmerArena;

void InitGlimmerMemory()
{
    Assert(g_glimmerPool == nullptr);

    // a big mesh's build data doesn't fit a block
    g_glimmerPool = new Pool(GlimmerPoolBlockSize, PF_THREAD_SAFE | PF_FALLBACK);
    g_glimmerArena = new Arena(GlimmerArenaSize);
}

void ShutdownGlimmerMemory()
{
    delete g_glimmerArena;
    g_glimmerArena = nullptr;

    delete g_glimmerPool;
    g_glimmerPool = nullptr;
}

} // namespace Hyperion
