#ifndef HYPERION_STATIC_STRING_H
#define HYPERION_STATIC_STRING_H

#include <Types.hpp>

#include <utility>
#include <string_view>

namespace hyperion {

template <SizeType N, class F, SizeType... Indices>
constexpr auto make_seq_helper(F f, std::index_sequence<Indices...>)
{
    return std::integer_sequence<char, (f()[Indices])...>{};
}

template <class F>
constexpr auto make_seq(F f)
{
    constexpr SizeType size = f().size();
    using SequenceType = std::make_index_sequence<size>;

    return make_seq_helper<size>(f, SequenceType { });
}

template <SizeType Size>
struct StaticString
{
    char data[Size];

    constexpr StaticString(const char (&str)[Size])
    {
        for (SizeType i = 0; i < Size; ++i) {
            data[i] = str[i];
        }
    }
};

template <auto StaticString>
struct IntegerSequenceFromString {
private:
    constexpr static auto value = make_seq([] { return std::string_view { StaticString.data }; });

public:
    using Type = decltype(value);

    static constexpr auto *data = StaticString.data;
};

template <auto StaticString>
using IntegerSequenceFromStringType = typename IntegerSequenceFromString<StaticString>::Type;

} // namespace hyperion

#endif