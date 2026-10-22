/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <limits.h>
#include <unistd.h>

namespace Hyperion {
namespace PlatformUtils {

namespace {

// cached battery state.
struct BatteryState
{
    bool hasBattery;
    
    bool isDischarging;
    bool hasExternalPower;
};

BatteryState g_batteryState;

// Reads the first line of a small sysfs attribute file, without the trailing newline
bool ReadSysfsValue(const char* directory, const char* attribute, char* outValue, size_t outValueSize)
{
    char path[PATH_MAX];
    std::snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s", directory, attribute);

    FILE* file = std::fopen(path, "r");

    if (!file)
    {
        return false;
    }

    const bool success = std::fgets(outValue, int(outValueSize), file) != nullptr;
    std::fclose(file);

    if (success)
    {
        outValue[std::strcspn(outValue, "\r\n")] = '\0';
    }

    return success;
}

void QueryBattery(BatteryState& outBatteryState)
{
    outBatteryState = {};

    DIR* powerSupplies = opendir("/sys/class/power_supply");

    if (!powerSupplies)
    {
        return;
    }

    while (dirent* entry = readdir(powerSupplies))
    {
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        char type[64];

        if (!ReadSysfsValue(entry->d_name, "type", type, sizeof(type)))
        {
            continue;
        }

        if (std::strcmp(type, "Battery") == 0)
        {
            // peripherals (mice, controllers) also report batteries, only count ones powering the system
            char scope[64];

            if (ReadSysfsValue(entry->d_name, "scope", scope, sizeof(scope)) && std::strcmp(scope, "Device") == 0)
            {
                continue;
            }

            outBatteryState.hasBattery = true;

            char status[64];

            if (ReadSysfsValue(entry->d_name, "status", status, sizeof(status)) && std::strcmp(status, "Discharging") == 0)
            {
                outBatteryState.isDischarging = true;
            }
        }
        else
        {
            // Mains, USB, USB_PD etc.
            char online[16];

            if (ReadSysfsValue(entry->d_name, "online", online, sizeof(online)) && std::strcmp(online, "1") == 0)
            {
                outBatteryState.hasExternalPower = true;
            }
        }
    }

    closedir(powerSupplies);
}

void InitBatteryState()
{
    static std::once_flag s_onceFlag;

    std::call_once(s_onceFlag, []()
    {
        QueryBattery(g_batteryState);
    });
}

} // namespace

ENGINE_API PlatformString GetExecutableAbsolutePath()
{
    char buffer[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));

    // readlink doesn't null terminate, and a full buffer means the path may have been truncated
    if (length <= 0 || length >= ssize_t(sizeof(buffer)))
    {
        return PlatformString();
    }

    return PlatformString(buffer, buffer + length);
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
    // Called every frame by the render thread, so only re-read sysfs every few seconds
    static constexpr int64 RefreshIntervalMs = 5000;

    static AtomicVar<int64> s_nextRefreshTimeMs { 0 };
    static AtomicVar<bool> s_isOnBatteryPower { false };

    InitBatteryState();

    if (!g_batteryState.hasBattery)
    {
        return false;
    }

    const int64 nowMs = int64(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
            .count());

    if (nowMs >= s_nextRefreshTimeMs.Get(MemoryOrder::ACQUIRE))
    {
        s_nextRefreshTimeMs.Set(nowMs + RefreshIntervalMs, MemoryOrder::RELEASE);

        QueryBattery(g_batteryState);
        
        s_isOnBatteryPower.Set(g_batteryState.isDischarging && !g_batteryState.hasExternalPower, MemoryOrder::RELAXED);
    }

    return s_isOnBatteryPower.Get(MemoryOrder::RELAXED);
}

ENGINE_API bool HasBattery()
{
    InitBatteryState();

    return g_batteryState.hasBattery;
}

ENGINE_API bool OpenInFileBrowser(const FilePath& path)
{
    // @TODO
    return false;
}

ENGINE_API uint64 GetSystemMemoryBytes()
{
    return uint64(sysconf(_SC_PHYS_PAGES)) * uint64(sysconf(_SC_PAGE_SIZE));
}

ENGINE_API uint32 GetLogicalCoreCount()
{
    const long numCores = sysconf(_SC_NPROCESSORS_ONLN);

    return numCores > 0 ? uint32(numCores) : 1;
}

ENGINE_API void InitializeNetwork()
{
}

} // namespace PlatformUtils
} // namespace Hyperion
