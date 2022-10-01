#ifndef HYPERION_V2_LIB_UNIQUE_ID_HPP
#define HYPERION_V2_LIB_UNIQUE_ID_HPP

#include <random>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

namespace containers {
namespace detail {

struct UniqueIDGenerator
{
    static inline UInt64 Generate()
    {
        static thread_local std::mt19937 random_engine;
        std::uniform_int_distribution<UInt64> distribution;

        return distribution(random_engine);
    }
};

} // namespace detail
} // namespace containers

struct UniqueID
{
public:
    UniqueID()
        : value(containers::detail::UniqueIDGenerator::Generate())
    {
    }

    template <class T>
    UniqueID(const T &value)
        : value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueID(const UniqueID &other)
        : value(other.value)
    {
    }

    UniqueID &operator=(const UniqueID &other)
    {
        value = other.value;

        return *this;
    }

    UniqueID(UniqueID &&other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }

    UniqueID &operator=(UniqueID &&other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    bool operator==(const UniqueID &other) const
        { return value == other.value; }

    bool operator!=(const UniqueID &other) const
        { return value != other.value; }

    bool operator<(const UniqueID &other) const
        { return value < other.value; }

    operator UInt64() const
        { return value; }

    HashCode GetHashCode() const
        { return HashCode(value); }

private:
    UInt64 value;
};

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueID);

#endif