/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/DataRaceDetector.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Bitset.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {
namespace threading {

#ifdef HYP_ENABLE_MT_CHECK

#pragma region DataAccessScope

DataRaceDetector::DataAccessScope::DataAccessScope(EnumFlags<DataAccessFlags> flags, const DataRaceDetector& detector, const DataAccessState& state)
    : m_detector(const_cast<DataRaceDetector&>(detector)),
      m_thread_id(ThreadID::Current())
{
    // Check if we should suppress data race detection
    if (IsGlobalContextActive<SuppressDataRaceDetectorContext>())
    {
        m_flags = 0;

        return;
    }

    m_flags = m_detector.AddAccess(m_thread_id, flags, state);
}

DataRaceDetector::DataAccessScope::~DataAccessScope()
{
    m_detector.RemoveAccess(m_thread_id, m_flags);
}

#pragma endregion DataAccessScope

#pragma region DataRaceDetector

static Span<const DataRaceDetector::ThreadAccessState> GetPreallocatedStates()
{
    static const struct PreallocatedThreadAccessStates
    {
        FixedArray<DataRaceDetector::ThreadAccessState, DataRaceDetector::num_preallocated_states> data;

        PreallocatedThreadAccessStates()
        {
            for (SizeType i = 0; i < DataRaceDetector::num_preallocated_states; i++)
            {
                const StaticThreadID thread_id = StaticThreadID(i);

                data[i] = DataRaceDetector::ThreadAccessState { thread_id, DataAccessFlags::ACCESS_NONE };
            }
        }
    } preallocated_states {};

    return preallocated_states.data;
}

DataRaceDetector::DataRaceDetector()
    : m_preallocated_states(GetPreallocatedStates()),
      m_writers(0),
      m_readers(0)
{
    AssertDebug(m_preallocated_states.Size() == num_preallocated_states);
}

DataRaceDetector::~DataRaceDetector()
{
    DebugLog(LogType::Debug, "DataRaceDetector destroyed : %p\n", this);
}

EnumFlags<DataAccessFlags> DataRaceDetector::AddAccess(ThreadID thread_id, EnumFlags<DataAccessFlags> access_flags, const DataAccessState& state)
{
    Threads::AssertOnThread(thread_id);

    uint32 index = ~0u;

    if (thread_id.IsStatic())
    {
        // ensure no writer from other thread
        index = static_cast<const StaticThreadID&>(thread_id).GetStaticThreadIndex();
        AssertDebug(index < num_preallocated_states);

        // disable bits that are already enabled
        access_flags &= ~m_preallocated_states[index].access;

        // If no access flags will be changed, return now (don't overwrite state)
        if (access_flags == DataAccessFlags::ACCESS_NONE)
        {
            return DataAccessFlags::ACCESS_NONE;
        }

        m_preallocated_states[index].access |= access_flags;
        m_preallocated_states[index].state = state;
    }
    else
    {
        Mutex::Guard guard(m_dynamic_states_mutex);

        // debugging
        AssertThrow(m_dynamic_states.Size() <= m_dynamic_states.Capacity());

        auto it = m_dynamic_states.FindIf([thread_id](const ThreadAccessState& thread_access_state)
            {
                return thread_access_state.thread_id == thread_id;
            });

        if (it == m_dynamic_states.End())
        {
            ThreadAccessState& thread_access_state = m_dynamic_states.EmplaceBack();
            thread_access_state.thread_id = thread_id;
            thread_access_state.original_index = uint32(m_dynamic_states.Size() - 1);

            it = &thread_access_state;
        }

        // disable bits that are already enabled
        access_flags &= ~it->access;

        // If no access flags will be changed, return now (don't overwrite state)
        if (access_flags == DataAccessFlags::ACCESS_NONE)
        {
            return DataAccessFlags::ACCESS_NONE;
        }

        it->access |= access_flags;
        it->state = state;

        index = (it - m_dynamic_states.Begin()) + num_preallocated_states;
    }

    if (index == ~0u)
    {
        return DataAccessFlags::ACCESS_NONE;
    }

    const uint64 test_mask = 1ull << index;

    uint64 mask;

    if (access_flags & DataAccessFlags::ACCESS_WRITE)
    {
        if (((mask = m_writers.BitOr(test_mask, MemoryOrder::ACQUIRE_RELEASE)) & ~test_mask))
        {
            LogDataRace(0ull, mask);
            HYP_FAIL("Potential data race detected: Attempt to acquire write access while other thread is writing. Write mask: %llu", mask);
        }

        if ((mask = m_readers.BitOr(test_mask, MemoryOrder::ACQUIRE_RELEASE)) & ~test_mask)
        {
            LogDataRace(mask, 0ull);
            HYP_FAIL("Potential data race detected: Attempt to acquire write access while other thread is reading. Write mask: %llu", mask);
        }
    }
    else
    {
        m_readers.BitOr(test_mask, MemoryOrder::RELEASE);

        if ((mask = m_writers.Get(MemoryOrder::ACQUIRE)) & ~test_mask)
        {
            LogDataRace(mask, 0ull);
            HYP_FAIL("Potential data race detected: Attempt to acquire read access while other thread is writing. Write mask: %llu", mask);
        }
    }

    // return the new bits that were enabled, so scopes know which ones to disable on exit
    return access_flags;
}

void DataRaceDetector::RemoveAccess(ThreadID thread_id, EnumFlags<DataAccessFlags> access_flags)
{
    if (access_flags == DataAccessFlags::ACCESS_NONE)
    {
        return;
    }

    Threads::AssertOnThread(thread_id);

    uint32 index = ~0u;
    EnumFlags<DataAccessFlags> flags = DataAccessFlags::ACCESS_NONE;

    if (thread_id.IsStatic())
    {
        // ensure no writer from other thread
        index = static_cast<const StaticThreadID&>(thread_id).GetStaticThreadIndex();
        AssertDebug(index < num_preallocated_states);

        flags = m_preallocated_states[index].access;
        m_preallocated_states[index].access &= ~access_flags;
    }
    else
    {
        Mutex::Guard guard(m_dynamic_states_mutex);

        // debugging
        AssertThrow(m_dynamic_states.Size() <= m_dynamic_states.Capacity());

        auto it = m_dynamic_states.FindIf([thread_id](const ThreadAccessState& thread_access_state)
            {
                return thread_access_state.thread_id == thread_id;
            });

        if (it == m_dynamic_states.End())
        {
            return;
        }

        index = it->original_index + num_preallocated_states;
        flags = it->access;

        // remove if no longer needed (all used flags removed)
        if (access_flags == flags)
        {
            m_dynamic_states.Erase(it);
        }
    }

    if (index == ~0u)
    {
        return;
    }

    const uint64 test_mask = 1ull << index;

    if (access_flags & DataAccessFlags::ACCESS_READ)
    {
        m_readers.BitAnd(~test_mask, MemoryOrder::RELEASE);
    }

    if (access_flags & DataAccessFlags::ACCESS_WRITE)
    {
        m_writers.BitAnd(~test_mask, MemoryOrder::RELEASE);
    }
}

void DataRaceDetector::LogDataRace(uint64 readers_mask, uint64 writers_mask) const
{
    Array<Pair<ThreadID, DataAccessState>> reader_thread_ids;
    Array<Pair<ThreadID, DataAccessState>> writer_thread_ids;
    GetThreadIDs(readers_mask, writers_mask, reader_thread_ids, writer_thread_ids);

    String reader_threads_string = "<None>";

    if (reader_thread_ids.Any())
    {
        reader_threads_string.Clear();

        for (SizeType i = 0; i < reader_thread_ids.Size(); i++)
        {
            if (i == 0)
            {
                reader_threads_string = HYP_FORMAT("{} ({}) (at: {}, message: {})", reader_thread_ids[i].first.GetName(), reader_thread_ids[i].first.GetValue(), reader_thread_ids[i].second.current_function, reader_thread_ids[i].second.message);
            }
            else
            {
                reader_threads_string = HYP_FORMAT("{}, {} ({}) (at: {}, message: {})", reader_threads_string, reader_thread_ids[i].first.GetName(), reader_thread_ids[i].first.GetValue(), reader_thread_ids[i].second.current_function, reader_thread_ids[i].second.message);
            }
        }
    }

    String writer_threads_string = "<None>";

    if (writer_thread_ids.Any())
    {
        writer_threads_string.Clear();

        for (SizeType i = 0; i < writer_thread_ids.Size(); i++)
        {
            if (i == 0)
            {
                writer_threads_string = HYP_FORMAT("{} ({}) (at: {}, message: {})", writer_thread_ids[i].first.GetName(), writer_thread_ids[i].first.GetValue(), writer_thread_ids[i].second.current_function, writer_thread_ids[i].second.message);
            }
            else
            {
                writer_threads_string = HYP_FORMAT("{}, {} ({}) (at: {}, message: {})", writer_threads_string, writer_thread_ids[i].first.GetName(), writer_thread_ids[i].first.GetValue(), writer_thread_ids[i].second.current_function, writer_thread_ids[i].second.message);
            }
        }
    }

    HYP_LOG(DataRaceDetector, Error, "Data race detected: Current thread: {} ({}), Writer threads: {}, Reader threads: {}",
        ThreadID::Current().GetName(), ThreadID::Current().GetValue(),
        writer_threads_string,
        reader_threads_string);
}

void DataRaceDetector::GetThreadIDs(uint64 readers_mask, uint64 writers_mask, Array<Pair<ThreadID, DataAccessState>>& out_reader_thread_ids, Array<Pair<ThreadID, DataAccessState>>& out_writer_thread_ids) const
{
    Mutex::Guard guard(m_dynamic_states_mutex);

    for (Bitset::BitIndex bit_index : Bitset(readers_mask))
    {
        if (bit_index >= num_preallocated_states)
        {
            const uint32 dynamic_state_index = bit_index - num_preallocated_states;
            AssertThrowMsg(dynamic_state_index < m_dynamic_states.Size(), "Invalid dynamic state index: %u; Out of range of elements: %u", dynamic_state_index, m_dynamic_states.Size());

            out_reader_thread_ids.EmplaceBack(m_dynamic_states[dynamic_state_index].thread_id, m_dynamic_states[dynamic_state_index].state);
        }
        else
        {
            AssertThrow(bit_index < m_preallocated_states.Size());

            out_reader_thread_ids.EmplaceBack(m_preallocated_states[bit_index].thread_id, m_preallocated_states[bit_index].state);
        }
    }

    for (Bitset::BitIndex bit_index : Bitset(writers_mask))
    {
        if (bit_index >= num_preallocated_states)
        {
            const uint32 dynamic_state_index = bit_index - num_preallocated_states;
            AssertThrowMsg(dynamic_state_index < m_dynamic_states.Size(), "Invalid dynamic state index: %u; Out of range of elements: %u", dynamic_state_index, m_dynamic_states.Size());

            out_writer_thread_ids.EmplaceBack(m_dynamic_states[dynamic_state_index].thread_id, m_dynamic_states[dynamic_state_index].state);
        }
        else
        {
            AssertThrow(bit_index < m_preallocated_states.Size());

            out_writer_thread_ids.EmplaceBack(m_preallocated_states[bit_index].thread_id, m_preallocated_states[bit_index].state);
        }
    }
}

#pragma endregion DataRaceDetector

#endif // HYP_ENABLE_MT_CHECK

} // namespace threading
} // namespace hyperion