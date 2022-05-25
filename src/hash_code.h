#ifndef HASH_CODE_H
#define HASH_CODE_H

#include <type_traits>
#include <functional>
#include <string>

namespace hyperion {
using HashCode_t = size_t;

struct HashCode {
    using Value_t = HashCode_t;

    HashCode()
        : hash(0)
    {
    }

    HashCode(const HashCode &other)
        : hash(other.hash)
    {
    }

    constexpr bool operator==(const HashCode &other) const { return hash == other.hash; }

    template<class T>
    typename std::enable_if_t<std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>>
    Add(const T &hash_code)
    {
        HashCombine(hash_code.Value());
    }

    template<class T>
    typename std::enable_if_t<std::is_fundamental_v<T> || std::is_pointer_v<T> || std::is_same_v<T, std::string>>
    Add(const T &value)
    {
        HashCombine(std::hash<T>()(value));
    }

    constexpr Value_t Value() const
        { return hash; }

private:
    Value_t hash;

    inline constexpr void HashCombine(Value_t other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

#endif
