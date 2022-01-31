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

namespace hyperion {
class ByteReader;
class ByteWriter;
namespace fbom {

using FBOMRawData_t = unsigned char[FBOM_DATA_MAX_SIZE];

enum FBOMResult {
    FBOM_OK = 0,
    FBOM_ERR = 1
};

struct FBOMType {
    std::string name;
    size_t size;

    inline bool IsUnbouned() const { return size == 0; }

    inline bool operator==(const FBOMType &other) const
    {
        return name == other.name && size == other.size;
    }
};

struct FBOMUnset : FBOMType {
    FBOMUnset()
        : FBOMType({ "UNSET", 0 }) {}
};

struct FBOMUnsignedInt : FBOMType {
    FBOMUnsignedInt()
        : FBOMType({ "UINT32", 4 }) {}
};

struct FBOMUnsignedLong : FBOMType {
    FBOMUnsignedLong()
        : FBOMType({ "UINT64", 8 }) {}
};

struct FBOMInt : FBOMType {
    FBOMInt()
        : FBOMType({ "INT32", 4 }) {}
};

struct FBOMLong : FBOMType {
    FBOMLong()
        : FBOMType({ "INT64", 8 }) {}
};

struct FBOMFloat : FBOMType {
    FBOMFloat()
        : FBOMType({ "FLOAT32", 4 }) {}
};

struct FBOMBool : FBOMType {
    FBOMBool()
        : FBOMType({ "BOOL", 1 }) {}
};

struct FBOMByte : FBOMType {
    FBOMByte()
        : FBOMType({ "BYTE", 1 }) {}
};

struct FBOMString : FBOMType {
    FBOMString()
        : FBOMType({ "STRING", 0 }) {}
};

struct FBOMData {
    FBOMData()
        : type(FBOMUnset()),
          data_size(0),
          next(nullptr)
    {
    }

    FBOMData(const FBOMType &type)
        : type(type),
          data_size(0),
          next(nullptr)
    {
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
            sz += data_size;
            tip = tip->next;
        }

        return sz;
    }

    void ReadBytes(size_t n, unsigned char *out) const
    {
        if (!type.IsUnbouned()) {
            ex_assert_msg(n <= type.size, "Attempt to read data past size max size of object");
        }

        const FBOMData *tip = this;

        while (n && tip) {
            size_t to_read = MathUtil::Min(n, MathUtil::Min(FBOM_DATA_MAX_SIZE, data_size));
            memcpy(out, raw_data, to_read);

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
    inline c_type Read##type_name() const \
    { \
        ex_assert_msg(Is##type_name(), "Type mismatch"); \
        \
        c_type val; \
        ReadBytes(FBOM##type_name().size, (unsigned char*)&val); \
        return val; \
    }

    FBOM_TYPE_FUNCTIONS(UnsignedInt, uint32_t)
    FBOM_TYPE_FUNCTIONS(UnsignedLong, uint64_t)
    FBOM_TYPE_FUNCTIONS(Int, int32_t)
    FBOM_TYPE_FUNCTIONS(Long, int64_t)
    FBOM_TYPE_FUNCTIONS(Float, float)
    FBOM_TYPE_FUNCTIONS(Bool, bool)
    FBOM_TYPE_FUNCTIONS(Byte, int8_t)
    //FBOM_TYPE_FUNCTIONS(String, const char*)

    inline bool IsString() const { return type == FBOMString(); }

    inline void ReadString(std::string &str) const
    {
        ex_assert_msg(IsString(), "Type mismatch");

        size_t total_size = TotalSize();
        const char *ch = new char[total_size];

        ReadBytes(total_size, (unsigned char*)ch);

        str.assign(ch);

        delete[] ch;
    }

#undef FBOM_TYPE_FUNCTIONS

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

class FBOMLoadable : public Loadable {
public:
    FBOMLoadable(const std::string &loadable_type)
        : m_loadable_type(loadable_type)
    {
    }

    virtual ~FBOMLoadable() = default;

    inline const std::string &GetLoadableType() const { return m_loadable_type; }

    virtual std::shared_ptr<Loadable> Clone() = 0;

protected:
    std::string m_loadable_type;
};

class FBOMObject {
public:
    std::string decl_type;
    std::vector<std::shared_ptr<FBOMObject>> nodes;
    std::map<std::string, FBOMData> properties;
    FBOMObject *parent = nullptr;
    std::shared_ptr<FBOMLoadable> deserialized_object = nullptr;

    FBOMObject(const std::string &loader_type)
        : decl_type(loader_type)
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

    inline void SetProperty(const std::string &key, const FBOMType &type, size_t size, const unsigned char *bytes)
    {
        FBOMData data(type);
        data.SetBytes(size, bytes);
        properties[key] = data;
    }

    FBOMObject *AddChild(const std::string &loader_type)
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
    FBOM_NODE_START,
    FBOM_NODE_END,
    FBOM_DEFINE_PROPERTY,
};

// using FBOMDeserializeFunction = std::function<FBOMResult(FBOMObject*, FBOMLoadable*)>;
// using FBOMSerializeFunction = std::function<FBOMResult(FBOMLoadable*, FBOMObject*)>;

// struct FBOMLoaderData {
//     FBOMDeserializeFunction deserialize_fn;
//     FBOMSerializeFunction serialize_fn;
// };

class Marshal {
public:
    Marshal() = default;
    virtual ~Marshal() = default;

    virtual FBOMResult Deserialize(FBOMObject *in, FBOMLoadable *&out) const = 0;
    virtual FBOMResult Serialize(FBOMLoadable *in, FBOMObject *out) const = 0;
};

class FBOMLoader : public AssetLoader {
public:
    FBOMLoader();
    virtual ~FBOMLoader() override;

    FBOMResult WriteToByteStream(ByteWriter *writer, FBOMLoadable *) const;

    virtual std::shared_ptr<Loadable> LoadFromFile(const std::string &) override;

private:
    FBOMObject *root;
    FBOMObject *last;
    // std::map<std::string, std::unique_ptr<FBOMLoader>> loaders;
    const std::map<std::string, Marshal*> m_loaders; // loader type -> loader
    std::vector<FBOMType> m_registered_types;

    std::string ReadString(ByteReader *);

    void Handle(ByteReader *, FBOMCommand);
};

class FBOMWriter {
public:
    static FBOMResult WriteToByteStream(ByteWriter *writer);
};

} // namespace fbom
} // namespace hyperion

#endif
