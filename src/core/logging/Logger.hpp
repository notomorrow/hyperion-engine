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

class HYP_API LogMessageQueue
{
public:
    LogMessageQueue(ThreadID owner_thread_id = ThreadID::Invalid());
    LogMessageQueue(const LogMessageQueue &other)                   = delete;
    LogMessageQueue &operator=(const LogMessageQueue &other)        = delete;
    LogMessageQueue(LogMessageQueue &&other) noexcept;
    LogMessageQueue &operator=(LogMessageQueue &&other) noexcept;
    ~LogMessageQueue()                                              = default;

    void PushBytes(ByteView bytes);
    void SwapBuffers();

private:
    ThreadID                    m_owner_thread_id;

    FixedArray<ByteBuffer, 2>   m_buffers;
    AtomicVar<uint32>           m_buffer_index;
};

struct HYP_API LogMessageQueueContainer
{
    FixedArray<LogMessageQueue, MathUtil::FastLog2_Pow2(uint64(ThreadName::THREAD_STATIC))> static_thread_queues;
    HashMap<ThreadID, LogMessageQueue>                                                      dynamic_thread_queues;
    Mutex                                                                                   dynamic_thread_queues_mutex;

    LogMessageQueueContainer()
    {
        for (uint64 i = 0; i < static_thread_queues.Size(); i++) {
            const uint64 thread_name_value = 1ull << i;
            const ThreadName thread_name = ThreadName(thread_name_value);

            const ThreadID thread_id = Threads::GetThreadID(thread_name);

            if (!thread_id.IsValid()) {
                continue;
            }

            AssertThrow(!thread_id.IsDynamic());

            static_thread_queues[i] = LogMessageQueue(thread_id);
        }
    }

    LogMessageQueue &GetMessageQueue(ThreadID thread_id)
    {
        if (thread_id.IsDynamic()) {
            Mutex::Guard guard(dynamic_thread_queues_mutex);

            auto it = dynamic_thread_queues.Find(thread_id);

            if (it == dynamic_thread_queues.End()) {
                it = dynamic_thread_queues.Insert({
                    thread_id,
                    LogMessageQueue(thread_id)
                }).first;
            }

            return it->second;
        }

#ifdef HYP_DEBUG_MODE
        AssertThrow(MathUtil::IsPowerOfTwo(thread_id.value));
        AssertThrow(MathUtil::FastLog2_Pow2(thread_id.value) < static_thread_queues.Size());
#endif

        return static_thread_queues[MathUtil::FastLog2_Pow2(thread_id.value)];
    }

    void SwapBuffers()
    {
        for (LogMessageQueue &queue : static_thread_queues) {
            queue.SwapBuffers();
        }

        Mutex::Guard guard(dynamic_thread_queues_mutex);

        for (auto &it : dynamic_thread_queues) {
            it.second.SwapBuffers();
        }
    }
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

    void Write(ByteView bytes);

    void FlushMessages();

private:
    LogMessageQueue &GetMessageQueue();

    uint32                      m_id;
    Name                        m_name;
    EnumFlags<LogChannelFlags>  m_flags;
    LogMessageQueueContainer    m_message_queue_container;
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

    void FlushChannels();

    template <auto FormatString, class... Args>
    void Log(const LogChannel &channel, Args &&... args)
    {
        if (IsChannelEnabled(channel)) {
            Log(channel, utilities::Format<FormatString>(std::forward<Args>(args)...));
        }
    }

private:
    void Log(const LogChannel &channel, StringView<StringType::UTF8> message);

    AtomicVar<uint64>                       m_log_mask;

    AtomicVar<uint64>                       m_dirty_channels;

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
    hyperion::logging::Logger::GetInstance().Log< hyperion::StaticString(fmt) >(Log_##channel, __VA_ARGS__)

#endif