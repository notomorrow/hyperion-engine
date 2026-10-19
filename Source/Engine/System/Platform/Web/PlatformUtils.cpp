/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <Core/Containers/String.hpp>

#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#include <emscripten/heap.h>

namespace Hyperion {
namespace PlatformUtils {

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    return PlatformString("/hyperion/hyperion-sample");
}

ENGINE_API bool IsInsideAppBundle()
{
    return false;
}

ENGINE_API PlatformString GetUserApplicationDataDirectory()
{
    return PlatformString();
}

ENGINE_API PlatformString GetUserDocumentsDirectory()
{
    return PlatformString();
}

ENGINE_API bool IsOnBatteryPower()
{
    return false;
}

ENGINE_API bool HasBattery()
{
    return false;
}

ENGINE_API bool IsMobileBrowser()
{
    return MAIN_THREAD_EM_ASM_INT({
        if (typeof navigator === 'undefined')
        {
            return 0;
        }

        const isMobileAgent = !!(navigator.userAgentData && navigator.userAgentData.mobile);
        const isTouchOnly = typeof matchMedia === 'function' && matchMedia('(hover: none) and (pointer: coarse)').matches;

        return isMobileAgent || isTouchOnly ? 1 : 0;
    }) != 0;
}

ENGINE_API uint64 GetSystemMemoryBytes()
{
    return uint64(emscripten_get_heap_max());
}

ENGINE_API uint32 GetLogicalCoreCount()
{
    const int numCores = emscripten_num_logical_cores();

    return numCores > 0 ? uint32(numCores) : 1;
}

ENGINE_API void InitializeNetwork()
{
}

} // namespace PlatformUtils
} // namespace Hyperion
