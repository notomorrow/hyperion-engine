#ifndef HYPERION_FORMAT_HPP
#define HYPERION_FORMAT_HPP

#include <core/utilities/StringView.hpp>
#include <core/containers/String.hpp>
#include <core/containers/StaticString.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <utility> // for std::tuple

namespace hyperion {
namespace utilities {

template <int StringType, class T>
struct Formatter;

template <int StringType>
struct Formatter<StringType, int>
{
    auto operator()(int value) const
    {
        return containers::detail::String<StringType>::template ToString(value);
    }
};

template <int StringType>
struct Formatter<StringType, float>
{
    auto operator()(float value) const
    {
        memory::ByteBuffer byte_buffer;
        byte_buffer.SetSize(10);

        int result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), "%f", value);

        if (result_size > byte_buffer.Size()) {
            byte_buffer.SetSize(result_size);

            result_size = std::snprintf(reinterpret_cast<char *>(byte_buffer.Data()), byte_buffer.Size(), "%f", value);
        }

        return containers::detail::String<StringType>(byte_buffer);
    }
};

template <int StringType, SizeType Size>
struct Formatter<StringType, StaticString<Size>>
{
    auto operator()(const StaticString<Size> &value) const
    {
        return containers::detail::String<StringType>(value.Data());
    }
};
template <int StringType, int OtherStringType>
struct Formatter<StringType, containers::detail::String<OtherStringType>>
{
    auto operator()(const containers::detail::String<OtherStringType> &value) const
    {
        return value.ToUTF8();
    }
};
template <int StringType>
struct Formatter<StringType, char *>
{
    auto operator()(const char *value) const
    {
        return containers::detail::String<StringType>(value);
    }
};

namespace detail {

#pragma region ConcatRuntimeStrings

template <int StringType>
struct ConcatRuntimeStrings_Impl;

template <int StringType>
struct ConcatRuntimeStrings_Impl
{
    template <class SecondStringType>
    constexpr auto Impl(const containers::detail::String<StringType> &str0, SecondStringType &&str1) const
    {
        return str0 + str1;
    }

    constexpr auto operator()(const containers::detail::String<StringType> &str0) const
    {
        return str0;
    }

    template <class SecondStringType>
    constexpr auto operator()(const containers::detail::String<StringType> &str0, SecondStringType &&str1) const
    {
        if (str0.Empty() && str1.Empty()) {
            return containers::detail::String<StringType>::empty;
        } else if (str0.Empty()) {
            return str1;
        } else if (str1.Empty()) {
            return str0;
        } else {
            return Impl(str0, str1);
        }
    }

    template <class SecondStringType, class ...OtherStringType>
    constexpr auto operator()(const containers::detail::String<StringType> &str0, SecondStringType &&str1, OtherStringType &&... other) const
    {
        if (str0.Empty() && str1.Empty()) {
            return ConcatRuntimeStrings_Impl()(containers::detail::String<StringType>::empty, other...);
        } else if (str0.Empty()) {
            return ConcatRuntimeStrings_Impl()(str1, other...);
        } else if (str1.Empty()) {
            return ConcatRuntimeStrings_Impl()(str0, other...);
        } else {
            return ConcatRuntimeStrings_Impl()(Impl(str0, str1), std::forward<OtherStringType>(other)...);
        }
    }
};

template <int StringType, class... Args>
constexpr auto ConcatRuntimeStrings(Args &&... strings)
{
    return ConcatRuntimeStrings_Impl<StringType>()(std::forward<Args>(strings)...);
}

#pragma endregion ConcatRuntimeStrings

#pragma region FormatString_BadFormat

template <auto Str>
struct FormatString_BadFormat : std::false_type
{
};

template <auto Str, auto Index>
struct FormatString_BadFormat_IndexOutOfBounds : std::false_type
{
};

#pragma endregion FormatString_BadFormat

#pragma region FormatString_FormatElement

template <int StringType, class T>
containers::detail::String<StringType> FormatString_FormatElement_Runtime(const T &element)
{
    static_assert(implementation_exists< Formatter< StringType, NormalizedType< T > > >, "No Formatter specialization exists for type");

    // if-constexpr is to prevent a huge swath of errors preventing the user from seeing the assertion failure.
    if constexpr (implementation_exists< Formatter< StringType, NormalizedType< T > > >) {
        return Formatter< StringType, NormalizedType< T > >{}(element);
    } else {
        return containers::detail::String<StringType>::empty;
    }
}

#pragma endregion FormatString_FormatElement

#pragma region FormatString_BuildTuple

template <auto Str, class Transformer, SizeType SubIndex = 0>
struct FormatString_BuildTuple;

template <auto Str, class Transformer, SizeType StringIndexStart = 0, SizeType StringIndexEnd = 0, SizeType SubIndex = 0>
struct FormatString_BuildTuple_Impl;

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple_Impl<Str, Transformer, SizeType(-1), SizeType(-1), SubIndex>
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        return std::tuple { Str };
    }
};

