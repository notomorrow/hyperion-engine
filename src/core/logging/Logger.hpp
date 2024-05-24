/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UTIL_LOGGER_HPP
#define HYPERION_UTIL_LOGGER_HPP

#include <core/Name.hpp>
#include <core/system/Debug.hpp>
#include <core/Defines.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>

namespace hyperion {

enum class LogChannelFlags : uint32
{
    NONE    = 0x0
};

HYP_MAKE_ENUM_FLAGS(LogChannelFlags)

namespace logging {

class Logger;

struct LogMessage
{
    StringView<StringType::UTF8>    message;
};

class HYP_API LogChannel
{
public:
    friend class Logger;

    LogChannel(Name name);
    LogChannel(const LogChannel &other)                 = delete;
    LogChannel &operator=(const LogChannel &other)      = delete;
    LogChannel(LogChannel &&other) noexcept             = delete;
    LogChannel &operator=(LogChannel &&other) noexcept  = delete;
    ~LogChannel();

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetID() const
        { return m_id; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Name GetName() const
        { return m_name; }

    EnumFlags<LogChannelFlags> GetFlags() const
        { return m_flags; }

private:
    uint32                      m_id;
    Name                        m_name;
    EnumFlags<LogChannelFlags>  m_flags;
};

class HYP_API Logger
{
public:
    static constexpr uint max_channels = 64;

    static Logger &GetInstance();

    Logger();
    Logger(Name context_name);
    Logger(const Logger &other)                 = delete;
    Logger &operator=(const Logger &other)      = delete;
    Logger(Logger &&other) noexcept             = delete;
    Logger &operator=(Logger &&other) noexcept  = delete;
    ~Logger()                                   = default;

    void RegisterChannel(LogChannel *channel);

    [[nodiscard]]
    bool IsChannelEnabled(const LogChannel &channel) const;

    void SetChannelEnabled(const LogChannel &channel, bool enabled);

    template <auto FunctionNameString, auto FormatString, class... Args>
    void Log(const LogChannel &channel, Args &&... args)
    {
        if (IsChannelEnabled(channel)) {
            Log(
                channel,
                LogMessage {
                    utilities::Format< containers::helpers::Concat< StaticString("["), FunctionNameString, StaticString("] "), FormatString >::value >(std::forward<Args>(args)...)
                }
            );
        }
    }

private:
    static String GetCurrentFunction();

    void Log(const LogChannel &channel, const LogMessage &message);

    AtomicVar<uint64>                       m_log_mask;

    FixedArray<LogChannel *, max_channels>  m_log_channels;
};

} // namespace logging

using logging::Logger;
using logging::LogChannel;

} // namespace hyperion

// Helper macros
#define HYP_DECLARE_LOG_CHANNEL(name) \
    extern hyperion::logging::LogChannel Log_##name

#define HYP_DEFINE_LOG_CHANNEL(name) \
    static hyperion::logging::LogChannel Log_##name(HYP_NAME(name))

#define HYP_LOG(channel, fmt, ...) \
    hyperion::logging::Logger::GetInstance().Log< hyperion::StaticString(HYP_DEBUG_FUNC_SHORT), hyperion::StaticString(fmt) >(Log_##channel, __VA_ARGS__)

#endif