/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/logging/Logger.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>

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

#pragma region LogMessageQueue

LogMessageQueue::LogMessageQueue(ThreadID owner_thread_id)
    : m_owner_thread_id(owner_thread_id),
      m_buffer_index(0)
{
}

LogMessageQueue::LogMessageQueue(LogMessageQueue &&other) noexcept
    : m_owner_thread_id(std::move(other.m_owner_thread_id)),
      m_buffers(std::move(other.m_buffers)),
      m_buffer_index(other.m_buffer_index.Exchange(0, MemoryOrder::SEQUENTIAL))
{
    other.m_owner_thread_id = ThreadID::Invalid();
    other.m_buffers = { };
}

LogMessageQueue &LogMessageQueue::operator=(LogMessageQueue &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_owner_thread_id = std::move(other.m_owner_thread_id);
    m_buffers = std::move(other.m_buffers);
    m_buffer_index.Set(other.m_buffer_index.Exchange(0, MemoryOrder::SEQUENTIAL), MemoryOrder::SEQUENTIAL);

    other.m_owner_thread_id = ThreadID::Invalid();

    return *this;
}

void LogMessageQueue::PushBytes(ByteView bytes)
{
    ByteBuffer &buffer = m_buffers[m_buffer_index.Get(MemoryOrder::ACQUIRE)];

    const SizeType offset = buffer.Size();

    ubyte *dst = buffer.Data() + offset;

    buffer.SetSize(buffer.Size() + bytes.Size());

    Memory::MemCpy(dst, bytes.Data(), bytes.Size());
}

void LogMessageQueue::SwapBuffers()
{
    m_buffer_index.BitAnd(1, MemoryOrder::RELEASE);
}

#pragma endregion LogMessageQueue

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

LogMessageQueue &LogChannel::GetMessageQueue()
{
    return m_message_queue_container.GetMessageQueue(ThreadID::Current());
}

void LogChannel::Write(ByteView bytes)
{
    GetMessageQueue().PushBytes(bytes);
}

void LogChannel::FlushMessages()
{
    m_message_queue_container.SwapBuffers();
}

#pragma endregion LogChannel

#pragma region Logger

static Logger g_logger;

Logger &Logger::GetInstance()
{
    return g_logger;
}

Logger::Logger()
    : m_log_mask(uint64(-1)),
      m_dirty_channels(uint64(-1))
{
}

Logger::Logger(Name context_name)
    : m_log_mask(uint64(-1)),
      m_dirty_channels(uint64(-1))
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

void Logger::FlushChannels()
{
    for (Bitset::BitIndex bit_index : Bitset(m_dirty_channels.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))) {
        if (LogChannel *channel = m_log_channels[bit_index]) {
            channel->FlushMessages();
        }
    }
}

void Logger::Log(const LogChannel &channel, StringView<StringType::UTF8> message)
{
    puts(message.Data());
}

#pragma endregion Logger

} // namespace logging
} // namespace hyperion