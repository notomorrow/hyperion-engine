#ifndef HYPERION_TUPLE_HPP
#define HYPERION_TUPLE_HPP

#include <Types.hpp>

namespace hyperion {
namespace utilities {

namespace detail {

// see: https://mitchnull.blogspot.com/2012/06/c11-tuple-implementation-details-part-1.html?m=1

#pragma region TupleIndices

template <SizeType... Index>
struct TupleIndices { };

template <SizeType Start, class TupleIndicesType, SizeType End>
struct MakeTupleIndices_Impl;

template <SizeType Start, SizeType... Indices, SizeType End>
struct MakeTupleIndices_Impl< Start, TupleIndices< Indices... >, End >
{
    using Type = typename MakeTupleIndices_Impl< Start + 1,  TupleIndices< Indices..., Start >, End >::Type;
};

template <SizeType End, SizeType... Indices>
struct MakeTupleIndices_Impl< End, TupleIndices< Indices... >, End >
{
    using Type = TupleIndices< Indices... >;
};

template <SizeType End, SizeType Start = 0>
struct MakeTupleIndices
{
    static_assert(Start <= End);

    using Type = typename MakeTupleIndices_Impl< Start, TupleIndices< >, End >::Type;
};

#pragma endregion TupleIndices

#pragma region TupleLeaf

template <SizeType Index, class T>
struct TupleLeaf
{
    using ElementType = T;

    ElementType value;

    constexpr TupleLeaf() = default;
    
    constexpr TupleLeaf(const T &value)
        : value(value)
    {
    }

    constexpr TupleLeaf(T &&value)
        : value(std::move(value))
    {
    }
};

#pragma endregion TupleLeaf

#pragma region TupleElement

template <SizeType Index, SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl;


template <SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl< Depth, Depth, FirstType, Types... >
{
    using Type = FirstType;
};

template <SizeType Index, SizeType Depth, class FirstType, class... Types>
struct TupleElement_SelectType_Impl
{
    using Type = typename TupleElement_SelectType_Impl< Index, Depth + 1, Types... >::Type;
};

template <SizeType Index, class... Types>
struct TupleElement
{
    using Type = typename TupleElement_SelectType_Impl< Index, 0, Types... >::Type; 
};

#pragma endregion TupleElement

#pragma region Tuple_Impl

template <class TupleIndicesType, class... Types>
struct Tuple_Impl;

template <SizeType... Indices, class... Types>
struct Tuple_Impl< TupleIndices< Indices ... >, Types... > : TupleLeaf< Indices, Types >...
{
    constexpr Tuple_Impl() = default;

    template <SizeType... Indices_, class... Types_>
    constexpr Tuple_Impl(std::integer_sequence< SizeType, Indices_... >, Types_ &&... values)
        : TupleLeaf< Indices_, Types >(std::forward< Types_ >(values))...
    {
    }
};

#pragma endregion Tuple_Impl

} // namespace detail

template <class... Types>
class Tuple
{
    template <std::size_t Index_, class... Types_>
    friend constexpr typename detail::TupleElement< Index_, Types_... >::Type &get(Tuple< Types_... > &) noexcept;

    template <std::size_t Index_, class... Types_>
    friend constexpr const typename detail::TupleElement< Index_, Types_... >::Type &get(const Tuple< Types_... > &) noexcept;
    
    template <std::size_t Index_, class... Types_>
    friend constexpr typename detail::TupleElement< Index_, Types_... >::Type &&get(Tuple< Types_... > &&) noexcept;

    template <std::size_t Index_, class... Types_>
    friend constexpr const typename detail::TupleElement< Index_, Types_... >::Type &&get(const Tuple< Types_... > &&) noexcept;

    detail::Tuple_Impl< typename detail::MakeTupleIndices< sizeof...(Types) >::Type, Types... > impl;

public:
    constexpr Tuple() = default;

    constexpr Tuple(const Types &... values)
        : impl(std::index_sequence_for< Types... >(), values...)
    {
    }

