#ifndef HYPERION_V2_LIB_PATH_HPP
#define HYPERION_V2_LIB_PATH_HPP

#include "String.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {
namespace containers {
namespace detail {

class Path : public String {
protected:
    using Base = String;

public:
    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    Path()
        : String()
    {   
    }

    Path(const String &str)
        : String(str)
    {
    }

    Path(String &&str) noexcept
        : String(std::move(str))
    {
    }

    template <class T, bool IsUtf8>
    Path(const DynString<T, IsUtf8> &str)
    {
        for (auto ch : str) {
            Base::Append(static_cast<String::ValueType>(ch));
        }
    }

    template <class T, bool IsUtf8>
    Path(DynString<T, IsUtf8> &&str) noexcept
    {
        for (auto ch : str) {
            Base::Append(static_cast<String::ValueType>(ch));
        }

        str.Clear();
    }

    Path(const Path &other)
        : String(other)
    {
    }

    Path(Path &&other) noexcept
        : String(std::move(other))
    {
    }

    ~Path() = default;

    // HYP_DEF_STL_BEGIN_END(
    //     Base::Begin(),
    //     Base::End()
    // )
};

} // namespace detail
} // namespace containers

using Path = containers::detail::Path;

} // namespace hyperion

#endif