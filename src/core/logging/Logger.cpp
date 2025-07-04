/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/logging/Logger.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/NotNullPtr.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace logging {

HYP_API Logger& GetLogger()
{
    return Logger::GetInstance();
}

HYP_API ANSIStringView GetCurrentThreadName()
{
    return *Threads::CurrentThreadId().GetName();
}

class LogChannelIdGenerator
{
public:
    LogChannelIdGenerator() = default;
    LogChannelIdGenerator(const LogChannelIdGenerator& other) = delete;
    LogChannelIdGenerator& operator=(const LogChannelIdGenerator& other) = delete;
    LogChannelIdGenerator(LogChannelIdGenerator&& other) noexcept = delete;
    LogChannelIdGenerator& operator=(LogChannelIdGenerator&& other) noexcept = delete;
    ~LogChannelIdGenerator() = default;

    HYP_NODISCARD HYP_FORCE_INLINE uint32 Next()
    {
        return m_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

private:
    AtomicVar<uint32> m_counter { 0 };
};

static LogChannelIdGenerator g_logChannelIdGenerator {};

#pragma region LoggerOutputStream

struct LoggerRedirect
{
    Bitset channelMask;
    void* context = nullptr;
    LoggerWriteFnPtr writeFnptr;
    LoggerWriteFnPtr writeErrorFnptr;
};

class BasicLoggerOutputStream : public ILoggerOutputStream
{
public:
    static constexpr uint64 writeFlag = 0x1;
    static constexpr uint64 readMask = uint64(-1) & ~writeFlag;

    static BasicLoggerOutputStream& GetDefaultInstance()
    {
        static BasicLoggerOutputStream instance { stdout, stderr };

        return instance;
    }

    BasicLoggerOutputStream(FILE* output, FILE* outputError)
        : m_output(output),
          m_outputError(outputError),
          m_rwMarker(0),
          m_redirectEnabledMask(0),
          m_redirectIdCounter(-1)
    {
        for (uint32 i = 0; i < Logger::maxChannels; i++)
        {
            m_contexts[i] = (void*)this;
            m_writeFnptrTable[i] = &Write_Static;
            m_writeErrorFnptrTable[i] = &WriteError_Static;
        }
    }

    virtual ~BasicLoggerOutputStream() override = default;

    // @FIXME: Needs to redirect all CHILD channels as well, not parent channels!
    virtual int AddRedirect(const Bitset& channelMask, void* context, LoggerWriteFnPtr writeFnptr, LoggerWriteFnPtr writeErrorFnptr) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (state & readMask)
        {
            state = m_rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        const int id = ++m_redirectIdCounter;

        m_redirects.Emplace(id, LoggerRedirect { channelMask, context, writeFnptr, writeErrorFnptr });

        for (Bitset::BitIndex bitIndex : channelMask)
        {
            m_contexts[bitIndex] = context;
            m_writeFnptrTable[bitIndex] = writeFnptr;
            m_writeErrorFnptrTable[bitIndex] = writeErrorFnptr;
        }

        m_redirectEnabledMask.BitOr(1ull << channelMask.LastSetBitIndex(), MemoryOrder::RELEASE);

        m_rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);

        return id;
    }

    virtual void RemoveRedirect(int id) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (state & readMask)
        {
            state = m_rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        auto it = m_redirects.Find(id);

        if (it == m_redirects.End())
        {
            return;
        }

        for (Bitset::BitIndex bitIndex : it->second.channelMask)
        {
            if (m_contexts[bitIndex] != it->second.context)
            {
                continue;
            }

            m_contexts[bitIndex] = (void*)this;
            m_writeFnptrTable[bitIndex] = &Write_Static;
            m_writeErrorFnptrTable[bitIndex] = &WriteError_Static;
        }

        m_redirects.Erase(it);

        m_redirectEnabledMask.BitAnd(~(1ull << it->second.channelMask.LastSetBitIndex()), MemoryOrder::RELEASE);

        m_rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);
    }

    virtual void Write(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.Id() >= Logger::maxChannels)
        {
            channelPtr = &Log_Core;
            return;
        }

        // set read flags
        uint64 state;
        while (((state = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE)) & writeFlag))
        {
            m_rwMarker.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            HYP_WAIT_IDLE();
        }

        void* context = m_contexts[channelPtr->Id()];
        LoggerWriteFnPtr fnptr = m_writeFnptrTable[channelPtr->Id()];

        uint32 bitIndex;
        uint64 mask = channelPtr->GetMaskBitset().ToUInt64() & ~(1ull << channelPtr->Id());

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask.Get(MemoryOrder::ACQUIRE) & (1ull << bitIndex))
            {
                context = m_contexts[bitIndex];
                fnptr = m_writeFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        fnptr(context, *channelPtr, message);

        m_rwMarker.Decrement(2, MemoryOrder::RELEASE);
    }

    virtual void WriteError(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.Id() >= Logger::maxChannels)
        {
            channelPtr = &Log_Core;
            return;
        }

        // set read flags
        uint64 state;
        while (((state = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE)) & writeFlag))
        {
            m_rwMarker.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            HYP_WAIT_IDLE();
        }

        void* context = m_contexts[channelPtr->Id()];
        LoggerWriteFnPtr fnptr = m_writeErrorFnptrTable[channelPtr->Id()];

        uint32 bitIndex;
        uint64 mask = channelPtr->GetMaskBitset().ToUInt64() & ~(1ull << channelPtr->Id());

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask.Get(MemoryOrder::ACQUIRE) & (1ull << bitIndex))
            {
                context = m_contexts[bitIndex];
                fnptr = m_writeErrorFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        fnptr(context, *channelPtr, message);

        m_rwMarker.Decrement(2, MemoryOrder::RELEASE);
    }

