#ifndef HYPERION_V2_LIB_PAIR_H
#define HYPERION_V2_LIB_PAIR_H

#include <system/Debug.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <utility>

namespace hyperion {

template <class First, class Second>
struct Pair {
    First  first;
    Second second;

    Pair()
        : first{},
          second{}
    {
    }

    Pair(First &&first, Second &&second)
        : first(std::forward<First>(first)),
          second(std::forward<Second>(second))
    {
    }

    Pair(const Pair &other)
        : first(other.first),
          second(other.second)
    {
    }

    Pair &operator=(const Pair &other)
    {
        first  = other.first;
        second = other.second;

        return *this;
    }

    Pair(Pair &&other) noexcept
        : first(std::move(other.first)),
          second(std::move(other.second))
    {
    }

    Pair &operator=(Pair &&other) noexcept
    {
        first  = std::move(other.first);
        second = std::move(other.second);

        return *this;
    }

    ~Pair() = default;

    bool operator<(const Pair &other) const
    {
        return first < other.first || (!(other.first < first) && second < other.second);
    }

    bool operator==(const Pair &other) const
    {
        return first == other.first
            && second == other.second;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

} // namespace hyperion

#endif