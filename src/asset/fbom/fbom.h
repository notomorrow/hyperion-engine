#ifndef FBOM_H
#define FBOM_H

#include "../loadable.h"
#include "../asset_loader.h"
#include "../../math/math_util.h"

#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <sstream>
#include <iomanip>
#include <memory>
#include <map>

#define FBOM_DATA_MAX_SIZE size_t(64)

#define FBOM_ASSERT(cond, message) \
    if (!(cond)) { return FBOMResult(FBOMResult::FBOM_ERR, (message)); }

#define FBOM_RETURN_OK return FBOMResult(FBOMResult::FBOM_OK)

namespace hyperion {
class ByteReader;
class ByteWriter;
namespace fbom {

using FBOMRawData_t = unsigned char[FBOM_DATA_MAX_SIZE];

struct FBOMResult {
    enum {
        FBOM_OK = 0,
        FBOM_ERR = 1
    } value;

    std::string message;

    FBOMResult(decltype(FBOM_OK) value = FBOM_OK, std::string message = "")
        : value(value),
          message(message)
    {
    }

    FBOMResult(const FBOMResult &other)
        : value(other.value),
          message(other.message)
    {
    }

    inline operator int() const { return int(value); }

    /*inline bool operator==(const FBOMResult &other)
    {
        // intentionally leave out message in comparison
        return value == other.value;
    }

    inline bool operator!=(const FBOMResult &other) { return !operator==(other); }*/
};

class FBOMObjectType;

struct FBOMType {
    std::string name;
    size_t size;
    FBOMType *extends = nullptr;

    FBOMType(const std::string &name, size_t size, const FBOMType *extends = nullptr)
        : name(name),
          size(size),
          extends(nullptr)
    {
        if (extends != nullptr) {
            this->extends = new FBOMType(*extends);
        }
    }

    FBOMType(const FBOMType &other)
        : name(other.name),
          size(other.size),
          extends(nullptr)
    {
        if (other.extends != nullptr) {
            extends = new FBOMType(*other.extends);
        }
    }

    ~FBOMType()
    {
        if (extends != nullptr) {
            delete extends;
        }
    }

    FBOMType Extend(const FBOMType &object) const;

    // inline bool IsArray() const { return IsOrExtends("ARRAY"); }
    // inline bool IsStruct() const { return IsOrExtends("STRUCT"); }

    inline FBOMType &operator=(const FBOMType &other)
    {
        if (extends != nullptr) {
            delete extends;
        }

        name = other.name;
        size = other.size;
        extends = nullptr;

        if (other.extends != nullptr) {
            extends = new FBOMType(*other.extends);
        }

        return *this;
    }

    bool IsOrExtends(const std::string &name) const
    {
        if (this->name == name) {
            return true;
        }

        if (extends == nullptr) {
            return false;
        }

        return extends->IsOrExtends(name);
    }

    bool IsOrExtends(const FBOMType &other, bool allow_unbounded = true) const
    {
        if (operator==(other)) {
            return true;
        }

        if (allow_unbounded && (IsUnbouned() || other.IsUnbouned())) {
            if (name == other.name && extends == other.extends) { // don't size check
                return true;
            }
        }

        return Extends(other);
    }

    inline bool Extends(const FBOMType &other) const
    {
        if (extends == nullptr) {
            return false;
        }

        if (*extends == other) {
            return true;
        }

        return extends->Extends(other);
    }

    inline bool IsUnbouned() const { return size == 0; }

    inline bool operator==(const FBOMType &other) const
    {
        return name == other.name
            && size == other.size
            && extends == other.extends;
    }

    std::string ToString() const
    {
        std::string str = name + " (" + std::to_string(size) + ") ";

        if (extends != nullptr) {
            str += "[" + extends->ToString() + "]";
        }

        return str;
    }
};

struct FBOMUnset : FBOMType {
    FBOMUnset()
        : FBOMType("UNSET", 0) {}
};

struct FBOMUnsignedInt : FBOMType {
    FBOMUnsignedInt()
        : FBOMType("UINT32", 4) {}
};

struct FBOMUnsignedLong : FBOMType {
    FBOMUnsignedLong()
        : FBOMType("UINT64", 8) {}
};

struct FBOMInt : FBOMType {
    FBOMInt()
        : FBOMType("INT32", 4) {}
};

struct FBOMLong : FBOMType {
    FBOMLong()
        : FBOMType("INT64", 8) {}
};

struct FBOMFloat : FBOMType {
    FBOMFloat()
        : FBOMType("FLOAT32", 4) {}
};

struct FBOMBool : FBOMType {
    FBOMBool()
        : FBOMType("BOOL", 1) {}
};

struct FBOMByte : FBOMType {
    FBOMByte()
        : FBOMType("BYTE", 1) {}
};

struct FBOMStruct : FBOMType {
    FBOMStruct(size_t sz)
        : FBOMType("STRUCT", sz) {}
};

struct FBOMArray : FBOMType {
    FBOMArray()
        : FBOMType("ARRAY", 0) {}

