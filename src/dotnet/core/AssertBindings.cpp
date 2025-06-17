/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Assert_Throw(const char* message, const char* funcName, uint32 line)
    {
        if (!message)
        {
            message = "<no message>";
        }

        if (funcName != nullptr)
        {
            HYP_FAIL("%s:%u: Assertion failed!\n\t%s", funcName, line, message);
        }
        else
        {
            HYP_FAIL("Assertion failed!\n\t%s", message);
        }
    }

} // extern "C"