template <auto Str, class Transformer, SizeType StringIndexStart, SizeType StringIndexEnd, SizeType SubIndex>
struct FormatString_BuildTuple_Impl
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        constexpr auto inner_value = containers::helpers::Substr< Str, StringIndexStart + 1, StringIndexEnd >::value;

        if constexpr (inner_value.Size() > 1 /* NUL */) {
            // Parse string to integer, use it as SubIndex.
            constexpr SizeType parsed_integer = SizeType(containers::helpers::ParseInteger< inner_value >::value);

            if constexpr (parsed_integer < sizeof...(Args)) {
                return std::tuple_cat(
                    std::make_tuple(containers::helpers::Substr< Str, 0, StringIndexStart >::value),
                    std::forward_as_tuple(std::get< parsed_integer >(std::forward_as_tuple(std::forward<Args>(args)...))),
                    FormatString_BuildTuple< containers::helpers::Substr< Str, StringIndexEnd + 1, Str.Size() - 1 >::value, Transformer, parsed_integer >{}(std::forward<Args>(args)...)
                );
            } else {
                static_assert(FormatString_BadFormat_IndexOutOfBounds< Str, inner_value >{}, "String interpolation attempted to access out of range element. Explicitly provided index value was outside of the number of arguments.");
            }
        } else if constexpr (SubIndex < sizeof...(Args)) {
            return std::tuple_cat(
                std::make_tuple(containers::helpers::Substr< Str, 0, StringIndexStart >::value),
                std::forward_as_tuple(std::get< SubIndex >(std::forward_as_tuple(std::forward<Args>(args)...))),
                FormatString_BuildTuple< containers::helpers::Substr< Str, StringIndexEnd + 1, Str.Size() - 1 >::value, Transformer, SubIndex + 1 >{}(std::forward<Args>(args)...)
            );
        } else {
            static_assert(FormatString_BadFormat_IndexOutOfBounds< Str, SubIndex >{}, "String interpolation attempted to access out of range element. Does the number of arguments match the number of replacement tokens?");

            return std::tuple { Str };
        }
    }
};

template <auto Str, class Transformer, SizeType SubIndex>
struct FormatString_BuildTuple
{
    template <class ... Args>
    constexpr auto operator()(Args &&... args) const
    {
        return FormatString_BuildTuple_Impl<
            Str,
            Transformer,
            Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString { { '{', '\0' } } > >(),
            Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString { { '}', '\0' } } > >(),
            SubIndex
        >{}(std::forward<Args>(args)...);
    }
};

#pragma endregion FormatString_BuildTuple

#pragma region FormatString_ProcessTuple

#if 0
template <class... Ts, SizeType... Indices>
constexpr auto FormatString_ProcessTuple_ProcessElements_CompileTime(const std::tuple<Ts...> &args, std::index_sequence<Indices...>)
{
    return containers::helpers::ConcatStrings(FormatString_FormatElement_CompileTime(std::get<Indices>(args))...);
}
#endif

template <int StringType, class... Ts, SizeType... Indices>
containers::detail::String<StringType> FormatString_ProcessTuple_ProcessElements_Runtime(const std::tuple<Ts...> &args, std::index_sequence<Indices...>)
{
    return ConcatRuntimeStrings<StringType>(FormatString_FormatElement_Runtime<StringType>(std::get<Indices>(args))...);
}

template <int StringType, class ... Ts>
struct FormatString_ProcessTuple_Impl
{
    using Types = std::tuple<Ts...>;

    std::tuple<Ts...>   args;

    constexpr auto operator()() const
    {
#if 0
        if constexpr (std::is_constant_evaluated())
            return FormatString_ProcessTuple_ProcessElements_CompileTime(args, std::make_index_sequence<sizeof...(Ts)>());
        else
#endif
            return FormatString_ProcessTuple_ProcessElements_Runtime<StringType>(args, std::make_index_sequence<sizeof...(Ts)>());
    }
};

template <int StringType, class ... Ts>
constexpr auto FormatString_ProcessTuple(std::tuple<Ts...> &&tup)
{
    return FormatString_ProcessTuple_Impl<StringType, Ts...> { std::move(tup) };
}

#pragma endregion FormatString_ProcessTuple

struct FormatTransformer
{
    static constexpr char delimiter = '%';
    static constexpr uint32 balance_bracket_options = containers::helpers::BALANCE_BRACKETS_NONE;

    template <auto Str>
    static constexpr auto Transform()
    {
        return Str;
    }
};

template <auto Str, class ... Args>
constexpr auto Format_Impl(Args &&... args)
{
    return (FormatString_ProcessTuple<containers::detail::StringType::UTF8>(
        FormatString_BuildTuple< Str, FormatTransformer >{}(std::forward<Args>(args)...)
    ))();
}

} // namespace detail

template <auto Str, class ... Args>
constexpr auto Format(Args &&... args)
{
    return detail::Format_Impl< Str >(std::forward<Args>(args)...);
}

} // namespace utilities

using utilities::Format;

} // namespace hyperion

// Helper macro for utilities::Format< FormatString >(...)
#define HYP_FORMAT(fmt, ...) Format<StaticString(fmt)>(__VA_ARGS__)

#endif