    FBOMArray(const FBOMType &held_type, size_t count)
        : FBOMType("ARRAY", held_type.size * count)
    {
        ex_assert_msg(!held_type.IsUnbouned(), "Cannot create array of unbounded type");
    }
};

struct FBOMString : FBOMType {
    FBOMString()
        : FBOMType("STRING", 0) {}

    FBOMString(size_t length)
        : FBOMType("STRING", length) {}
};

struct FBOMBaseObjectType : FBOMType {
    FBOMBaseObjectType()
        : FBOMType("OBJECT", 0) {}
};

struct FBOMObjectType : FBOMType {
    explicit FBOMObjectType(void)
        : FBOMObjectType("UNSET")
    {
    }

    FBOMObjectType(const std::string &name)
        : FBOMObjectType(name, FBOMBaseObjectType())
    {
    }

    FBOMObjectType(const std::string &name, FBOMType extends)
        : FBOMType(name, 0, &extends)
    {
    }
};

struct FBOMData {
    FBOMData()
        : type(FBOMUnset()),
          data_size(0),
          next(nullptr)
    {
        memset(raw_data, 0, sizeof(raw_data)); // tmp
    }

    FBOMData(const FBOMType &type)
        : type(type),
          data_size(0),
          next(nullptr)
    {
        memset(raw_data, 0, sizeof(raw_data)); // tmp
    }

    FBOMData(const FBOMData &other)
        : type(other.type),
          data_size(other.data_size)
    {
        memcpy(raw_data, other.raw_data, data_size);

        if (other.next != nullptr) {
            next = new FBOMData(*other.next);
        } else {
            next = nullptr;
        }
    }

    FBOMData &operator=(const FBOMData &other)
    {
        if (next != nullptr) {
            delete next;
            next = nullptr;
        }

        type = other.type;
        data_size = other.data_size;

        memcpy(raw_data, other.raw_data, data_size);

        if (other.next != nullptr) {
            next = new FBOMData(*other.next);
        }

        return *this;
    }

    ~FBOMData()
    {
        if (next != nullptr) {
            delete next;
        }
    }

    operator bool() const
    {
        return data_size != 0;
    }

    inline const FBOMType &GetType() const { return type; }

    size_t TotalSize() const
    {
        const FBOMData *tip = this;
        size_t sz = 0;

        while (tip) {
            sz += tip->data_size;
            tip = tip->next;
        }

        return sz;
    }

    void ReadBytes(size_t n, unsigned char *out) const
    {
        if (!type.IsUnbouned()) {
            if (n > type.size) {
                throw std::runtime_error(std::string("attempt to read past max size of object (") + type.name + ": " + std::to_string(type.size) + ") vs " + std::to_string(n));
            }
        }

        const FBOMData *tip = this;

        while (n && tip) {
            size_t to_read = MathUtil::Min(n, MathUtil::Min(FBOM_DATA_MAX_SIZE, tip->data_size));
            memcpy(out, tip->raw_data, to_read);

            n -= to_read;
            out += to_read;
            tip = tip->next;
        }
    }

