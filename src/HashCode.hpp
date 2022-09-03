#ifndef HASH_CODE_H
#define HASH_CODE_H

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <functional>
#include <string>

namespace hyperion {

template <class T, class HashCode>
struct HasGetHashCode
{
    template <class U, HashCode(U::*)() const>
    struct Resolve {};

    template <class U>
    static char Test(Resolve<U, &U::GetHashCode> *);

    template <class U>
    static int Test(...);

    static constexpr bool value = sizeof(Test<T>(0)) == sizeof(char);
};

struct HashCode
{
    using ValueType = UInt64;

    HashCode()
        : hash(0)
    {
    }

    HashCode(const HashCode &other)
        : hash(other.hash)
    {
    }

    constexpr bool operator==(const HashCode &other) const { return hash == other.hash; }
    constexpr bool operator!=(const HashCode &other) const { return hash != other.hash; }

    template<class T>
    typename std::enable_if_t<std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>>
    Add(const T &hash_code) { HashCombine(hash_code.Value()); }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HasGetHashCode<DecayedType, HashCode>::value && implementation_exists<std::hash<DecayedType>>>
    Add(const T &value) { HashCombine(std::hash<DecayedType>()(value)); }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && HasGetHashCode<DecayedType, HashCode>::value>
    Add(const T &value) { HashCombine(value.GetHashCode().Value()); }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HasGetHashCode<DecayedType, HashCode>::value && !implementation_exists<std::hash<DecayedType>>>
    Add(const T &value) { static_assert(resolution_failure<T>, "No GetHashCode() method or std::hash<T> implementation"); }

    constexpr ValueType Value() const { return hash; }

    template <class T>
    static inline HashCode GetHashCode(const T &value)
    {
        HashCode hc;
        hc.Add(value);
        return hc;
    }

private:
    ValueType hash;

    constexpr void HashCombine(ValueType other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

#endif
