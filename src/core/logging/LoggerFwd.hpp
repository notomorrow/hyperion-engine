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

#define HYP_LOG(channel, level, fmt, ...) \
    hyperion::logging::detail::Log_Internal< level, HYP_PRETTY_FUNCTION_NAME, HYP_STATIC_STRING(fmt) >(hyperion::logging::GetLogger(), hyperion::Log_##channel, ##__VA_ARGS__)

#ifndef HYP_LOG_ONCE
    #define HYP_LOG_ONCE(channel, level, fmt, ...)
#endif

#endif