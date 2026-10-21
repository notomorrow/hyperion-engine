/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/Memory.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Reflection/Handle.hpp>

#include <bit>
#include <cstring>
#include <new>
#include <type_traits>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {
namespace ABI {

CORE_API void* Alloc(size_t size);
CORE_API void Free(void* ptr);

CORE_API char* AllocReturnString(const char* data, size_t size);

template <class StringType>
HYP_FORCE_INLINE char* AllocReturnString(const StringType& str)
{
    return AllocReturnString(str.Data(), str.Size());
}

/// {data, length, cap} array crossing the C ABI (C#, Strata, wasm); allocated with ABI::Alloc
template <class T>
struct ABIArray
{
    T* data;
    uint32 length;
    uint32 cap;
};

using ABIString = ABIArray<char>;

template <class T>
HYP_FORCE_INLINE void SetReturnArray(ABIArray<T>* outArray, const T* data, size_t count)
{
    T* copy = nullptr;

    if (count > 0)
    {
        copy = reinterpret_cast<T*>(Alloc(count * sizeof(T)));

        Memory::Copy(copy, data, count * sizeof(T));
    }

    outArray->data = copy;
    outArray->length = uint64(count);
    outArray->cap = uint64(count);
}

HYP_FORCE_INLINE void SetReturnString(ABIString* outString, const char* data, size_t size)
{
    outString->data = AllocReturnString(data, size);
    outString->length = uint64(size);

    // we allocate an extra byte for the NUL terminator
    outString->cap = uint64(size) + 1;
}

template <class StringType>
HYP_FORCE_INLINE void SetReturnString(ABIString* outString, const StringType& str)
{
    SetReturnString(outString, str.Data(), str.Size());
}

template <class T>
HYP_FORCE_INLINE T LoadUnaligned(const void* source)
{
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        struct Bytes
        {
            unsigned char data[sizeof(T)];
        } bytes;

        std::memcpy(bytes.data, source, sizeof(T));

        return std::bit_cast<T>(bytes);
    }
    else if constexpr (alignof(T) > alignof(void*))
    {
        // Over-aligned value types with user-defined copies need the memcpy from an aligned buffer.
        alignas(T) unsigned char storage[sizeof(T)];
        std::memcpy(storage, source, sizeof(T));

        return *std::launder(reinterpret_cast<const T*>(storage));
    }
    else
    {
        return *static_cast<const T*>(source);
    }
}

template <class T, class ValueType>
HYP_FORCE_INLINE void StoreUnaligned(void* destination, ValueType&& value)
{
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        const T temp = std::forward<ValueType>(value);

        std::memcpy(destination, &temp, sizeof(T));
    }
    else if constexpr (alignof(T) > alignof(void*))
    {
        const T temp = std::forward<ValueType>(value);

        std::memcpy(destination, &temp, sizeof(T));
    }
    else
    {
        *static_cast<T*>(destination) = std::forward<ValueType>(value);
    }
}

template <class T>
struct UnalignedRef final
{
    void* destination;
    T value;

    explicit UnalignedRef(void* destination)
        : destination(destination),
          value(LoadUnaligned<T>(destination))
    {
    }

    UnalignedRef(const UnalignedRef&) = delete;
    UnalignedRef& operator=(const UnalignedRef&) = delete;

    ~UnalignedRef()
    {
        StoreUnaligned<T>(destination, value);
    }

    HYP_FORCE_INLINE T* operator->()
    {
        return &value;
    }

    HYP_FORCE_INLINE T& Get()
    {
        return value;
    }
};

template <class T>
HYP_FORCE_INLINE T* RetainObject(T* ptr)
{
    if (ptr && !reinterpret_cast<const ObjectBase*>(ptr)->GetObjectHeader_Internal()->TryIncRefStrong())
    {
        return nullptr;
    }

    return ptr;
}

template <class T>
HYP_FORCE_INLINE T* RetainObject(const Handle<T>& handle)
{
    return RetainObject(handle.Get());
}

template <class T>
HYP_FORCE_INLINE Handle<T> ToHandle(T* ptr)
{
    return Handle<T>::FromPointer(reinterpret_cast<ObjectBase*>(ptr));
}

template <class T>
HYP_FORCE_INLINE Array<Handle<T>> ToHandleArray(const ABIArray<T*>* objects)
{
    Array<Handle<T>> result;
    result.Reserve(objects->length);

    for (uint32 i = 0; i < objects->length; i++)
    {
        result.PushBack(ToHandle(objects->data[i]));
    }

    return result;
}

template <class T, class ContainerType>
HYP_FORCE_INLINE void SetReturnObjectArray(ABIArray<T*>* outArray, const ContainerType& objects)
{
    const size_t count = objects.Size();

    T** data = count > 0 ? reinterpret_cast<T**>(Alloc(count * sizeof(T*))) : nullptr;

    for (size_t i = 0; i < count; i++)
    {
        data[i] = RetainObject(objects[i]);
    }

    outArray->data = data;
    outArray->length = uint32(count);
    outArray->cap = uint32(count);
}

template <class ContainerType>
HYP_FORCE_INLINE void SetReturnStringArray(ABIArray<ABIString>* outArray, const ContainerType& strings)
{
    const size_t count = strings.Size();

    ABIString* data = count > 0 ? reinterpret_cast<ABIString*>(Alloc(count * sizeof(ABIString))) : nullptr;

    for (size_t i = 0; i < count; i++)
    {
        SetReturnString(&data[i], strings[i]);
    }

    outArray->data = data;
    outArray->length = uint32(count);
    outArray->cap = uint32(count);
}

} // namespace ABI
} // namespace Hyperion
