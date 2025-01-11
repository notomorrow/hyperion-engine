/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_LOGGER_FWD_HPP
#define HYPERION_UTIL_LOGGER_FWD_HPP

#include <core/Defines.hpp>

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

class LogCategory
{
    enum Flags : uint8
    {
        LOG_CATEGORY_NONE           = 0x0,
        LOG_CATEGORY_FATAL          = 0x1
    };

    constexpr LogCategory(uint8 flags, uint16 priority, LogLevel level)
        : m_value(uint32(flags) | (uint32(priority) << 8) | (uint32(level) << 24))
    {
    }

public:
    HYP_FORCE_INLINE static constexpr LogCategory Debug()
        { return LogCategory(LOG_CATEGORY_NONE, 10000, LogLevel::DEBUG); }

    HYP_FORCE_INLINE static constexpr LogCategory Warning()
        { return LogCategory(LOG_CATEGORY_NONE, 1000, LogLevel::WARNING); }

    HYP_FORCE_INLINE static constexpr LogCategory Info()
        { return LogCategory(LOG_CATEGORY_NONE, 100, LogLevel::INFO); }

    HYP_FORCE_INLINE static constexpr LogCategory Error()
        { return LogCategory(LOG_CATEGORY_NONE, 10, LogLevel::ERR); }

    HYP_FORCE_INLINE static constexpr LogCategory Fatal()
        { return LogCategory(LOG_CATEGORY_FATAL, 1, LogLevel::FATAL); }

    constexpr LogCategory(const LogCategory &other)
        : m_value(other.m_value)
    {
    }

    LogCategory &operator=(const LogCategory &other)
    {
        m_value = other.m_value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const LogCategory &other) const
        { return m_value == other.m_value; }

    HYP_FORCE_INLINE constexpr bool operator!=(const LogCategory &other) const
        { return m_value != other.m_value; }

    HYP_FORCE_INLINE constexpr bool operator<(const LogCategory &other) const
        { return GetPriority() < other.GetPriority(); }

    HYP_FORCE_INLINE constexpr uint8 GetFlags() const
        { return m_value & 0xFF; }

    HYP_FORCE_INLINE constexpr uint16 GetPriority() const
        { return uint16((m_value >> 8) & 0xFFFF); }

    HYP_FORCE_INLINE constexpr LogLevel GetLevel() const
        { return LogLevel((m_value >> 24) & 0xFF); }

    uint32          m_value;
};

namespace detail {

template <LogLevel Level, auto FunctionNameString, auto FormatString, class... Args>
static inline void Log_Internal(Logger &logger, const LogChannel &channel, Args &&... args);

} // namespace detail

HYP_API extern Logger &GetLogger();

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
    hyperion::logging::detail::Log_Internal< LogCategory::category().GetLevel(), HYP_PRETTY_FUNCTION_NAME, HYP_STATIC_STRING(fmt) >(hyperion::logging::GetLogger(), hyperion::Log_##channel, ##__VA_ARGS__)

#ifndef HYP_LOG_ONCE
    #define HYP_LOG_ONCE(channel, level, fmt, ...)
#endif

#endif