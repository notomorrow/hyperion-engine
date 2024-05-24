/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/logging/Logger.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>

#include <core/system/StackDump.hpp>

namespace hyperion {
namespace logging {

class LogChannelIDGenerator
{
public:
    LogChannelIDGenerator()                                                     = default;
    LogChannelIDGenerator(const LogChannelIDGenerator &other)                   = delete;
    LogChannelIDGenerator &operator=(const LogChannelIDGenerator &other)        = delete;
    LogChannelIDGenerator(LogChannelIDGenerator &&other) noexcept               = delete;
    LogChannelIDGenerator &operator=(LogChannelIDGenerator &&other) noexcept    = delete;
    ~LogChannelIDGenerator()                                                    = default;

    uint32 ForName(Name name)
    {
        Mutex::Guard guard(mutex);

        auto it = name_map.Find(name);

        if (it != name_map.End()) {
            return it->second;
        }

        const uint32 id = id_counter++;

        name_map.Insert(name, id);

        return id;
    }

private:
    uint32                  id_counter = 0u;
    Mutex                   mutex;
    HashMap<Name, uint64>   name_map;
};

static LogChannelIDGenerator g_log_channel_id_generator { };

#pragma region LogChannel

LogChannel::LogChannel(Name name)
    : m_id(g_log_channel_id_generator.ForName(name)),
      m_name(name),
      m_flags(LogChannelFlags::NONE)
{
}

LogChannel::~LogChannel()
{
}

#pragma endregion LogChannel

#pragma region Logger

static Logger g_logger;

Logger &Logger::GetInstance()
{
    return g_logger;
}

Logger::Logger()
    : m_log_mask(uint64(-1))
{
}

Logger::Logger(Name context_name)
    : m_log_mask(uint64(-1))
{
}

void Logger::RegisterChannel(LogChannel *channel)
{
    AssertThrow(channel != nullptr);
    AssertThrow(channel->GetID() < m_log_channels.Size());

    m_log_channels[channel->GetID()] = channel;
}

bool Logger::IsChannelEnabled(const LogChannel &channel) const
{
    return m_log_mask.Get(MemoryOrder::ACQUIRE) & (1ull << channel.GetID());
}

void Logger::SetChannelEnabled(const LogChannel &channel, bool enabled)
{
    if (enabled) {
        m_log_mask.BitOr(1ull << channel.GetID(), MemoryOrder::RELEASE);
    } else {
        m_log_mask.BitAnd(~(1ull << channel.GetID()), MemoryOrder::RELEASE);
    }
}

void Logger::Log(const LogChannel &channel, const LogMessage &message)
{
    // std::printf("%s %s\n", *message.function, *message.message);

    std::puts(*message.message);
}

String Logger::GetCurrentFunction()
{
    return StackDump(2).GetTrace().Front();
}

#pragma endregion Logger

} // namespace logging
} // namespace hyperion