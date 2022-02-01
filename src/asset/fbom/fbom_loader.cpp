#include "fbom.h"
#include "../byte_reader.h"

// marshal classes
#include "../../entity.h"
#include "../../terrain/noise_terrain/noise_terrain_control.h"
#include "../../rendering/mesh.h"

namespace hyperion {
namespace fbom {

FBOMLoader::FBOMLoader()
    : root(new FBOMObject(FBOMObjectType("ROOT"))),
      m_loaders({
          { "ENTITY", FBOM_MARSHAL_CLASS(Entity) },
          { "NOISE_TERRAIN_CONTROL", FBOM_MARSHAL_CLASS(NoiseTerrainControl) },
          { "MESH", FBOM_MARSHAL_CLASS(Mesh) }
      })
{
    last = root;

    m_registered_types = {
        FBOMUnsignedInt(),
        FBOMUnsignedLong(),
        FBOMInt(),
        FBOMLong(),
        FBOMFloat(),
        FBOMBool(),
        FBOMByte(),
        FBOMString(),
        FBOMStruct(0),
        FBOMArray()
    };

    // add array types for each type
    // for (auto it : m_registered_types) {
    //     m_registered_types.push_back(FBOMArray())
    // }
}

FBOMLoader::~FBOMLoader()
{
    if (root) {
        delete root;
    }
}

FBOMResult FBOMLoader::Deserialize(FBOMObject *in, FBOMDeserialized &out)
{
    ex_assert(in != nullptr);
    ex_assert(out == nullptr);
    ex_assert_msg(in->deserialized_object == nullptr, "Object was already deserialized");

    std::string object_type = in->m_object_type.name;

    auto it = m_loaders.find(object_type);

    if (it == m_loaders.end()) {
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("No loader for type ") + object_type);
    }

    FBOMResult deserialize_result = it->second.m_deserializer(this, in, out);

    in->deserialized_object = out;

    return deserialize_result;
}

FBOMResult FBOMLoader::Serialize(FBOMLoadable *in, FBOMObject *out)
{
    ex_assert(in != nullptr);
    ex_assert(out != nullptr);

    std::string object_type = in->GetLoadableType().name;

    auto it = m_loaders.find(object_type);

    if (it == m_loaders.end()) {
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("No loader for type ") + object_type);
    }

    FBOMResult serialize_result = it->second.m_serializer(this, in, out);

    return serialize_result;
}

FBOMResult FBOMLoader::WriteToByteStream(ByteWriter *writer, FBOMLoadable *loadable)
{
    ex_assert(writer != nullptr);

    const FBOMObjectType &loadable_type = loadable->GetLoadableType();
    FBOMObject base(loadable_type);
    FBOMResult result = Serialize(loadable, &base);

    if (result == FBOMResult::FBOM_OK) {
        base.WriteToByteStream(writer);
    }

    return result;
}

std::shared_ptr<Loadable> FBOMLoader::LoadFromFile(const std::string &path)
{
    ByteReader *reader = new FileByteReader(path);

    // TODO: file header

    // expect first FBOMObject defined

    uint8_t ins = 0;

    while (!reader->Eof()) {
        reader->Read(&ins, sizeof(uint8_t));
        Handle(reader, (FBOMCommand)ins);

        if (last == nullptr) {
            // end of file
            ex_assert_msg(reader->Eof(), "last was nullptr before eof reached");

            break;
        }
    }

    delete reader;

    ex_assert(ins == FBOM_OBJECT_END);
    hard_assert(root != nullptr);

    ex_assert_msg(root->nodes.size() == 1, "No object added to root (should be one)");
    ex_assert(root->nodes[0] != nullptr);

    return root->nodes[0]->deserialized_object;
}

std::string FBOMLoader::ReadString(ByteReader *reader)
{
    // read 4 bytes of string length
    uint32_t len;
    reader->Read(&len);

    char *bytes = new char[len + 1];
    memset(bytes, 0, len + 1);

    reader->Read(bytes, len);

    std::string str(bytes);

    delete[] bytes;

    return str;
}

FBOMObjectType FBOMLoader::ReadObjectType(ByteReader *reader)
{
    FBOMObjectType result("UNSET");

    while (true) {
        std::string object_type_name = ReadString(reader);

        result.name = object_type_name;

        // object type has unbounded size (0), so we don't write it in here, no need to read

        uint8_t extends_flag;
        reader->Read(&extends_flag);

        soft_assert_break_msg(extends_flag <= 1, "flag should be 0 or 1, higher number indicative of read error");

        if (!extends_flag) {
            break;
        }

        result = result.Extend(FBOMObjectType("UNSET"));
    }

    return result;
}

void FBOMLoader::Handle(ByteReader *reader, FBOMCommand command)
{
    std::string object_type;

    // TODO: pre-saving ObjectTypes at start, then using offset

    switch (command) {
    case FBOM_OBJECT_START:
    {
        hard_assert(last != nullptr);

        // read string of "type" - loader to use
        FBOMObjectType object_type = ReadObjectType(reader);

        auto it = m_loaders.find(object_type.name);

        if (it == m_loaders.end()) {
            throw std::runtime_error(std::string("No loader defined for ") + object_type.name);
        }

        last = last->AddChild(object_type);

        break;
    }
    case FBOM_OBJECT_END:
    {
        hard_assert(last != nullptr);

        FBOMDeserialized out = nullptr;
        FBOMResult deserialize_result = Deserialize(last, out);

        last->deserialized_object = out;

        if (deserialize_result != FBOMResult::FBOM_OK) {
            throw std::runtime_error(std::string("Could not deserialize ") + last->m_object_type.name + " object: " + deserialize_result.message);
        }

        last = last->parent;

        break;
    }
    case FBOM_DEFINE_PROPERTY:
    {
        hard_assert(last != nullptr);

        std::string property_name = ReadString(reader);
        std::string property_type = ReadString(reader);
        
        auto type_it = std::find_if(m_registered_types.begin(), m_registered_types.end(), [=](const auto &it) {
            return it.name == property_type;
        });

        if (type_it == m_registered_types.end()) {
            throw std::runtime_error(std::string("Unregistered primitive type '") + property_type + "'");
        }

        uint32_t sz;
        reader->Read(&sz);

        // if (!type_it->IsUnbouned()) {
        //     ex_assert_msg(sz == type_it->size, "size mismatch for bounded size type");
        // }

        unsigned char *bytes = new unsigned char[sz];
        reader->Read(bytes, sz);

        last->SetProperty(property_name, *type_it, sz, bytes);

        delete[] bytes;

        break;
    }
    default:
        throw std::runtime_error(std::string("Unknown command: ") + std::to_string(int(command)));
    }
}

} // namespace fbom
} // namespace hyperion
