/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/Time.hpp>

#include <Core/Utilities/StringView.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Defines.hpp>
#include <Core/Util.hpp>

#include <Core/Types.hpp>

#ifdef HYP_TRACY
#if defined(HYP_CLANG_OR_GCC) && !defined(TracyFunction)
#define TracyFunction __PRETTY_FUNCTION__
#endif // HYP_TRACY

#include <tracy/Tracy.hpp>

namespace Hyperion {
namespace profiling {

HYP_FORCE_INLINE void SetZoneName(tracy::ScopedZone& zone, ANSIStringView name)
{
    zone.Name(name.Data(), name.Size());
}

} // namespace profiling
} // namespace Hyperion

#define HYP_SCOPE ZoneScoped

#define HYP_NAMED_SCOPE(label) \
    ZoneScoped;                \
    ::Hyperion::profiling::SetZoneName(___tracy_scoped_zone, (label))

// skip formatting the name when no profiler is connected
#define HYP_NAMED_SCOPE_FMT(label, ...)                                                                    \
    ZoneScoped;                                                                                            \
    if (TracyIsConnected)                                                                                  \
    {                                                                                                      \
        const auto _profile_scope_format_string = HYP_FORMAT(label, ##__VA_ARGS__);                        \
        ::Hyperion::profiling::SetZoneName(___tracy_scoped_zone, _profile_scope_format_string.Data());     \
    }

#define HYP_PROFILE_BEGIN HYP_NAMED_SCOPE(*CurrentThreadId().GetName())

#define HYP_PROFILE_FRAME_MARK FrameMark
#define HYP_PROFILE_FRAME_MARK_NAMED(name) FrameMarkNamed(name)

#else
#define HYP_SCOPE
#define HYP_NAMED_SCOPE(...)
#define HYP_NAMED_SCOPE_FMT(label, ...)
#define HYP_PROFILE_BEGIN
#define HYP_PROFILE_FRAME_MARK
#define HYP_PROFILE_FRAME_MARK_NAMED(name)
#endif