    constexpr Tuple(Types &&... values)
        : impl(std::index_sequence_for< Types... >(), std::move(values)...)
    {
    }

    template <SizeType Index>
    constexpr typename detail::TupleElement< Index, Types... >::Type &GetElement() &
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast< detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &>(impl).value;
    }

    template <SizeType Index>
    constexpr const typename detail::TupleElement< Index, Types... >::Type &GetElement() const &
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast< const detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &>(impl).value;
    }

    template <SizeType Index>
    constexpr typename detail::TupleElement< Index, Types... >::Type &&GetElement() &&
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast< detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &&>(impl).value;
    }

    template <SizeType Index>
    constexpr const typename detail::TupleElement< Index, Types... >::Type &&GetElement() const &&
    {
        static_assert(Index < sizeof...(Types), "Index out of range of tuple");

        return static_cast< const detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &&>(impl).value;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return sizeof...(Types); }
};

namespace helpers {

#pragma region TupleSize

template <class TupleType>
struct TupleSize;

template <class... Types>
struct TupleSize< Tuple< Types... > >
{
    static constexpr SizeType value = sizeof...(Types);
};

#pragma endregion TupleSize

#pragma region ConcatTuples

namespace detail {

template <class... FirstTypes, SizeType... FirstIndices, class... SecondTypes, SizeType... SecondIndices>
constexpr auto ConcatTuples_Impl(
    const Tuple< FirstTypes... > &first_tuple,
    hyperion::utilities::detail::TupleIndices< FirstIndices... >,
    const Tuple< SecondTypes... > &second_tuple,
    hyperion::utilities::detail::TupleIndices< SecondIndices... >
)
{
    return Tuple< FirstTypes..., SecondTypes...> {
        first_tuple.template GetElement< FirstIndices >()...,
        second_tuple.template GetElement< SecondIndices >()...
    };
}

} // namespace detail


template <class... FirstTupleTypes, class... SecondTupleTypes>
constexpr auto ConcatTuples(const Tuple< FirstTupleTypes... > &first, const Tuple< SecondTupleTypes... > &second) -> Tuple< FirstTupleTypes..., SecondTupleTypes... >
{
    return detail::ConcatTuples_Impl(
        first,
        typename hyperion::utilities::detail::MakeTupleIndices< sizeof...(FirstTupleTypes) >::Type(),
        second,
        typename hyperion::utilities::detail::MakeTupleIndices< sizeof...(SecondTupleTypes) >::Type()
    );
}

#pragma endregion ConcatTuples

} // namespace helpers

template <std::size_t Index, class... Types>
constexpr typename detail::TupleElement< Index, Types... >::Type &get(Tuple< Types... > &tup) noexcept
{
    return static_cast< detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &>(tup.impl).value;
}

template <std::size_t Index, class... Types>
constexpr const typename detail::TupleElement< Index, Types... >::Type &get(const Tuple< Types... > &tup) noexcept
{
    return static_cast< const detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &>(tup.impl).value;
}

template <std::size_t Index, class... Types>
constexpr typename detail::TupleElement< Index, Types... >::Type &&get(Tuple< Types... > &&tup) noexcept
{
    return static_cast< detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &&>(tup.impl).value;
}

template <std::size_t Index, class... Types>
constexpr const typename detail::TupleElement< Index, Types... >::Type &&get(const Tuple< Types... > &&tup) noexcept
{
    return static_cast< const detail::TupleLeaf< Index, typename detail::TupleElement< Index, Types... >::Type > &&>(tup.impl).value;
}

} // namespace utilities

using utilities::Tuple;

} // namespace hyperion

// std::tuple-like behavior
namespace std {

template <class... Types>
struct tuple_size< hyperion::utilities::Tuple< Types... > >
{
    static constexpr size_t value = sizeof...(Types);
};

template <size_t Index, class... Types>
struct tuple_element< Index, hyperion::utilities::Tuple< Types... > >
{
    using type = typename hyperion::utilities::detail::TupleElement< Index, Types... >::Type;
};

} // namespace std

#endif