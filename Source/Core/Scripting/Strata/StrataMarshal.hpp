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

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#if defined(HYP_ARM)
#include <arm_neon.h>
#elif defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64))
#include <immintrin.h>
#endif

namespace Hyperion {
namespace Strata {

#if defined(HYP_ARM)
using SimdVector = float32x4_t;

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec3f& value)
{
    alignas(16) float data[4] = { value.x, value.y, value.z, 0.0f };

    return vld1q_f32(data);
}

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec4f& value)
{
    alignas(16) float data[4] = { value.x, value.y, value.z, value.w };

    return vld1q_f32(data);
}

HYP_FORCE_INLINE Vec3f SimdVectorToVec3f(SimdVector value)
{
    alignas(16) float data[4];
    vst1q_f32(data, value);

    return { data[0], data[1], data[2] };
}

HYP_FORCE_INLINE Vec4f SimdVectorToVec4f(SimdVector value)
{
    alignas(16) float data[4];
    vst1q_f32(data, value);

    return { data[0], data[1], data[2], data[3] };
}

#else

using SimdVector = __m128;

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec3f& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, 0.0f);
}

HYP_FORCE_INLINE SimdVector ToSimdVector(const Vec4f& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, value.w);
}

HYP_FORCE_INLINE Vec3f SimdVectorToVec3f(SimdVector value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2] };
}

HYP_FORCE_INLINE Vec4f SimdVectorToVec4f(SimdVector value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2], data[3] };
}

#endif

#if defined(HYP_MSVC)
union SimdVector2
{
    float values[2];
    struct { float x; float y; };
};

HYP_FORCE_INLINE SimdVector2 ToSimdVector2(const Vec2f& value)
{
    SimdVector2 result;
    result.x = value.x;
    result.y = value.y;

    return result;
}

HYP_FORCE_INLINE Vec2f SimdVector2ToVec2f(SimdVector2 value)
{
    return { value.x, value.y };
}
#else
using SimdVector2 = float __attribute__((vector_size(8)));

HYP_FORCE_INLINE SimdVector2 ToSimdVector2(const Vec2f& value)
{
    return SimdVector2 { value.x, value.y };
}

HYP_FORCE_INLINE Vec2f SimdVector2ToVec2f(SimdVector2 value)
{
    return { value[0], value[1] };
}
#endif

CORE_API void* Alloc(size_t size);
CORE_API void Free(void* ptr);

CORE_API char* AllocReturnString(const char* data, size_t size);

template <class StringType>
HYP_FORCE_INLINE char* AllocReturnString(const StringType& str)
{
    return AllocReturnString(str.Data(), str.Size());
}

/// Strata Array wrapper
template <class T>
struct SArray
{
    T* data;
    uint32 length;
    uint32 cap;
};

using SString = SArray<char>;

template <class T>
HYP_FORCE_INLINE void SetReturnArray(SArray<T>* outArray, const T* data, size_t count)
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

HYP_FORCE_INLINE void SetReturnString(SString* outString, const char* data, size_t size)
{
    outString->data = AllocReturnString(data, size);
    outString->length = uint64(size);

    // we allocate an extra byte for the NUL terminator
    outString->cap = uint64(size) + 1;
}

template <class StringType>
HYP_FORCE_INLINE void SetReturnString(SString* outString, const StringType& str)
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

    HYP_FORCE_INLINE T& Get() &
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
HYP_FORCE_INLINE Array<Handle<T>> ToHandleArray(const SArray<T*>* objects)
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
HYP_FORCE_INLINE void SetReturnObjectArray(SArray<T*>* outArray, const ContainerType& objects)
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
HYP_FORCE_INLINE void SetReturnStringArray(SArray<SString>* outArray, const ContainerType& strings)
{
    const size_t count = strings.Size();

    SString* data = count > 0 ? reinterpret_cast<SString*>(Alloc(count * sizeof(SString))) : nullptr;

    for (size_t i = 0; i < count; i++)
    {
        SetReturnString(&data[i], strings[i]);
    }

    outArray->data = data;
    outArray->length = uint32(count);
    outArray->cap = uint32(count);
}

} // namespace Strata
} // namespace Hyperion
