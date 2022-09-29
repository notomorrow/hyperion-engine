#ifndef HYPERION_V2_LIB_TRAITS_H
#define HYPERION_V2_LIB_TRAITS_H

#include <type_traits>

namespace hyperion {

template <bool Copyable, bool Moveable, class Type>
struct ConstructAssignmentTraits { };

template <class Type>
struct ConstructAssignmentTraits<false, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};

} // namespace hyperion

#endif