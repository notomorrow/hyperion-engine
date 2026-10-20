/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/StringView.hpp>

namespace Hyperion::DataProcessing {

static inline bool IsValidIdentifier(const ANSIStringView& inStr)
{
    size_t len = inStr.Size();

    if (len == 0)
    {
        return false;
    }

    const char* curr = inStr.Data();

    bool isValidIdent = *curr == '_'
        || (*curr >= 'a' && *curr <= 'z')
        || (*curr >= 'A' && *curr<= 'Z');

    while (isValidIdent && len)
    {
        --len;
        ++curr;

        isValidIdent &= (*curr == '_'
            || (*curr >= 'a' && *curr <= 'z')
            || (*curr >= 'A' && *curr <= 'Z')
            || (*curr >= '0' && *curr <= '9'));
    }

    return isValidIdent;
}

} // namespace Hyperion::DataProcessing