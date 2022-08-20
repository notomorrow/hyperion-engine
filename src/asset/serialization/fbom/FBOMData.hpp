#ifndef HYPERION_V2_FBOM_DATA_HPP
#define HYPERION_V2_FBOM_DATA_HPP

#include <core/lib/String.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <math/MathUtil.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>

#include <cstring>

#define FBOM_ASSERT(cond, message) \
    if (!(cond)) { return FBOMResult(FBOMResult::FBOM_ERR, (message)); }

#define FBOM_RETURN_OK return FBOMResult(FBOMResult::FBOM_OK)

namespace hyperion::v2::fbom {

using FBOMRawData_t = unsigned char *;

struct FBOMData {
    static const FBOMData UNSET;

    FBOMData();
    FBOMData(const FBOMType &type);
    FBOMData(const FBOMData &other);
    FBOMData &operator=(const FBOMData &other);
    FBOMData(FBOMData &&other) noexcept;
    FBOMData &operator=(FBOMData &&other) noexcept;
    ~FBOMData();

    operator bool() const
    {
        return data_size != 0 && raw_data != nullptr;
    }

    inline const FBOMType &GetType() const { return type; }
    inline SizeType TotalSize() const { return data_size; }

    void ReadBytes(SizeType n, void *out) const;
    void SetBytes(SizeType n, const void *data);

#define FBOM_TYPE_FUNCTIONS(type_name, c_type) \
    inline bool Is##type_name() const { return type == FBOM##type_name(); } \
    inline FBOMResult Read##type_name(c_type *out) const \
    { \
        FBOM_ASSERT(Is##type_name(), "Type mismatch (expected " #type_name ")"); \
        ReadBytes(FBOM##type_name().size, out); \
        FBOM_RETURN_OK; \
    }

    FBOM_TYPE_FUNCTIONS(UnsignedInt, UInt32)
    FBOM_TYPE_FUNCTIONS(UnsignedLong, UInt64)
    FBOM_TYPE_FUNCTIONS(Int, Int32)
    FBOM_TYPE_FUNCTIONS(Long, Int64)
    FBOM_TYPE_FUNCTIONS(Float, Float32)
    FBOM_TYPE_FUNCTIONS(Bool, bool)
    FBOM_TYPE_FUNCTIONS(Byte, UByte)
    //FBOM_TYPE_FUNCTIONS(String, const char*)

#undef FBOM_TYPE_FUNCTIONS

    inline bool IsString() const { return type.IsOrExtends(FBOMString()); }

    inline FBOMResult ReadString(String &str) const
    {
        FBOM_ASSERT(IsString(), "Type mismatch (expected String)");

        const auto total_size = TotalSize();
        char *ch = new char[total_size + 1];

        ReadBytes(total_size, (unsigned char*)ch);
        ch[total_size] = '\0';

        str = ch;

        delete[] ch;

        FBOM_RETURN_OK;
    }

    inline bool IsStruct() const
        { return type.IsOrExtends(FBOMStruct(0)); }

    inline bool IsStruct(SizeType size) const
        { return type.IsOrExtends(FBOMStruct(size)); }

    inline FBOMResult ReadStruct(SizeType size, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsStruct(size), "Object is not a struct or not struct of requested size");

        ReadBytes(size, out);

        FBOM_RETURN_OK;
    }

    inline bool IsArray() const
        { return type.IsOrExtends(FBOMArray()); }

    // does NOT check that the types are exact, just that the size is a match
    inline bool IsArrayMatching(const FBOMType &held_type, SizeType num_items) const
        { return type.IsOrExtends(FBOMArray(held_type, num_items)); }

    // does the array size equal byte_size bytes?
    inline bool IsArrayOfByteSize(SizeType byte_size) const
        { return type.IsOrExtends(FBOMArray(FBOMByte(), byte_size)); }

    // count is number of ELEMENTS
    inline FBOMResult ReadArrayElements(const FBOMType &held_type, SizeType num_items, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsArray(), "Object is not an array or not array of requested size");

        // assert that the number of items fits evenly.
        // FBOM_ASSERT(type.size % held_type.size == 0, "Array does not evenly store held_type -- byte size mismatch");

        // assert that we are not reading over bounds (although ReadBytes accounts for this)
        // FBOM_ASSERT(held_type.size * num_items <= type.size, std::string("Attempt to read over array bounds (") + std::to_string(held_type.size * num_items) + " > " + std::to_string(type.size) + ")");

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

    inline std::string ToString() const
    {
        std::stringstream stream;
        stream << "FBOM[";
        stream << "type: " << type.name << ", ";
        stream << "size: " << std::to_string(data_size) << ", ";
        stream << "data: { ";

        for (SizeType i = 0; i < data_size; i++) {
            stream << std::hex << int(raw_data[i]) << " ";
        }

        stream << " } ";

        //if (next != nullptr) {
         //   stream << "<" << next->GetType().name << ">";
        //}

        stream << "]";

        return stream.str();
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(data_size);
        hc.Add(type.GetHashCode());

        for (SizeType i = 0; i < data_size; i++) {
            hc.Add(raw_data[i]);
        }

        //if (next != nullptr) {
        //    hc.Add(next->GetHashCode());
        //}

        return hc;
    }

private:
    SizeType data_size;
    FBOMRawData_t raw_data;
    //FBOMData *next;
    FBOMType type;
};

} // namespace hyperion::v2::fbom

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT

#endif
