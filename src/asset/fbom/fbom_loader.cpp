#include "fbom.h"
#include "../byte_reader.h"
#include "marshals/entity_marshal.h"

namespace hyperion {
namespace fbom {

FBOMLoader::FBOMLoader()
    : root(new FBOMObject("ROOT")),
      m_loaders({
          { "ENTITY", new EntityMarshal() }
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
        FBOMString()
    };
}

FBOMLoader::~FBOMLoader()
{
    for (auto it : m_loaders) {
        if (it.second != nullptr) {
            delete it.second;
        }
    }

    if (root) {
        delete root;
    }
}

FBOMResult FBOMLoader::WriteToByteStream(ByteWriter *writer, FBOMLoadable *loadable) const
{
    ex_assert(writer != nullptr);

    std::string loadable_type = loadable->GetLoadableType();
    FBOMObject base(loadable_type);

    // get loader for this type
    auto it = m_loaders.find(loadable_type);

    ex_assert_msg(it != m_loaders.end(), "No loader found");

    FBOMResult result = it->second->Serialize(loadable, &base);

    if (result == FBOM_OK) {
        base.WriteToByteStream(writer);
    }

    return result;
}

std::shared_ptr<Loadable> FBOMLoader::LoadFromFile(const std::string &path)
{
    /*const char test_data[] = {
        char(FBOM_OBJECT_START),
            11, 0, 0, 0,
            'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',
            char(FBOM_DEFINE_PROPERTY),
                4, 0, 0, 0,
                'n', 'a', 'm', 'e',
                5, 0, 0, 0,
                'I', 'N', 'T', '3', '2',
                4, 0, 0, 0,
                123, 0, 0, 0,
            char(FBOM_DEFINE_PROPERTY),
                5, 0, 0, 0,
                'c', 'o', 'l', 'o', 'r',
                5, 0, 0, 0,
                'I', 'N', 'T', '6', '4',
                8, 0, 0, 0,
                12, 0, 0, 0, 0, 0, 0, 0,
            char(FBOM_DEFINE_PROPERTY),
                5, 0, 0, 0,
                'h', 'e', 'l', 'l', 'o',
                6, 0, 0, 0,
                'S', 'T', 'R', 'I', 'N', 'G',
                4, 0, 0, 0,
                'w', 'o', 'o', 't',
            char(FBOM_OBJECT_START), // subnode
                11, 0, 0, 0,
                'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',
                char(FBOM_DEFINE_PROPERTY),
                    5, 0, 0, 0,
                    'h', 'e', 'l', 'l', 'o',
                    6, 0, 0, 0,
                    'S', 'T', 'R', 'I', 'N', 'G',
                    4, 0, 0, 0,
                    'p', 'o', 'o', 't',
            char(FBOM_OBJECT_END),
        char(FBOM_OBJECT_END)
    };

    FBOMLoaderData loader;
    loader.deserialize_fn = [](FBOMObject *obj) -> std::shared_ptr<FBOMLoadable> {
        std::cout << "deserialize object\n";

        if (auto property = obj->GetProperty("hello")) {
            std::cout << "color: " << property.ToString() << "\n";
        }

        return nullptr;
    };

    m_loaders.insert_or_assign("hello world", loader);*/

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

    return root->deserialized_object;
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

void FBOMLoader::Handle(ByteReader *reader, FBOMCommand command)
{
    std::string object_type;

    switch (command) {
    case FBOM_OBJECT_START:
    {
        hard_assert(last != nullptr);

        // read string of "type" - loader to use
        object_type = ReadString(reader);

        auto it = m_loaders.find(object_type);

        if (it == m_loaders.end()) {
            throw std::runtime_error(std::string("No loader defined for ") + object_type);
        }

        last = last->AddChild(object_type);

        break;
    }
    case FBOM_OBJECT_END:
    {
        hard_assert(last != nullptr);

        // call the deserializer
        object_type = last->decl_type;

        ex_assert(last->deserialized_object == nullptr);

        FBOMLoadable *out_ptr = nullptr;
        FBOMResult deserialize_result = m_loaders.at(object_type)->Deserialize(last, out_ptr);

        last->deserialized_object.reset(out_ptr);
        last = last->parent;

        soft_assert_msg(deserialize_result == FBOM_OK, "Could not serialize object");

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
            throw std::runtime_error(std::string("Unregistered type '") + property_type + "'");
        }

        uint32_t sz;
        reader->Read(&sz);

        if (!type_it->IsUnbouned()) {
            ex_assert_msg(sz == type_it->size, "size mismatch for bounded size type");
        }

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
