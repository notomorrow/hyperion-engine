/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace threading {

class Spinlock
{
    static constexpr uint32 g_maxSpins = 1024; // before we yield
public:
    Spinlock(volatile int64* value)
        : m_value(value)
    {
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    Spinlock(Spinlock&& other) noexcept = delete;
    Spinlock& operator=(Spinlock&& other) noexcept = delete;

    ~Spinlock() = default;

    void LockWriter()
    {
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicBitOr(m_value, 0x1);

        while (ustate & (~0u << 1))
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= g_maxSpins)
            {
                // yield to other threads
                Threads::Sleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicBitOr(m_value, 0);
        }
    }

    void UnlockWriter()
    {
        AtomicBitAnd(m_value, ~0x1);
    }

    void LockReader()
    {
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicAdd(m_value, 2);

        while (ustate & 0x1)
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= g_maxSpins)
            {
                Threads::Sleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicAdd(m_value, 0);
        }
    }

    void UnlockReader()
    {
        AtomicSub(m_value, 2);
    }

private:
    volatile int64* m_value;
};

} // namespace threading

using threading::Spinlock;

} // namespace hyperion
