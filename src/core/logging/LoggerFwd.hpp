/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_LOGGER_FWD_HPP
#define HYPERION_UTIL_LOGGER_FWD_HPP

#include <core/Defines.hpp>
#include <core/debug/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace logging {

class Logger;
class LogChannel;

enum LogLevel : uint32
{
    DEBUG,
    INFO,
    WARNING,
    ERR,
    FATAL
};

struct LogCategory
{
    enum LogCategoryFlags : uint8
    {
        LCF_NONE = 0x0,
        LCF_ENABLED = 0x1,
        LCF_FATAL = 0x2,

        LCF_DEFAULT = LCF_ENABLED
    };

    constexpr LogCategory(LogLevel level, uint16 priority, uint8 flags = LCF_DEFAULT)
        : value(uint32(flags) | (uint32(priority) << 8) | (uint32(level) << 24))
    {
    }

    constexpr LogCategory(const LogCategory& other)
        : value(other.value)
    {
    }

    LogCategory& operator=(const LogCategory& other)
    {
        value = other.value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const LogCategory& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const LogCategory& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const LogCategory& other) const
    {
        return GetPriority() < other.GetPriority();
    }

    HYP_FORCE_INLINE constexpr uint8 GetFlags() const
    {
        return value & 0xFF;
    }

    HYP_FORCE_INLINE constexpr uint16 GetPriority() const
    {
        return uint16((value >> 8) & 0xFFFF);
    }

    HYP_FORCE_INLINE constexpr LogLevel GetLevel() const
    {
        return LogLevel((value >> 24) & 0xFF);
    }

    HYP_FORCE_INLINE constexpr bool IsEnabled() const
    {
        return (GetFlags() & LCF_ENABLED) != 0;
    }

    uint32 value;
};

#ifdef HYP_DEBUG_MODE
constexpr LogCategory Debug()
{
    return LogCategory(LogLevel::DEBUG, 10000, LogCategory::LCF_ENABLED);
}
#else
constexpr LogCategory Debug()
{
    return LogCategory(LogLevel::DEBUG, 10000, LogCategory::LCF_NONE);
}
#endif

constexpr LogCategory Warning()
{
    return LogCategory(LogLevel::WARNING, 1000);
}

constexpr LogCategory Info()
{
    return LogCategory(LogLevel::INFO, 100);
}

constexpr LogCategory Error()
{
    return LogCategory(LogLevel::ERR, 10);
}

constexpr LogCategory Fatal()
{
    return LogCategory(LogLevel::FATAL, 1, LogCategory::LCF_FATAL);
}

template <LogCategory Category, auto FunctionNameString, auto FormatString, class... Args>
static inline void Log_Internal(Logger& logger, const LogChannel& channel, Args&&... args);

HYP_API extern Logger& GetLogger();

} // namespace logging

using logging::LogChannel;

} // namespace hyperion

// Helper macros
#define HYP_DECLARE_LOG_CHANNEL(name) \
    extern hyperion::logging::LogChannel Log_##name

#ifdef HYP_LOG
#error "HYP_LOG already defined!"
#endif

#define HYP_LOG(channel, category, fmt, ...) \
    hyperion::logging::Log_Internal<hyperion::logging::category(), HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT), HYP_STATIC_STRING(fmt "\n")>(hyperion::logging::GetLogger(), Log_##channel, ##__VA_ARGS__)

#ifndef HYP_LOG_ONCE
#define HYP_LOG_ONCE(channel, category, fmt, ...)
#endif

#endif