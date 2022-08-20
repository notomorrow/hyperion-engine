#include "FBOM.hpp"

namespace hyperion::v2::fbom {

FBOMLoader::FBOMLoader()
    : root(new FBOMObject(FBOMObjectType("ROOT"))),
      m_in_static_data(false)
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
}

FBOMLoader::~FBOMLoader()
{
    if (root) {
        delete root;
    }
}

FBOMCommand FBOMLoader::NextCommand(ByteReader *reader)
{
    AssertThrow(!reader->Eof());

    UInt8 ins = 0;
    reader->Read(&ins);

    return FBOMCommand(ins);
}

FBOMCommand FBOMLoader::PeekCommand(ByteReader *reader)
{
    AssertThrow(!reader->Eof());

    UInt8 ins = 0;
    reader->Peek(&ins);

    return FBOMCommand(ins);
}

FBOMResult FBOMLoader::Eat(ByteReader *reader, FBOMCommand command, bool read)
{
    FBOMCommand received;

    if (read) {
        received = NextCommand(reader);
    } else {
        received = PeekCommand(reader);
    }

    if (received != command) {
        return FBOMResult(FBOMResult::FBOM_ERR, std::string("unexpected command: expected ") + std::to_string(command) + ", received " + std::to_string(received));
    }

    return FBOMResult::FBOM_OK;
}

std::string FBOMLoader::ReadString(ByteReader *reader)
{
    // read 4 bytes of string length
    UInt32 len;
    reader->Read(&len);

    char *bytes = new char[len + 1];
    memset(bytes, 0, len + 1);

    reader->Read(bytes, len);

    std::string str(bytes);

    delete[] bytes;

    return str;
}

FBOMType FBOMLoader::ReadObjectType(ByteReader *reader)
{
    FBOMType result = FBOMUnset();

    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        UInt8 extend_level;
        reader->Read(&extend_level);

        for (int i = 0; i < extend_level; i++) {
            std::string type_name = ReadString(reader);
            result.name = type_name;

            // read size of object
            UInt64 type_size;
            reader->Read(&type_size);
            result.size = type_size;

            if (i == extend_level - 1) {
                break;
            }

            result = result.Extend(FBOMUnset());
        }
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as u32
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_TYPE);
        result = m_static_data_pool.at(offset).type_data;
    }

    return result;
}

FBOMResult FBOMLoader::ReadData(ByteReader *reader, FBOMData &data)
{
    // read data location
    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        FBOMType object_type = ReadObjectType(reader);

        UInt32 sz;
        reader->Read(&sz);

        unsigned char *bytes = new unsigned char[sz];
        reader->Read(bytes, sz);

        data = FBOMData(object_type);
        data.SetBytes(sz, bytes);

        delete[] bytes;
    } else if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as u32
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_DATA);
        data = m_static_data_pool.at(offset).data_data.Get();
    }

    return FBOMResult::FBOM_OK;
}


FBOMResult FBOMLoader::ReadObject(ByteReader *reader, FBOMObject &object)
{
    if (auto err = Eat(reader, FBOM_OBJECT_START)) {
        return err;
    }

    FBOMCommand command = FBOM_NONE;

    // read data location
    UInt8 object_type_location = FBOM_DATA_LOCATION_NONE;
    reader->Read(&object_type_location);

    if (object_type_location == FBOM_DATA_LOCATION_STATIC) {
        // read offset as u32
        UInt32 offset;
        reader->Read(&offset);

        // grab from static data pool
        AssertThrow(m_static_data_pool.at(offset).type == FBOMStaticData::FBOM_STATIC_DATA_OBJECT);
        object = m_static_data_pool.at(offset).object_data;

        return FBOMResult::FBOM_OK;
    }

    if (object_type_location == FBOM_DATA_LOCATION_INPLACE) {
        // read string of "type" - loader to use
        const FBOMType object_type = ReadObjectType(reader);

        object = FBOMObject(object_type);

        do {
            command = PeekCommand(reader);

            switch (command) {
            case FBOM_OBJECT_START:
            {
                FBOMObject child;

                if (auto err = ReadObject(reader, child)) {
                    return err;
                }

                object.nodes.push_back(child);

                break;
            }
            case FBOM_OBJECT_END:
                if (auto err = Deserialize(object, object.deserialized)) {
                    return FBOMResult(FBOMResult::FBOM_ERR, std::string("Read object: could not deserialize ") + object.m_object_type.name + " object: " + err.message);
                }

                break;
            case FBOM_DEFINE_PROPERTY:
            {
                if (auto err = Eat(reader, FBOM_DEFINE_PROPERTY)) {
                    return err;
                }

                std::string property_name = ReadString(reader);

                FBOMData data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                object.SetProperty(property_name, data);

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("Read object: cannot process command ") + std::to_string(command) + " while reading object");
            }
        } while (command != FBOM_OBJECT_END && command != FBOM_NONE);

        // eat end
        if (auto err = Eat(reader, FBOM_OBJECT_END)) {
            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMLoader::Handle(ByteReader *reader, FBOMCommand command, FBOMObject *parent)
{
    std::string object_type;

    // TODO: pre-saving ObjectTypes at start, then using offset

    switch (command) {
    case FBOM_OBJECT_START:
    {
        FBOMObject child;

        if (auto err = ReadObject(reader, child)) {
            return err;
        }

        last->nodes.push_back(child);

        break;
    }
    case FBOM_STATIC_DATA_START:
    {
        AssertThrow(!m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_START)) {
            return err;
        }

        m_in_static_data = true;

        // read u32 describing size of static data pool
        UInt32 static_data_size;
        reader->Read(&static_data_size);
        const auto initial_static_data_size = static_data_size;

        // skip 8 bytes of padding
        UInt64 tmp;
        reader->Read(&tmp);

        m_static_data_pool.resize(static_data_size);

        // now read each item
        //   u32 as index/offset
        //   u8 as type of static data
        //   then, the actual size of the data will vary depending on the held type
        for (UInt i = 0; i < static_data_size; i++) {
            UInt32 offset;
            reader->Read(&offset);

            if (offset >= initial_static_data_size) {
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("read offset as ") + std::to_string(offset) + ", which is >= alotted size of static data pool (" + std::to_string(static_data_size) + ")");
            }

            UInt8 type;
            reader->Read(&type);

            switch (type)
            {
            case FBOMStaticData::FBOM_STATIC_DATA_NONE:
                m_static_data_pool[offset] = FBOMStaticData();

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            {
                FBOMObject object;

                if (auto err = ReadObject(reader, object)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(object, offset);

                break;
            }
            case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
                m_static_data_pool[offset] = FBOMStaticData(ReadObjectType(reader), offset);

                break;
            case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            {
                FBOMData data;

                if (auto err = ReadData(reader, data)) {
                    return err;
                }

                m_static_data_pool[offset] = FBOMStaticData(data, offset);

                break;
            }
            default:
                return FBOMResult(FBOMResult::FBOM_ERR, std::string("Cannot process static data type ") + std::to_string(type));
            }
        }

        break;
    }
    case FBOM_STATIC_DATA_END:
    {
        AssertThrow(m_in_static_data);

        if (auto err = Eat(reader, FBOM_STATIC_DATA_END)) {
            return err;
        }

        m_in_static_data = false;

        break;
    }
    default:
        throw std::runtime_error(std::string("Cannot process command ") + std::to_string(int(command)) + " in top level");
    }

    return FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2::fbom