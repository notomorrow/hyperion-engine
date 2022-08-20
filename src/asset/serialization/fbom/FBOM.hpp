#ifndef HYPERION_V2_FBOM_HPP
#define HYPERION_V2_FBOM_HPP

#include <core/lib/Optional.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Any.hpp>
#include <core/Class.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>
#include <Types.hpp>

#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <memory>
#include <map>
#include <cstring>

using std::memset;
using std::memcpy;

#include "Result.hpp"
#include "Type.hpp"
#include "BaseTypes.hpp"
#include "Data.hpp"
#include "Object.hpp"
#include "Loadable.hpp"

namespace hyperion {

namespace v2 {
namespace fbom {
class FBOMObjectType;

class FBOMObject;
class FBOMLoader;
class FBOMWriter;


enum FBOMCommand : UInt8 {
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_DEFINE_PROPERTY,
};

using FBOMDeserializeFunction = std::function<FBOMResult(FBOMLoader *, FBOMObject *, FBOMDeserialized &)>;
using FBOMSerializeFunction = std::function<FBOMResult(const FBOMWriter *, FBOMLoadable *, FBOMObject *)>;

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
#define FBOM_DEF_SERIALIZER(w, in, out) \
    static inline fbom::FBOMResult FBOM_Serialize(const fbom::FBOMWriter *w, fbom::FBOMLoadable *in, fbom::FBOMObject *out)
#define FBOM_GET_DESERIALIZER(kls) \
    &kls::FBOM_Deserialize
#define FBOM_GET_SERIALIZER(kls) \
    &kls::FBOM_Serialize

#define FBOM_MARSHAL_CLASS(kls) \
    FBOMMarshal(FBOM_GET_DESERIALIZER(kls), FBOM_GET_SERIALIZER(kls))


enum FBOMDataLocation {
    FBOM_DATA_LOCATION_NONE = 0x00,
    FBOM_DATA_LOCATION_STATIC = 0x01,
    FBOM_DATA_LOCATION_INPLACE = 0x02
};

struct FBOMStaticData {
    enum {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04
    } type;

    int64_t offset;

    // no longer a union due to destructors needed
    void *raw_data;
    FBOMObject object_data;
    FBOMType type_data;
    Optional<FBOMData> data_data;
    bool written;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          raw_data(nullptr),
          offset(-1),
          written(false)
    {
    }

    FBOMStaticData(const FBOMObject &object_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          object_data(object_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMType &type_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          type_data(type_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMData &data_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data_data(data_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          object_data(other.object_data),
          type_data(other.type_data),
          data_data(other.data_data),
          raw_data(other.raw_data),
          offset(other.offset),
          written(other.written) {}
    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        object_data = other.object_data;
        type_data = other.type_data;
        data_data = other.data_data;
        raw_data = other.raw_data;
        offset = other.offset;
        written = other.written;

        return *this;
    }

    ~FBOMStaticData() = default;

    inline HashCode GetHashCode() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.GetHashCode();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.GetHashCode();
        case FBOM_STATIC_DATA_DATA:
            return data_data.Get().GetHashCode();
        default:
            return HashCode();
        }
    }

    inline std::string ToString() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.ToString();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.ToString();
        case FBOM_STATIC_DATA_DATA:
            return data_data.Get().ToString();
        default:
            return "???";
        }
    }
};

class FBOMMarshalerBase {
    friend class FBOMLoader;

public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;

protected:
    virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const = 0;
};

template <class T>
class FBOMObjectMarshalerBase : public FBOMMarshalerBase {
public:
    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override = 0;

    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(const FBOMObject &in, T &out_object) const = 0;

private:
    virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const override
    {
        return Serialize(in.Get<T>(), out);
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const override
    {
        out.Set<T>();

        return Deserialize(in, out.Get<T>());
    }
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T> {
};

class FBOM {
    FlatMap<ANSIString, std::unique_ptr<FBOMMarshalerBase>> marshalers;

public:
    FBOM();
    FBOM(const FBOM &other) = delete;
    FBOM &operator=(const FBOM &other) = delete;
    ~FBOM();

    template <class T, class Marshaler = FBOMMarshaler<T>>
    void RegisterLoader()
    {
        static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
            "Marshaler class must be a derived class of FBOMMarshalBase");

        auto loader = std::make_unique<Marshaler>();
        const auto name = loader->GetObjectType().name.c_str();

        marshalers.Set(name, std::move(loader));
    }

    FBOMMarshalerBase *GetLoader(const ANSIString &type_name)
    {
        auto it = marshalers.Find(type_name);

        AssertThrowMsg(it != marshalers.End(), "Not a registered type!");

        return it->second.get();
    }

    static FBOM &GetInstance()
    {
        static FBOM instance;

        return instance;
    }
};

class FBOMLoader {
public:
    FBOMLoader();
    ~FBOMLoader();

    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object)
    {
        const ANSIString object_type_name(in.m_object_type.name.c_str());
        const auto *loader = FBOM::GetInstance().GetLoader(object_type_name);

        if (!loader) {
            return FBOMResult(FBOMResult::FBOM_ERR, "Loader not registered for type");
        }

        return loader->Deserialize(in, out_object);
    }

