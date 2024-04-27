/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_DATA_HPP
#define HYPERION_FBOM_DATA_HPP

#include <core/Name.hpp>
#include <core/containers/String.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <math/MathUtil.hpp>
#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix3.hpp>
#include <math/Matrix4.hpp>
#include <math/Quaternion.hpp>
#include <core/system/Debug.hpp>

#include <Types.hpp>

#include <cstring>
#include <sstream>

#define FBOM_ASSERT(cond, message) \
    if (!(cond)) { return FBOMResult(FBOMResult::FBOM_ERR, (message)); }

#define FBOM_RETURN_OK return FBOMResult(FBOMResult::FBOM_OK)

namespace hyperion::fbom {

struct FBOMData
{
    static const FBOMData UNSET;

    FBOMData();
    FBOMData(const FBOMType &type);
    FBOMData(const FBOMType &type, ByteBuffer &&byte_buffer);
    FBOMData(const FBOMData &other);
    FBOMData &operator=(const FBOMData &other);
    FBOMData(FBOMData &&other) noexcept;
    FBOMData &operator=(FBOMData &&other) noexcept;
    ~FBOMData();

    explicit operator bool() const
        { return bytes.Any(); }

    const FBOMType &GetType() const { return type; }
    SizeType TotalSize() const { return bytes.Size(); }

    /*! \returns The number of bytes read */
    SizeType ReadBytes(SizeType n, void *out) const;
    ByteBuffer ReadBytes() const;
    ByteBuffer ReadBytes(SizeType n) const;