private:
    static void Write_Static(void* context, const LogChannel& channel, const LogMessage& message)
    {
        std::fwrite(*message.message, 1, message.message.Size(), ((BasicLoggerOutputStream*)context)->m_output);
    }

    static void WriteError_Static(void* context, const LogChannel& channel, const LogMessage& message)
    {
        std::fwrite(*message.message, 1, message.message.Size(), ((BasicLoggerOutputStream*)context)->m_outputError);
    }

    AtomicVar<uint64> m_rwMarker;

    FILE* m_output;
    FILE* m_outputError;

    void* m_contexts[Logger::maxChannels];

    LoggerWriteFnPtr m_writeFnptrTable[Logger::maxChannels];
    LoggerWriteFnPtr m_writeErrorFnptrTable[Logger::maxChannels];

    Mutex m_mutex;
    HashMap<int, LoggerRedirect> m_redirects;
    AtomicVar<uint64> m_redirectEnabledMask;
    int m_redirectIdCounter;
};

#pragma endregion LoggerOutputStream

#pragma region LogChannel

LogChannel::LogChannel(Name name)
    : m_id(g_logChannelIdGenerator.Next()),
      m_name(name),
      m_flags(LogChannelFlags::NONE),
      m_parentChannel(nullptr),
      m_maskBitset(1ull << m_id)
{
}

LogChannel::LogChannel(Name name, LogChannel* parentChannel)
    : LogChannel(name)
{
    m_parentChannel = parentChannel;

    if (m_parentChannel != nullptr)
    {
        m_maskBitset |= m_parentChannel->m_maskBitset;
    }
}

LogChannel::~LogChannel()
{
}

#pragma endregion LogChannel

#pragma region LoggerImpl

class LoggerImpl
{
public:
    friend class Logger;

    LoggerImpl(ILoggerOutputStream& outputStream)
        : m_logMask(-1),
          m_outputStream(&outputStream)
    {
    }

private:
    AtomicVar<Logger::ChannelMask> m_logMask;
    FixedArray<LogChannel*, Logger::maxChannels> m_logChannels;
    LinkedList<LogChannel> m_dynamicLogChannels;
    mutable Mutex m_dynamicLogChannelsMutex;
    NotNullPtr<ILoggerOutputStream> m_outputStream;
};

#pragma endregion LoggerImpl

#pragma region Logger

Logger& Logger::GetInstance()
{
    static Logger instance;

    return instance;
}

Logger::Logger()
    : Logger(BasicLoggerOutputStream::GetDefaultInstance())
{
}

Logger::Logger(ILoggerOutputStream& outputStream)
    : m_pImpl(MakePimpl<LoggerImpl>(outputStream))
{
}

Logger::~Logger() = default;

ILoggerOutputStream* Logger::GetOutputStream() const
{
    return m_pImpl->m_outputStream;
}

const LogChannel* Logger::FindLogChannel(WeakName name) const
{
    for (LogChannel* channel : m_pImpl->m_logChannels)
    {
        if (!channel)
        {
            break;
        }

        if (channel->GetName() == name)
        {
            return channel;
        }
    }

    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    for (const LogChannel& channel : m_pImpl->m_dynamicLogChannels)
    {
        return &channel;
    }

    return nullptr;
}

void Logger::RegisterChannel(LogChannel* channel)
{
    HYP_CORE_ASSERT(channel != nullptr);
    HYP_CORE_ASSERT(channel->Id() < m_pImpl->m_logChannels.Size());

    m_pImpl->m_logChannels[channel->Id()] = channel;
}

LogChannel* Logger::CreateDynamicLogChannel(Name name, LogChannel* parentChannel)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    return &m_pImpl->m_dynamicLogChannels.EmplaceBack(name, parentChannel);
}

void Logger::DestroyDynamicLogChannel(Name name)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    auto it = m_pImpl->m_dynamicLogChannels.FindIf([name](const auto& item)
        {
            return item.GetName() == name;
        });

    if (it == m_pImpl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_pImpl->m_dynamicLogChannels.Erase(it);
}

void Logger::DestroyDynamicLogChannel(LogChannel* channel)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    auto it = m_pImpl->m_dynamicLogChannels.FindIf([channel](const auto& item)
        {
            return &item == channel;
        });

    if (it == m_pImpl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_pImpl->m_dynamicLogChannels.Erase(it);
}

bool Logger::IsChannelEnabled(const LogChannel& channel) const
{
    const uint64 channelMaskBitset = channel.GetMaskBitset().ToUInt64();

    return (m_pImpl->m_logMask.Get(MemoryOrder::RELAXED) & channelMaskBitset) == channelMaskBitset;
}

void Logger::SetChannelEnabled(const LogChannel& channel, bool enabled)
{
    if (enabled)
    {
        m_pImpl->m_logMask.BitOr(1ull << channel.Id(), MemoryOrder::RELAXED);
    }
    else
    {
        m_pImpl->m_logMask.BitAnd(~(1ull << channel.Id()), MemoryOrder::RELAXED);
    }
}

void Logger::Log(const LogChannel& channel, const LogMessage& message)
{
    if (uint32(message.level) >= uint32(LogLevel::WARNING))
    {
        m_pImpl->m_outputStream->WriteError(channel, message);
    }
    else
    {
        m_pImpl->m_outputStream->Write(channel, message);
    }
}

#pragma endregion Logger

} // namespace logging

HYP_DECLARE_LOG_CHANNEL(Core);

namespace logging {
// For Assert() to work with logging
template HYP_API void LogDynamic<Debug(), HYP_MAKE_CONST_ARG(&Log_Core)>(Logger&, const char*);
} // namespace logging

} // namespace hyperion