    void SetBytes(size_t n, const unsigned char *data)
    {
        if (!type.IsUnbouned()) {
            ex_assert_msg(n <= type.size, "Attempt to insert data past size max size of object");
        }

        if (next) {
            delete next;
            next = nullptr;
        }

        size_t to_copy = MathUtil::Min(n, FBOM_DATA_MAX_SIZE);
        memcpy(raw_data, data, to_copy);
        data_size = to_copy;

        data += to_copy;
        n -= to_copy;

        if (n) {
            next = new FBOMData(type);
            next->SetBytes(n, data);
        }
    }

#define FBOM_TYPE_FUNCTIONS(type_name, c_type) \
    inline bool Is##type_name() const { return type == FBOM##type_name(); } \
    inline FBOMResult Read##type_name(c_type *out) const \
    { \
        FBOM_ASSERT(Is##type_name(), std::string("Type mismatch (object of type ") + type.ToString() + " was asked for " #c_type " value)"); \
        ReadBytes(FBOM##type_name().size, (unsigned char*)out); \
        FBOM_RETURN_OK; \
    }

    FBOM_TYPE_FUNCTIONS(UnsignedInt, uint32_t)
    FBOM_TYPE_FUNCTIONS(UnsignedLong, uint64_t)
    FBOM_TYPE_FUNCTIONS(Int, int32_t)
    FBOM_TYPE_FUNCTIONS(Long, int64_t)
    FBOM_TYPE_FUNCTIONS(Float, float)
    FBOM_TYPE_FUNCTIONS(Bool, bool)
    FBOM_TYPE_FUNCTIONS(Byte, int8_t)
    //FBOM_TYPE_FUNCTIONS(String, const char*)

#undef FBOM_TYPE_FUNCTIONS

    inline bool IsString() const { return type.IsOrExtends(FBOMString()); }

    inline FBOMResult ReadString(std::string &str) const
    {
        FBOM_ASSERT(IsString(), std::string("Type mismatch (object of type ") + type.ToString() + " was asked for string value)");

        size_t total_size = TotalSize();
        char *ch = new char[total_size + 1];

        ReadBytes(total_size, (unsigned char*)ch);

        ch[total_size] = '\0';

        str.assign(ch);

        delete[] ch;

        FBOM_RETURN_OK;
    }

    inline bool IsArray() const
        { return type.IsOrExtends(FBOMArray()); }

    // does NOT check that the types are exact, just that the size is a match
    inline bool IsArrayMatching(const FBOMType &held_type, size_t num_items) const
        { return type.IsOrExtends(FBOMArray(held_type, num_items)); }

    // does the array size equal byte_size bytes?
    inline bool IsArrayOfByteSize(size_t byte_size) const
        { return type.IsOrExtends(FBOMArray(FBOMByte(), byte_size)); }

    // count is number of ELEMENTS
    inline FBOMResult ReadArrayElements(const FBOMType &held_type, size_t num_items, unsigned char *out)
    {
        ex_assert(out != nullptr);

        FBOM_ASSERT(IsArray(), std::string("Type mismatch (object of type ") + type.ToString() + " was asked for array value)");

        // assert that the number of items fits evenly.
        // FBOM_ASSERT(type.size % held_type.size == 0, "Array does not evenly store held_type -- byte size mismatch");

        // assert that we are not reading over bounds (although ReadBytes accounts for this)
        // FBOM_ASSERT(held_type.size * num_items <= type.size, std::string("Attempt to read over array bounds (") + std::to_string(held_type.size * num_items) + " > " + std::to_string(type.size) + ")");

        ReadBytes(held_type.size * num_items, out);

        FBOM_RETURN_OK;
    }

    inline std::string ToString() const
    {
        std::stringstream stream;
        stream << "FBOM[";
        stream << "type: " << type.name << ", ";
        stream << "size: " << std::to_string(data_size) << ", ";
        stream << "data: { ";

        for (size_t i = 0; i < data_size; i++) {
            stream << std::hex << int(raw_data[i]) << " ";
        }

        stream << " }, next: ";

        if (next != nullptr) {
            stream << next->ToString();
        }

        stream << "]";

        return stream.str();
    }

private:
    size_t data_size;
    FBOMRawData_t raw_data;
    FBOMData *next;
    FBOMType type;
};

class FBOMObject;
class FBOMLoader;

class FBOMLoadable : public Loadable {
public:
    FBOMLoadable(const FBOMType &loadable_type)
        : m_loadable_type(loadable_type)
    {
    }

    virtual ~FBOMLoadable() = default;

    inline const FBOMType &GetLoadableType() const { return m_loadable_type; }

    virtual std::shared_ptr<Loadable> Clone() = 0;

protected:
    FBOMType m_loadable_type;
};

using FBOMDeserialized = std::shared_ptr<FBOMLoadable>;

class FBOMObject {
public:
    // std::string decl_type;
    FBOMType m_object_type;
    std::vector<std::shared_ptr<FBOMObject>> nodes;
    std::map<std::string, FBOMData> properties;
    FBOMObject *parent = nullptr;
    FBOMDeserialized deserialized_object = nullptr;

    explicit FBOMObject(void)
        : m_object_type(FBOMObjectType())
    {
    }

    FBOMObject(const FBOMType &loader_type)
        : m_object_type(loader_type)
    {

    }

    FBOMData GetProperty(const std::string &key)
    {
        auto it = properties.find(key);

        if (it == properties.end()) {
            return FBOMData(FBOMUnset());
        }

        return it->second;
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, size_t size, const void *bytes)
    {
        FBOMData data(type);
        data.SetBytes(size, reinterpret_cast<const unsigned char*>(bytes));
        properties[key] = data;
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, const void *bytes)
    {
        ex_assert_msg(!type.IsUnbouned(), "Cannot determine size of an unbounded type, please manually specify size");

        SetProperty(key, type, type.size, bytes);
    }

    FBOMObject *AddChild(const FBOMType &loader_type)
    {
        std::shared_ptr<FBOMObject> child_node = std::make_shared<FBOMObject>(loader_type);
        child_node->parent = this;

        nodes.push_back(child_node);

        return child_node.get();
    }

    void WriteToByteStream(ByteWriter *out) const;
};

enum FBOMCommand {
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_DEFINE_PROPERTY,
};

using FBOMDeserializeFunction = std::function<FBOMResult(FBOMLoader *, FBOMObject *, FBOMDeserialized &)>;
using FBOMSerializeFunction = std::function<FBOMResult(FBOMLoader *, FBOMLoadable *, FBOMObject *)>;

struct FBOMMarshal {
    FBOMMarshal(FBOMDeserializeFunction deserializer, FBOMSerializeFunction serializer)
        : m_deserializer(deserializer),
          m_serializer(serializer)
    {
    }

    FBOMDeserializeFunction m_deserializer;
    FBOMSerializeFunction m_serializer;
};

#define FBOM_DEF_DESERIALIZER(l, in, out) \
    static inline fbom::FBOMResult FBOM_Deserialize(fbom::FBOMLoader *l, fbom::FBOMObject *in, fbom::FBOMDeserialized &out)
#define FBOM_DEF_SERIALIZER(l, in, out) \
    static inline fbom::FBOMResult FBOM_Serialize(fbom::FBOMLoader *l, fbom::FBOMLoadable *in, fbom::FBOMObject *out)
#define FBOM_GET_DESERIALIZER(kls) \
    &kls::FBOM_Deserialize
#define FBOM_GET_SERIALIZER(kls) \
    &kls::FBOM_Serialize

#define FBOM_MARSHAL_CLASS(kls) \
    FBOMMarshal(FBOM_GET_DESERIALIZER(kls), FBOM_GET_SERIALIZER(kls))

class Marshal {
public:
    Marshal() = default;
    virtual ~Marshal() = default;

    virtual FBOMResult Deserialize(FBOMLoader *, FBOMObject *in, FBOMDeserialized &out) const = 0;
    virtual FBOMResult Serialize(FBOMLoader *, FBOMLoadable *in, FBOMObject *out) const = 0;
};

class FBOMLoader : public AssetLoader {
public:
    FBOMLoader();
    virtual ~FBOMLoader() override;

    FBOMResult Deserialize(FBOMObject *in, FBOMDeserialized &out);
    FBOMResult Serialize(FBOMLoadable *in, FBOMObject *out);
    FBOMResult WriteToByteStream(ByteWriter *writer, FBOMLoadable *);

    virtual std::shared_ptr<Loadable> LoadFromFile(const std::string &) override;

private:
    FBOMObject *root;
    FBOMObject *last;
    // std::map<std::string, std::unique_ptr<FBOMLoader>> loaders;
    const std::map<std::string, FBOMMarshal> m_loaders; // loader type -> loader
    std::vector<FBOMType> m_registered_types;

    std::string ReadString(ByteReader *);

    FBOMType ReadObjectType(ByteReader *);

    void Handle(ByteReader *, FBOMCommand);

    struct FBOMStaticData {
        enum {
            FBOM_STATIC_DATA_NONE = 0x00,
            FBOM_STATIC_DATA_OBJECT = 0x01,
            FBOM_STATIC_DATA_TYPE = 0x02
        } type;

        union {
            void *raw_data;
            FBOMObject object_data;
            FBOMType type_data;
        };
    };
};

class FBOMWriter {
public:
    static FBOMResult WriteToByteStream(ByteWriter *writer);
    static FBOMResult WriteObjectType(ByteWriter *writer, FBOMType type);
};

} // namespace fbom
} // namespace hyperion

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT

#endif