    void SetBytes(SizeType n, const void *data);

#define FBOM_TYPE_FUNCTIONS(type_name, c_type) \
    bool Is##type_name() const { return type == FBOM##type_name(); } \
    FBOMResult Read##type_name(c_type *out) const \
    { \
        FBOM_ASSERT(Is##type_name(), "Type mismatch (expected " #c_type ")"); \
        ReadBytes(FBOM##type_name().size, out); \
        FBOM_RETURN_OK; \
    } \
    /*! \brief Read with static_cast to result type */ \
    template <class T> \
    FBOMResult Read##type_name(T *out) const \
    { \
        static_assert(sizeof(T) == sizeof(c_type), "sizeof(T) must match sizeof " #c_type "!"); \
        if constexpr (std::is_enum_v<T>) { \
            static_assert(std::is_same_v<std::underlying_type_t<T>, c_type>, "Underlying type of enum T must be " #c_type "!"); \
        } else { \
            static_assert(std::is_convertible_v<c_type, T>, "T must be convertible to " #c_type "!"); \
        } \
        c_type read_value; \
        FBOM_ASSERT(Is##type_name(), "Type mismatch (expected " #c_type ")"); \
        ReadBytes(FBOM##type_name().size, &read_value); \
        *out = static_cast<T>(read_value); \
        FBOM_RETURN_OK; \
    } \
    static FBOMData From##type_name(const c_type &value) \
    { \
        static_assert(std::is_standard_layout_v<c_type>, "Type " #c_type " must be standard layout"); \
        \
        FBOM##type_name type; \
        AssertThrowMsg(sizeof(c_type) == type.size, "sizeof(" #c_type ") must be equal to FBOM" #type_name "::size"); \
        \
        FBOMData data(type); \
        data.SetBytes(sizeof(c_type), &value); \
        return data; \
    } \
    template <SizeType Sz> \
    static FBOMData FromArray(const FixedArray<c_type, Sz> &items) \
    { \
        FBOMSequence type(FBOM##type_name(), items.Size()); \
        FBOMData data(type); \
        data.SetBytes(sizeof(c_type) * items.Size(), items.Data()); \
        return data; \
    } \
    template <SizeType Sz> \
    static FBOMData FromArray(const c_type (&items)[Sz]) \
    { \
        FBOMSequence type(FBOM##type_name(), Sz); \
        FBOMData data(type); \
        data.SetBytes(sizeof(c_type) * Sz, items); \
        return data; \
    } \
    static FBOMData FromArray(const Array<c_type> &items) \
    { \
        FBOMSequence type(FBOM##type_name(), items.Size()); \
        AssertThrowMsg(sizeof(c_type) * items.Size() == type.size, "sizeof(" #c_type ") * %llu must be equal to %llu", items.Size(), type.size); \
        FBOMData data(type); \
        data.SetBytes(sizeof(c_type) * items.Size(), items.Data()); \
        return data; \
    }

    FBOM_TYPE_FUNCTIONS(UnsignedInt, uint32)
    FBOM_TYPE_FUNCTIONS(UnsignedLong, uint64)
    FBOM_TYPE_FUNCTIONS(Int, int32)
    FBOM_TYPE_FUNCTIONS(Long, int64)
    FBOM_TYPE_FUNCTIONS(Float, float)
    FBOM_TYPE_FUNCTIONS(Bool, bool)
    FBOM_TYPE_FUNCTIONS(Byte, ubyte)
    FBOM_TYPE_FUNCTIONS(Name, Name)
    FBOM_TYPE_FUNCTIONS(Mat3, Matrix3)
    FBOM_TYPE_FUNCTIONS(Mat4, Matrix4)
    FBOM_TYPE_FUNCTIONS(Vec2f, Vector2)
    FBOM_TYPE_FUNCTIONS(Vec3f, Vector3)
    FBOM_TYPE_FUNCTIONS(Vec4f, Vector4)
    FBOM_TYPE_FUNCTIONS(Quaternion, Quaternion)

#undef FBOM_TYPE_FUNCTIONS

    bool IsString() const { return type.IsOrExtends(FBOMString()); }

    template <int string_type>
    FBOMResult ReadString(containers::detail::String<string_type> &str) const
    {
        static_assert(string_type == int(StringType::ANSI) || string_type == int(StringType::UTF8), "String type must be ANSI or UTF8");

        FBOM_ASSERT(IsString(), "Type mismatch (expected String)");

        const auto total_size = TotalSize();
        char *ch = new char[total_size + 1];

        ReadBytes(total_size, ch);
        ch[total_size] = '\0';

        str = ch;

        delete[] ch;

        FBOM_RETURN_OK;
    }
    
    template <int string_type>
    static FBOMData FromString(const containers::detail::String<string_type> &str)
    {
        static_assert(string_type == int(StringType::ANSI) || string_type == int(StringType::UTF8), "String type must be ANSI or UTF8");

        FBOMData data(FBOMString(str.Size()));
        data.SetBytes(str.Size(), str.Data());

        return data;
    }

    bool IsByteBuffer() const
        { return type.IsOrExtends(FBOMByteBuffer()); }

    FBOMResult ReadByteBuffer(ByteBuffer &byte_buffer) const
    {
        FBOM_ASSERT(IsByteBuffer(), "Type mismatch (expected ByteBuffer)");

        byte_buffer = bytes;

        FBOM_RETURN_OK;
    }
    
    static FBOMData FromByteBuffer(const ByteBuffer &byte_buffer)
    {
        FBOMData data(FBOMByteBuffer(byte_buffer.Size()));
        data.SetBytes(byte_buffer.Size(), byte_buffer.Data());

        return data;
    }

    bool IsStruct(const char *type_name) const
        { return type.IsOrExtends(FBOMStruct(type_name, 0)); }

    bool IsStruct(const char *type_name, SizeType size) const
        { return type.IsOrExtends(FBOMStruct(type_name, size)); }

    FBOMResult ReadStruct(const char *type_name, SizeType size, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsStruct(type_name, size), "Object is not a struct or not struct of requested size");

        ReadBytes(size, out);

        FBOM_RETURN_OK;
    }

    template <class T>
    FBOMResult ReadStruct(T *out) const
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout to use ReadStruct()");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable to use ReadStruct()");

        return ReadStruct(TypeNameWithoutNamespace<T>().Data(), sizeof(T), out);
    }

    bool IsSequence() const
        { return type.IsOrExtends(FBOMSequence()); }

    // does NOT check that the types are exact, just that the size is a match
    bool IsSequenceMatching(const FBOMType &held_type, SizeType num_items) const
        { return type.IsOrExtends(FBOMSequence(held_type, num_items)); }

    // does the array size equal byte_size bytes?
    bool IsSequenceOfByteSize(SizeType byte_size) const
        { return type.IsOrExtends(FBOMSequence(FBOMByte(), byte_size)); }

    /*! \brief If type is an sequence, return the number of elements,
        assuming the sequence contains the given type. Note, sequence could
        contain another type, and still a result will be returned.
        
        If type is /not/ an sequence, return zero. */
    SizeType NumElements(const FBOMType &held_type) const
    {
        if (!IsSequence()) {
            return 0;
        }

        const auto held_type_size = held_type.size;
        
        if (held_type_size == 0) {
            return 0;
        }

        return TotalSize() / held_type_size;
    }

    FBOMResult ReadBytes(SizeType count, ByteBuffer &out) const
    {
        FBOM_ASSERT(count <= bytes.Size(), "Attempt to read past max size of object");

        out = ByteBuffer(count, bytes.Data());

        FBOM_RETURN_OK;
    }

    // count is number of ELEMENTS
    FBOMResult ReadElements(const FBOMType &held_type, SizeType num_items, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsSequence(), "Object is not an sequence");

        ReadBytes(held_type.size * num_items, out);

        FBOM_RETURN_OK;
    }

    FBOMResult ReadAsType(const FBOMType &read_type, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(type.IsOrExtends(read_type), "Type mismatch");

        ReadBytes(read_type.size, out);

        FBOM_RETURN_OK;
    }

    String ToString() const
    {
        std::stringstream stream;
        stream << "FBOM[";
        stream << "type: " << type.name << ", ";
        stream << "size: " << String::ToString(bytes.Size()) << ", ";
        stream << "data: { ";

        for (SizeType i = 0; i < bytes.Size(); i++) {
            stream << std::hex << int(bytes[i]) << " ";
        }

        stream << " } ";

        //if (next != nullptr) {
         //   stream << "<" << next->GetType().name << ">";
        //}

        stream << "]";

        return String(stream.str().c_str());
    }

    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes.Size());
        hc.Add(type);
        hc.Add(bytes);

        return hc;
    }

private:
    ByteBuffer bytes;
    FBOMType type;
};

} // namespace hyperion::fbom

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT

#endif