    // template <class T, class Marshaler = FBOMMarshaler<T>>
    // FBOMResult Deserialize(const FBOMObject &in, T &out_object)
    // {
    //     const ANSIString object_type_name(in.m_object_type.name.c_str());
    //     const auto *loader = FBOM::GetInstance().GetLoader(object_type_name);

    //     if (!loader) {
    //         return FBOMResult(FBOMResult::FBOM_ERR, "Loader not registered for type");
    //     }

    //     return loader->Deserialize(in, static_cast<void *>(&out_object));
    // }

    FBOMResult LoadFromFile(const std::string &path, FBOMDeserializedObject &out)
    {
        // Include our root dir as part of the path
        std::string root_dir = FileSystem::CurrentPath();
        ByteReader *reader = new FileByteReader(FileSystem::Join(root_dir, path));

        m_static_data_pool.clear();
        m_in_static_data = false;

        // expect first FBOMObject defined
        FBOMCommand command = FBOM_NONE;

        while (!reader->Eof()) {
            command = PeekCommand(reader);

            if (auto err = Handle(reader, command, root)) {
                throw std::runtime_error(err.message);
            }

            if (last == nullptr) {
                // end of file
                AssertThrowMsg(reader->Eof(), "last was nullptr before eof reached");

                break;
            }
        }

        delete reader;

        AssertThrow(root != nullptr);

        AssertThrowMsg(root->nodes.size() == 1, "No object added to root (should be one)");

        return Deserialize(root->nodes[0], out);
    }

private:
    FBOMCommand NextCommand(ByteReader *);
    FBOMCommand PeekCommand(ByteReader *);
    FBOMResult Eat(ByteReader *, FBOMCommand, bool read = true);

    std::string ReadString(ByteReader *);
    FBOMType ReadObjectType(ByteReader *);
    FBOMResult ReadData(ByteReader *, FBOMData &data);
    FBOMResult ReadObject(ByteReader *, FBOMObject &object);

    FBOMResult Handle(ByteReader *, FBOMCommand, FBOMObject *parent);

    FBOMObject *root;
    FBOMObject *last;

    std::vector<FBOMType> m_registered_types;

    bool m_in_static_data;
    std::vector<FBOMStaticData> m_static_data_pool;
};

template <class T, class Marshaler = FBOMMarshaler<T>>
static inline FBOMResult SerializeObject(const T &object, FBOMObject &out)
{
    static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
        "Marshaler class must be a derived class of FBOMMarshalBase");

    return Marshaler().Serialize(object, out);
}

template <class T, class Marshaler = FBOMMarshaler<T>>
static inline FBOMResult DeserializeObject(const FBOMObject &in, T &object)
{
    static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
        "Marshaler class must be a derived class of FBOMMarshalBase");

    return Marshaler().Deserialize(in, object);
}

template <class T, class Marshaler = FBOMMarshaler<T>>
static inline FBOMType GetObjectType()
{
    static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
        "Marshaler class must be a derived class of FBOMMarshalBase");

    return Marshaler().GetObjectType();
}

class FBOMWriter {
public:
    FBOMWriter();
    ~FBOMWriter();

    template <class T, class Marshaler = FBOMMarshaler<T>>
    FBOMResult Append(T &object)
    {
        if (auto err = m_write_stream.m_last_result) {
            return err;
        }

        FBOMObject base(GetObjectType<T, Marshaler>());

        if (auto err = SerializeObject(object, base)) {
            m_write_stream.m_last_result = err;

            return err;
        }

        return Append(std::move(base));
    }

    FBOMResult Append(FBOMObject &&object);
    FBOMResult Emit(ByteWriter *out);

private:
    void BuildStaticData();
    void Prune(FBOMObject &);
    FBOMResult WriteStaticDataToByteStream(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &);
    FBOMResult WriteData(ByteWriter *out, const FBOMData &);
    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &);
    void AddStaticData(const FBOMType &);
    void AddStaticData(const FBOMObject &);
    void AddStaticData(const FBOMData &);

    struct WriteStream {
        std::map<HashCode::ValueType, FBOMStaticData> m_static_data; // map hashcodes to static data to be stored.
        std::unordered_map<HashCode::ValueType, int> m_hash_use_count_map;
        std::vector<FBOMObject> m_object_data; // TODO: make multiple objects be supported by the loader.
        size_t m_static_data_offset = 0;
        FBOMResult m_last_result = FBOMResult::FBOM_OK;

        FBOMDataLocation GetDataLocation(HashCode::ValueType, FBOMStaticData &out) const;
        void MarkStaticDataWritten(HashCode::ValueType);
    } m_write_stream;
};

} // namespace fbom
} // namespace v2
} // namespace hyperion

#endif
