/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Utilities/Process.hpp>

#ifdef HYP_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace Hyperion {

CORE_API uint32 GetCurrentProcessIdentifier()
{
#ifdef HYP_WINDOWS
    return uint32(::GetCurrentProcessId());
#else
    return uint32(::getpid());
#endif
}

CORE_API bool IsProcessRunning(uint32 processIdentifier)
{
#ifdef HYP_WINDOWS
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(processIdentifier));

    if (process == nullptr)
    {
        return ::GetLastError() == ERROR_ACCESS_DENIED;
    }

    DWORD exitCode = 0;
    const bool isRunning = !::GetExitCodeProcess(process, &exitCode) || exitCode == STILL_ACTIVE;

    ::CloseHandle(process);

    return isRunning;
#else
    return ::kill(pid_t(processIdentifier), 0) == 0 || errno == EPERM;
#endif
}

} // namespace Hyperion
