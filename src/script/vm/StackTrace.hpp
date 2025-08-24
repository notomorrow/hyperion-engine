#ifndef STACK_TRACE_HPP
#define STACK_TRACE_HPP

#include <core/Core.hpp>

struct StackTrace
{
    int callAddresses[10];

    StackTrace()
    {
        hyperion::Memory::MemSet(callAddresses, 0, sizeof(callAddresses));
    }

    StackTrace(const StackTrace &other)
    {
        hyperion::Memory::MemCpy(callAddresses, other.callAddresses, sizeof(callAddresses));
    }

    StackTrace &operator=(const StackTrace &other)
    {
        hyperion::Memory::MemCpy(callAddresses, other.callAddresses, sizeof(callAddresses));

        return *this;
    }

    ~StackTrace() = default;
};

#endif