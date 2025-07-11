/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONTAINER_BASE_HPP
#define HYPERION_CONTAINER_BASE_HPP

#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

#include <core/functional/FunctionWrapper.hpp>

#include <core/Defines.hpp>

#include <Constants.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

#include <algorithm> // for std::lower_bound, std::upper_bound

namespace hyperion {

template <class ValueType>
constexpr decltype(auto) KeyBy_Identity(const ValueType& value)
{
    return value;
}

namespace containers {

template <class Container, class Key>
class ContainerBase
{
protected:
    using Base = ContainerBase;

public:
    using KeyType = Key;

    ContainerBase()
    {
    }

    ~ContainerBase()
    {
    }

    auto& Get(KeyType key)
    {
        const auto it = static_cast<const Container*>(this)->Find(key);
        AssertThrowMsg(it != static_cast<const Container*>(this)->End(), "Cannot Get(): value not found");

        return *it;
    }

    const auto& Get(KeyType key) const
    {
        const auto it = static_cast<const Container*>(this)->Find(key);
        AssertThrowMsg(it != static_cast<const Container*>(this)->End(), "Cannot Get(): value not found");

        return *it;
    }

    auto* TryGet(KeyType key)
    {
        const auto it = static_cast<Container*>(this)->Find(key);
        return it != static_cast<Container*>(this)->End()
            ? &(*it)
            : nullptr;
    }

    const auto* TryGet(KeyType key) const
    {
        const auto it = static_cast<const Container*>(this)->Find(key);
        return it != static_cast<const Container*>(this)->End()
            ? &(*it)
            : nullptr;
    }

    template <class ValueType>
    void Set(KeyType index, const ValueType& value)
    {
        AssertThrow(index < static_cast<KeyType>(static_cast<const Container*>(this)->Size()));
        static_cast<Container*>(this)->operator[](index) = value;
    }

    template <class ValueType>
    void Set(KeyType index, ValueType&& value)
    {
        AssertThrow(index < static_cast<KeyType>(static_cast<const Container*>(this)->Size()));
        static_cast<Container*>(this)->operator[](index) = std::forward<ValueType>(value);
    }

    template <class T>
    auto Find(const T& value)
    {
        auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (*_begin == value)
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class T>
    auto Find(const T& value) const
    {
        auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (*_begin == value)
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class U>
    auto FindAs(const U& value)
    {
        auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (value == *_begin)
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class U>
    auto FindAs(const U& value) const
    {
        auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (value == *_begin)
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class Func>
    auto FindIf(Func&& pred)
    {
        FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(pred) };

        typename Container::Iterator _begin = static_cast<Container*>(this)->Begin();
        const typename Container::Iterator _end = static_cast<Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (fn(*_begin))
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class Func>
    auto FindIf(Func&& pred) const
    {
        FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(pred) };

        typename Container::ConstIterator _begin = static_cast<const Container*>(this)->Begin();
        const typename Container::ConstIterator _end = static_cast<const Container*>(this)->End();

        for (; _begin != _end; ++_begin)
        {
            if (fn(*_begin))
            {
                return _begin;
            }
        }

        return _begin;
    }

    template <class T>
    auto LowerBound(const T& key)
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key);
    }

    template <class T>
    auto LowerBound(const T& key) const
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        return std::lower_bound(
            _begin,
            _end,
            key);
    }

    template <class T>
    auto UpperBound(const T& key)
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<Container*>(this)->Begin();
        const auto _end = static_cast<Container*>(this)->End();

        return std::upper_bound(
            _begin,
            _end,
            key);
    }

    template <class T>
    auto UpperBound(const T& key) const
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        return std::upper_bound(
            _begin,
            _end,
            key);
    }

    template <class T>
    bool Contains(const T& value) const
    {
        return static_cast<const Container*>(this)->Find(value)
            != static_cast<const Container*>(this)->End();
    }

    /*! \brief Returns the number of elements matching the given value. */
    template <class T>
    SizeType Count(const T& value) const
    {
        SizeType count = 0;

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        for (auto it = _begin; it != _end; ++it)
        {
            if (*it == value)
            {
                ++count;
            }
        }

        return count;
    }

    auto Sum() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container*>(this)->Begin())>>;

        HeldType result {};
        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist)
        {
            return result;
        }

        for (auto it = _begin; it != _end; ++it)
        {
            result += static_cast<HeldType>(*it);
        }

        return result;
    }

    auto Avg() const
    {
        using HeldType = std::remove_const_t<std::remove_reference_t<decltype(*static_cast<const Container*>(this)->Begin())>>;

        HeldType result {};

        const auto _begin = static_cast<const Container*>(this)->Begin();
        const auto _end = static_cast<const Container*>(this)->End();

        const auto dist = static_cast<HeldType>(_end - _begin);

        if (!dist)
        {
            return result;
        }

        for (auto it = _begin; it != _end; ++it)
        {
            result += static_cast<HeldType>(*it);
        }

        result /= dist;

        return result;
    }

    template <class ConstIterator>
    SizeType IndexOf(ConstIterator iter) const
    {
        static_assert(Container::is_contiguous, "Container must be contiguous to perform IndexOf()");

        static_assert(std::is_convertible_v<decltype(iter),
                          typename Container::ConstIterator>,
            "Iterator type does not match container");

        return iter != static_cast<const Container*>(this)->End()
            ? SizeType(iter - static_cast<const Container*>(this)->Begin())
            : SizeType(-1);
    }

    template <class OtherContainer>
    bool CompareBitwise(const OtherContainer& other_container) const
    {
        static_assert(Container::is_contiguous && OtherContainer::is_contiguous, "Containers must be contiguous to perform bitwise comparison");

        const SizeType this_size_bytes = static_cast<const Container*>(this)->ByteSize();
        const SizeType other_size_bytes = other_container.ByteSize();

        if (this_size_bytes != other_size_bytes)
        {
            return false;
        }

        return Memory::MemCmp(
                   static_cast<const Container*>(this)->Begin(),
                   other_container.Begin(),
                   this_size_bytes)
            == 0;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (auto it = static_cast<const Container*>(this)->Begin(); it != static_cast<const Container*>(this)->End(); ++it)
        {
            hc.Add(*it);
        }

        return hc;
    }
};

} // namespace containers
} // namespace hyperion

#endif
