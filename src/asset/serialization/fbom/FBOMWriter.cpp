#include "FBOM.hpp"
#include <asset/ByteWriter.hpp>
#include <core/lib/SortedArray.hpp>

#include <stack>
#include <algorithm>

namespace hyperion::v2::fbom {

FBOMWriter::FBOMWriter()
{
}

FBOMWriter::~FBOMWriter()
{
}

FBOMResult FBOMWriter::Append(const FBOMObject &object)
{
    AddObjectData(object);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter *out)
{
    if (auto err = m_write_stream.m_last_result) {
        return err;
    }

    BuildStaticData();
    WriteStaticDataToByteStream(out);

    for (const auto &it : m_write_stream.m_object_data) {
        if (auto err = WriteObject(out, it)) {
            m_write_stream.m_last_result = err;

            return err;
        }
    }

    if (auto err = WriteExternalObjects()) {
        return err;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects()
{
    for (const auto &it : m_write_stream.m_external_objects) {
        FBOMWriter chunk_writer;

        for (const auto &objects_it : it.second.objects) {
            FBOMObject object(objects_it.second);

            // set to empty to not keep recursing. we only write the external data once.
            object.SetExternalObjectInfo(FBOMExternalObjectInfo { });

            if (auto err = chunk_writer.Append(object)) {
                return err;
            }
        }

        FileByteWriter byte_writer(it.first.Data());
        chunk_writer.Emit(&byte_writer);
        byte_writer.Close();
    }

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    for (auto &object : m_write_stream.m_object_data) {
        Prune(object); // TODO: ensure that no invalidation occurs by m_object_data being modified
    }
}

void FBOMWriter::Prune(FBOMObject &object)
{
    if (object.IsExternal()) {
        return;
    }

    AddStaticData(object.m_object_type);

    for (auto &node : *object.nodes) {
        Prune(node);
    }

    for (auto &prop : object.properties) {
        int property_value_usage = m_write_stream.m_hash_use_count_map[prop.second.GetHashCode().Value()];

        // if (property_value_usage > 1) {
        AddStaticData(prop.second);
        // }
    }

    // convert use counts into static objects
    HashCode::ValueType hc = object.GetHashCode().Value();
    int object_usage = 0;

    auto it = m_write_stream.m_hash_use_count_map.find(hc);

    if (it != m_write_stream.m_hash_use_count_map.end()) {
        object_usage = it->second;
    }

    //if (object_usage > 1) {
       AddStaticData(object);
    //}
}

FBOMResult FBOMWriter::WriteStaticDataToByteStream(ByteWriter *out)
{
    SortedArray<FBOMStaticData> static_data_ordered;
    static_data_ordered.Reserve(m_write_stream.m_static_data.size());

    for (auto &it : m_write_stream.m_static_data) {
        static_data_ordered.Insert(it.second);
    }

    AssertThrowMsg(static_data_ordered.Size() == m_write_stream.m_static_data_offset,
        "Values do not match, incorrect bookkeeping");

    out->Write<UInt8>(FBOM_STATIC_DATA_START);

    // write SIZE of static data as uint32_t
    out->Write<UInt32>(static_data_ordered.Size());
    // 8 bytes of padding
    out->Write<UInt64>(0);

    // static data
    //   uint32_t as index/offset
    //   uint8_t as type of static data
    //   then, the actual size of the data will vary depending on the held type

    for (auto &it : static_data_ordered) {
        AssertThrow(it.offset < static_data_ordered.Size());

        out->Write<UInt32>(it.offset);
        out->Write<UInt8>(it.type);

        switch (it.type) {
        case FBOMStaticData::FBOM_STATIC_DATA_OBJECT:
            if (auto err = WriteObject(out, it.object_data)) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_TYPE:
            if (auto err = WriteObjectType(out, it.type_data)) {
                return err;
            }
            break;
        case FBOMStaticData::FBOM_STATIC_DATA_DATA:
            if (auto err = WriteData(out, it.data_data.Get())) {
                return err;
            }
            break;
        default:
            return FBOMResult(FBOMResult::FBOM_ERR, "Cannot write static object to bytestream, unknown type");
        }
    }

    out->Write<UInt8>(FBOM_STATIC_DATA_END);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObject(ByteWriter *out, const FBOMObject &object)
{
    if (object.IsExternal()) {
        // defer writing it. instead we pass the object into our external data,
        // which we will later be write

        auto &external_object = m_write_stream.m_external_objects[object.GetExternalObjectKey()];
        external_object.objects[object.GetHashCode().Value()] = object;

        // return FBOMResult::FBOM_OK;
    }

    out->Write<UInt8>(FBOM_OBJECT_START);

    FBOMStaticData static_data;
    const HashCode::ValueType hash_code = object.GetHashCode().Value();
    const auto data_location = m_write_stream.GetDataLocation(hash_code, static_data);
    out->Write<UInt8>(static_cast<UInt8>(data_location));

    switch (data_location) {
    // case FBOM_DATA_LOCATION_STATIC: // fallthrough
    // case FBOM_DATA_LOCATION_INPLACE: // fallthrough
    case FBOM_DATA_LOCATION_STATIC:
        return WriteStaticDataUsage(out, static_data);
    case FBOM_DATA_LOCATION_INPLACE:
    {
        // write typechain
        if (auto err = WriteObjectType(out, object.m_object_type)) {
            return err;
        }

        // add all properties
        for (auto &it : object.properties) {
            out->Write<UInt8>(FBOM_DEFINE_PROPERTY);

            // // write property name
            out->WriteString(it.first);

            if (auto err = WriteData(out, it.second)) {
                return err;
            }
        }

        // now write out all child nodes
        for (auto &node : *object.nodes) {
            if (auto err = WriteObject(out, node)) {
                return err;
            }
        }

        out->Write<UInt8>(FBOM_OBJECT_END);

        m_write_stream.MarkStaticDataWritten(hash_code);

        break;
    }
    case FBOM_DATA_LOCATION_EXT_REF:
    {
        const auto *external_info = object.GetExternalObjectInfo();
        AssertThrow(external_info != nullptr);

        out->WriteString(external_info->key);

        // write object index as u32
        // TODO!
        out->Write<UInt32>(0);

        // write flags -- i.e, lazy loaded, etc.
        // not yet implemented, just write 0 for now
        out->Write<UInt32>(0);

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteObjectType(ByteWriter *out, const FBOMType &type)
{
    FBOMStaticData static_data;
    const HashCode::ValueType hash_code = type.GetHashCode().Value();
    const auto data_location = m_write_stream.GetDataLocation(hash_code, static_data);
    out->Write<UInt8>(static_cast<UInt8>(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        std::stack<const FBOMType *> type_chain;
        const FBOMType *type_ptr = &type;

        while (type_ptr != nullptr) {
            type_chain.push(type_ptr);
            type_ptr = type_ptr->extends;
        }

        // TODO: assert length < max of uint8_t
        out->Write<UInt8>(static_cast<UInt8>(type_chain.size()));

        while (!type_chain.empty()) {
            // write string of object type (loader to use)
            out->WriteString(type_chain.top()->name);

            // write size of the type
            out->Write<UInt64>(static_cast<UInt64>(type_chain.top()->size));

            type_chain.pop();
        }

        m_write_stream.MarkStaticDataWritten(hash_code);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteData(ByteWriter *out, const FBOMData &data)
{
    FBOMStaticData static_data;
    const auto hash_code = data.GetHashCode().Value();
    const auto data_location = m_write_stream.GetDataLocation(hash_code, static_data);
    out->Write<UInt8>(static_cast<UInt8>(data_location));

    if (data_location == FBOM_DATA_LOCATION_STATIC) {
        return WriteStaticDataUsage(out, static_data);
    }

    if (data_location == FBOM_DATA_LOCATION_INPLACE) {
        // write type first
        if (auto err = WriteObjectType(out, data.GetType())) {
            return err;
        }

        const auto sz = data.TotalSize();

        unsigned char *bytes = new unsigned char[sz];

        data.ReadBytes(sz, bytes);

        // size of data as uint32_t
        out->Write<UInt32>(static_cast<UInt32>(sz));
        out->Write(bytes, sz);

        delete[] bytes;

        m_write_stream.MarkStaticDataWritten(hash_code);
    } else {
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    const auto offset = static_data.offset;

    out->Write<UInt32>(static_cast<UInt32>(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object)
{
    const auto hc = object.GetHashCode().Value();

    m_write_stream.m_object_data.push_back(object);

    auto it = m_write_stream.m_hash_use_count_map.find(hc);

    if (it == m_write_stream.m_hash_use_count_map.end()) {
        m_write_stream.m_hash_use_count_map[hc] = 0;
    }

    m_write_stream.m_hash_use_count_map[hc]++;
}

void FBOMWriter::AddStaticData(const FBOMType &type)
{
    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_TYPE;
    sd.type_data = type;

    const HashCode::ValueType hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }
}

void FBOMWriter::AddStaticData(const FBOMObject &object)
{
    AddStaticData(object.m_object_type);

    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_OBJECT;
    sd.object_data = object;

    const HashCode::ValueType hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }
}

void FBOMWriter::AddStaticData(const FBOMData &data)
{
    AddStaticData(data.GetType());

    FBOMStaticData sd;
    sd.type = FBOMStaticData::FBOM_STATIC_DATA_DATA;
    sd.data_data = data;

    const HashCode::ValueType hash_code = sd.GetHashCode().Value();

    auto it = m_write_stream.m_static_data.find(hash_code);

    if (it == m_write_stream.m_static_data.end()) {
        sd.offset = m_write_stream.m_static_data_offset++;
        m_write_stream.m_static_data[hash_code] = sd;
    }
}

FBOMDataLocation FBOMWriter::WriteStream::GetDataLocation(HashCode::ValueType hash_code, FBOMStaticData &out) const
{
    FBOMDataLocation data_location = FBOM_DATA_LOCATION_INPLACE;

    { // check static data
        auto it = m_static_data.find(hash_code);

        if (it != m_static_data.end() && it->second.written) {
            out = it->second;
            return FBOM_DATA_LOCATION_STATIC;
        }
    }

    // check external files
    for (auto &it : m_external_objects) {
        auto &external_data = it.second;

        const auto objects_it = external_data.objects.Find(hash_code);

        if (objects_it == external_data.objects.End()) {
            continue;
        }

        return FBOM_DATA_LOCATION_EXT_REF;
    }

    return data_location;
}

void FBOMWriter::WriteStream::MarkStaticDataWritten(HashCode::ValueType hash_code)
{
    auto it = m_static_data.find(hash_code);

    AssertThrow(it != m_static_data.end());

    m_static_data[hash_code].written = true;
}

} // namespace hyperion::v2::fbom