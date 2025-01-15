/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMWriter.hpp>
#include <asset/serialization/fbom/FBOMReader.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

#include <asset/ByteWriter.hpp>

#include <core/containers/Stack.hpp>

#include <core/compression/Archive.hpp>

#include <core/util/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Constants.hpp>

#include <algorithm>

namespace hyperion::fbom {

#pragma region FBOMWriteStream

FBOMWriteStream::FBOMWriteStream() = default;

FBOMDataLocation FBOMWriteStream::GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, const FBOMExternalObjectInfo **out_external_object_info) const
{
    *out_static_data = nullptr;
    *out_external_object_info = nullptr;

    { // check static data
        auto it = m_static_data.Find(unique_id);

        if (it != m_static_data.End()) {
            if (IsWritingStaticData() || it->second.IsWritten()) {
                *out_static_data = &it->second;

                if (it->second.IsWritten()) {
                    return FBOMDataLocation::LOC_STATIC;
                }
            }
        }
    }

    // check external objects
    for (const FBOMObjectLibrary &object_library : m_object_libraries) {
        const auto objects_it = object_library.object_data.FindIf([&unique_id](const FBOMObject &item)
        {
            return item.GetUniqueID() == unique_id;
        });

        if (objects_it == object_library.object_data.End()) {
            continue;
        }

        if (!objects_it->IsExternal()) {
            break;
        }

        *out_external_object_info = objects_it->GetExternalObjectInfo();

        return FBOMDataLocation::LOC_EXT_REF;
    }

    return FBOMDataLocation::LOC_INPLACE;
}

void FBOMWriteStream::AddToObjectLibrary(FBOMObject &object)
{
    // static constexpr SizeType desired_max_size = 1024 * 1024 * 1024 * 32; // 32 MiB
    static constexpr SizeType desired_max_size = 10;

    FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
    AssertThrow(external_object_info != nullptr);
    AssertThrow(!external_object_info->IsLinked());

    // FBOMData object_data = FBOMData::FromObject(object);
    // const SizeType object_data_size = object_data.TotalSize();

    FBOMObjectLibrary *library_ptr = nullptr;

    for (auto it = m_object_libraries.Begin(); it != m_object_libraries.End(); ++it) {
        const SizeType library_size = it->CalculateTotalSize();

        if (library_size + 1 <= desired_max_size) {
            library_ptr = it;

            break;
        }
    }

    if (!library_ptr) {
        library_ptr = &m_object_libraries.EmplaceBack();
    }

    const uint32 index = library_ptr->Put(object);
    // const uint32 index = library_ptr->Put(std::move(object_data));

    external_object_info->library_id = library_ptr->uuid;
    external_object_info->index = index;
}

#pragma endregion FBOMWriteStream

#pragma region FBOMWriter

FBOMWriter::FBOMWriter(const FBOMWriterConfig &config)
    : FBOMWriter(config, MakeRefCountedPtr<FBOMWriteStream>())
{
}

FBOMWriter::FBOMWriter(const FBOMWriterConfig &config, const RC<FBOMWriteStream> &write_stream)
    : m_config(config),
      m_write_stream(write_stream)
{
}

FBOMWriter::FBOMWriter(FBOMWriter &&other) noexcept
    : m_config(std::move(other.m_config)),
      m_write_stream(std::move(other.m_write_stream))
{
}

FBOMWriter &FBOMWriter::operator=(FBOMWriter &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_config = std::move(other.m_config);
    m_write_stream = std::move(other.m_write_stream);

    return *this;
}

FBOMWriter::~FBOMWriter()
{
}

FBOMResult FBOMWriter::Append(const FBOMObject &object)
{
    AddObjectData(object, object.GetUniqueID());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Append(FBOMObject &&object)
{
    const UniqueID id = object.GetUniqueID();

    AddObjectData(std::move(object), id);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Emit(ByteWriter *out, bool write_header)
{
    if (FBOMResult err = m_write_stream->m_last_result) {
        return err;
    }

    BuildStaticData();

    if (write_header) {
        if (FBOMResult err = WriteHeader(out)) {
            return err;
        }
    }
    
    if (FBOMResult err = WriteExternalObjects(out)) {
        return err;
    }

    if (FBOMResult err = WriteStaticData(out)) {
        return err;
    }

    for (const FBOMObject &object : m_write_stream->m_object_data) {
        if (FBOMResult err = object.Visit(this, out)) {
            return err;
        }
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteExternalObjects(ByteWriter *out)
{
    if (m_write_stream->m_object_libraries.Any()) {
        DebugLog(LogType::Debug, "Writing %u external library files\n", m_write_stream->m_object_libraries.Size());
    }

    AtomicVar<bool> any_errors = false;

    Mutex mtx;

    ParallelForEach(m_write_stream->m_object_libraries, [out, &any_errors, &mtx, write_stream = m_write_stream.Get()](const FBOMObjectLibrary &library, uint32, uint32)
    {
        FBOMWriter serializer { FBOMWriterConfig { } };

        for (const FBOMObject &object : library.object_data) {
            FBOMObject object_copy(object);

            // unset to stop recursion
            object_copy.SetIsExternal(false);

            if (FBOMResult err = serializer.Append(object_copy)) {
                HYP_LOG(Serialization, Error, "Failed to write external object: {}", err.message);

                any_errors.Set(true, MemoryOrder::RELAXED);

                return;
            }
        }

        MemoryByteWriter byte_writer;

        if (FBOMResult err = serializer.Emit(&byte_writer, /* write_header */ false)) {
            HYP_LOG(Serialization, Error, "Failed to write external object: {}", err.message);

            any_errors.Set(true, MemoryOrder::RELAXED);

            return;
        }

        ByteBuffer buffer = std::move(byte_writer.GetBuffer());

        Mutex::Guard guard(mtx);

        out->Write<uint8>(FBOM_OBJECT_LIBRARY_START);

        out->Write<UUID>(library.uuid);

        out->Write<uint8>(uint8(FBOMObjectLibraryFlags::LOCATION_INLINE));

        // write size of buffer
        out->Write<uint64>(buffer.Size());

        // write actual buffer data
        out->Write(buffer.Data(), buffer.Size());

        out->Write<uint8>(FBOM_OBJECT_LIBRARY_END);
    });

    return any_errors.Get(MemoryOrder::RELAXED)
        ? FBOMResult::FBOM_ERR
        : FBOMResult::FBOM_OK;
}

void FBOMWriter::BuildStaticData()
{
    m_write_stream->LockObjectDataWriting();

    for (FBOMObject &object : m_write_stream->m_object_data) {
        AddExternalObjects(object);
    }

    for (const FBOMObject &object : m_write_stream->m_object_data) {
        // will be added as static data by other instance when it is written
        if (object.IsExternal()) {
            return;
        }
        
        AddStaticData(object);
    }

    m_write_stream->UnlockObjectDataWriting();
}

void FBOMWriter::AddExternalObjects(FBOMObject &object)
{
    if (object.IsExternal()) {
        FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(!external_object_info->IsLinked());

        m_write_stream->AddToObjectLibrary(object);

        return;
    }

    for (SizeType index = 0; index < object.nodes->Size(); index++) {
        FBOMObject &subobject = object.nodes->Get(index);

        AddExternalObjects(subobject);
    }
}

FBOMResult FBOMWriter::WriteStaticData(ByteWriter *out)
{
    EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE;

    if (m_config.compress_static_data) {
        attributes |= FBOMDataAttributes::COMPRESSED;
    }

    m_write_stream->BeginStaticDataWriting();

    Array<FBOMStaticData *> static_data_ordered;
    static_data_ordered.Reserve(m_write_stream->m_static_data.Size());

    for (auto &it : m_write_stream->m_static_data) {
        static_data_ordered.PushBack(&it.second);
    }

    std::sort(static_data_ordered.Begin(), static_data_ordered.End(), [](const FBOMStaticData *a, const FBOMStaticData *b)
    {
        return a->offset < b->offset;
    });

#ifdef HYP_DEBUG_MODE
    // sanity check: make sure no duplicate offsets
    for (SizeType i = 1; i < static_data_ordered.Size(); i++) {
        AssertThrow(static_data_ordered[i]->offset == static_data_ordered[i - 1]->offset + 1);
    }
#endif

    AssertThrowMsg(static_data_ordered.Size() == m_write_stream->m_static_data_offset,
        "Values do not match, incorrect bookkeeping");

    MemoryByteWriter static_data_byte_writer;
    Array<SizeType> static_data_buffer_offsets;
    static_data_buffer_offsets.Resize(static_data_ordered.Size());

    for (FBOMStaticData *static_data : static_data_ordered) {
        AssertThrow(static_data->offset < static_data_ordered.Size());

        const SizeType buffer_offset = static_data_byte_writer.Position();

        AssertThrowMsg(!static_data->IsWritten(),
            "Static data object has already been written: %s",
            static_data->ToString().Data());

        if (FBOMResult err = static_data->data->Visit(static_data->GetUniqueID(), this, &static_data_byte_writer, attributes)) {
            m_write_stream->EndStaticDataWriting();

            return err;
        }

        static_data->SetIsWritten(true);

        AssertThrowMsg(static_data->IsWritten(),
            "Static data object was not written: %s\t%p",
            static_data->ToString().Data(),
            static_data);

        static_data_buffer_offsets[static_data->offset] = buffer_offset;
    }

    out->Write<uint8>(FBOM_STATIC_DATA_START);

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_START);

    // write number of items as uint32
    out->Write<uint32>(uint32(static_data_ordered.Size()));

    // write buffer size
    out->Write<uint64>(uint64(static_data_byte_writer.GetBuffer().Size()));

    for (SizeType i = 0; i < static_data_ordered.Size(); i++) {
        const FBOMStaticData *static_data = static_data_ordered[i];

        // write index
        out->Write<uint32>(uint32(static_data->offset));

        // write type
        out->Write<uint8>(static_data->type);

        // write start offset
        out->Write<uint64>(static_data_buffer_offsets[i]);

        // write size of object
        if (i == static_data_ordered.Size() - 1) {
            out->Write<uint64>(static_data_byte_writer.GetBuffer().Size() - static_data_buffer_offsets[i]);
        } else {
            AssertThrow(static_data_buffer_offsets[i + 1] >= static_data_buffer_offsets[i]);

            out->Write<uint64>(static_data_buffer_offsets[i + 1] - static_data_buffer_offsets[i]);
        }
    }

    out->Write<uint8>(FBOM_STATIC_DATA_HEADER_END);

    // Write entire buffer
    out->Write(static_data_byte_writer.GetBuffer().Data(), static_data_byte_writer.GetBuffer().Size());

    out->Write<uint8>(FBOM_STATIC_DATA_END);

    m_write_stream->EndStaticDataWriting();

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteHeader(ByteWriter *out)
{
    const SizeType position_before = out->Position();

    // hyp identifier string
    out->Write(FBOM::header_identifier, sizeof(FBOM::header_identifier));

    // endianness
    out->Write<uint8>(IsBigEndian());

    // binary version
    out->Write<uint32>(FBOM::version.value);

    const SizeType position_change = SizeType(out->Position()) - position_before;
    AssertThrow(position_change <= FBOM::header_size);

    const SizeType remaining_bytes = FBOM::header_size - position_change;
    AssertThrow(remaining_bytes < 64);

    void *zeros = StackAlloc(remaining_bytes);
    Memory::MemSet(zeros, 0, remaining_bytes);

    out->Write(zeros, remaining_bytes);
    
    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMObject &object, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    AssertThrow(uint64(id) != 0);

    if (object.IsExternal()) {
        // defer writing it. instead we pass the object into our external data,
        // which we will later be write
        const FBOMExternalObjectInfo *external_object_info = object.GetExternalObjectInfo();
        AssertThrow(external_object_info != nullptr);
        AssertThrow(external_object_info->IsLinked());

        return FBOMResult::FBOM_OK;
    }

    out->Write<uint8>(FBOM_OBJECT_START);
    out->Write<uint64>(id);

    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    switch (data_location) {
    case FBOMDataLocation::LOC_STATIC:
    {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }
    case FBOMDataLocation::LOC_INPLACE:
    {
        // write typechain
        if (FBOMResult err = object.m_object_type.Visit(this, out)) {
            return err;
        }
        // add all properties
        for (const auto &it : object.properties) {
            EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE;

            if (it.second.IsCompressed()) {
                attributes |= FBOMDataAttributes::COMPRESSED;
            }

            out->Write<uint8>(FBOM_DEFINE_PROPERTY);

            // write key
            out->WriteString(it.first, BYTE_WRITER_FLAGS_WRITE_SIZE);

            // write value
            if (FBOMResult err = it.second.Visit(this, out, attributes)) {
                return err;
            }
        }

        for (FBOMObject &subobject : *object.nodes) {
            if (FBOMResult err = subobject.Visit(this, out)) {
                return err;
            }
        }

        out->Write<uint8>(FBOM_OBJECT_END);

        break;
    }
    case FBOMDataLocation::LOC_EXT_REF:
    {
        AssertThrow(external_object_info_ptr != nullptr);
        AssertThrow(external_object_info_ptr->IsLinked());

        out->Write<UUID>(external_object_info_ptr->library_id);

        // write object index as u32
        out->Write<uint32>(external_object_info_ptr->index);

        // write flags -- i.e, lazy loaded, etc.
        // not yet implemented, just write 0 for now
        out->Write<uint32>(0);

        break;
    }
    default:
        AssertThrowMsg(false, "Invalid data location type");
        break;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMType &type, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        if (type.extends != nullptr) {
            out->Write<uint8>(1);

            if (FBOMResult err = type.extends->Visit(this, out)) {
                return err;
            }
        } else {
            out->Write<uint8>(0);
        }

        // write string of object type (loader to use)
        out->WriteString(type.name, BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE);

        // write flags
        out->Write<uint8>(uint8(type.flags));

        // write size of the type
        out->Write<uint64>(type.size);

        // write native TypeID
        out->Write<TypeID::ValueType>(type.GetNativeTypeID().Value());
    } else {
        // unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMData &data, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    ByteWriter *writer_ptr = out;

    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;

    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        // write type first
        if (FBOMResult err = data.GetType().Visit(this, out)) {
            return err;
        }

        SizeType size = data.TotalSize();
        ByteBuffer byte_buffer;

        if (FBOMResult err = data.ReadBytes(size, byte_buffer)) {
            return err;
        }

        if (attributes & FBOMDataAttributes::COMPRESSED) {
            if (!Archive::IsEnabled()) {
                return { FBOMResult::FBOM_ERR, "Cannot write to archive because the Archive feature is not enabled" };
            }

            // Write compressed data
            ArchiveBuilder archive_builder;
            archive_builder.Append(byte_buffer);

            if (FBOMResult err = WriteArchive(out, archive_builder.Build())) {
                return err;
            }
        } else {
            // raw bytebuffer - write size and then buffer
            out->Write<uint32>(uint32(size));
            out->Write(byte_buffer.Data(), byte_buffer.Size());
        }
    } else {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::Write(ByteWriter *out, const FBOMArray &array, UniqueID id, EnumFlags<FBOMDataAttributes> attributes)
{
    const FBOMStaticData *static_data_ptr;
    const FBOMExternalObjectInfo *external_object_info_ptr;
    
    const FBOMDataLocation data_location = m_write_stream->GetDataLocation(id, &static_data_ptr, &external_object_info_ptr);
    
    if (FBOMResult err = WriteDataAttributes(out, attributes, data_location)) {
        return err;
    }

    if (data_location == FBOMDataLocation::LOC_STATIC) {
        AssertThrow(static_data_ptr != nullptr);

        return WriteStaticDataUsage(out, *static_data_ptr);
    }

    if (data_location == FBOMDataLocation::LOC_INPLACE) {
        // Write array size
        out->Write<uint32>(uint32(array.Size()));

        if (array.GetElementType().IsUnset()) {
            return { FBOMResult::FBOM_ERR, "Array element type is not set" };
        }

        // Write array element type
        if (FBOMResult err = array.GetElementType().Visit(this, out)) {
            return err;
        }

        ByteWriter *writer_ptr = out;
        MemoryByteWriter archive_writer;

        if (attributes & FBOMDataAttributes::COMPRESSED) {
            if (!Archive::IsEnabled()) {
                return { FBOMResult::FBOM_ERR, "Cannot write to archive because the Archive feature is not enabled" };
            }

            writer_ptr = &archive_writer;            
        }

        // Write each element
        for (SizeType index = 0; index < array.Size(); index++) {
            const FBOMData *data_ptr = array.TryGetElement(index);

            if (!data_ptr) {
                return { FBOMResult::FBOM_ERR, "Array had invalid element" };
            }

            const FBOMData &data = *data_ptr;
            SizeType data_size = data.TotalSize();

            if (data_size == 0) {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Array element at index {} (type: {}) has size 0", index, data.GetType().name) };
            }

            ByteBuffer byte_buffer;

            if (FBOMResult err = data.ReadBytes(data_size, byte_buffer)) {
                return err;
            }

            if (byte_buffer.Size() != data_size) {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Array element buffer is corrupt - expected size: {} bytes, but got {} bytes", data_size, byte_buffer.Size()) };
            }
            
            // raw bytebuffer - write size and then buffer
            writer_ptr->Write<uint32>(uint32(data_size));
            writer_ptr->Write(byte_buffer.Data(), byte_buffer.Size());
        }

        if (attributes & FBOMDataAttributes::COMPRESSED) {
            // Write compressed data
            ArchiveBuilder archive_builder;
            archive_builder.Append(std::move(archive_writer.GetBuffer()));

            if (FBOMResult err = WriteArchive(out, archive_builder.Build())) {
                return err;
            }
        }
    } else {
        // Unsupported method
        return FBOMResult::FBOM_ERR;
    }

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteArchive(ByteWriter *out, const Archive &archive) const
{
    out->Write<uint64>(archive.GetUncompressedSize());
    out->Write<uint64>(archive.GetCompressedSize());
    out->Write(archive.GetCompressedBuffer().Data(), archive.GetCompressedBuffer().Size());

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const
{
    out->Write<uint8>(uint8(attributes));

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const
{
    constexpr uint8 loc_static = (1u << uint32(FBOMDataLocation::LOC_STATIC)) << 5;
    constexpr uint8 loc_inplace = (1u << uint32(FBOMDataLocation::LOC_INPLACE)) << 5;
    constexpr uint8 loc_ext_ref = (1u << uint32(FBOMDataLocation::LOC_EXT_REF)) << 5;

    uint8 attributes_value = uint8(attributes);

    switch (location) {
    case FBOMDataLocation::LOC_STATIC:
        attributes_value |= loc_static;

        break;
    case FBOMDataLocation::LOC_INPLACE:
        attributes_value |= loc_inplace;

        break;
    case FBOMDataLocation::LOC_EXT_REF:
        attributes_value |= loc_ext_ref;

        break;
    }

    out->Write<uint8>(attributes_value);

    return FBOMResult::FBOM_OK;
}

FBOMResult FBOMWriter::WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &static_data) const
{
    const auto offset = static_data.offset;
    AssertThrow(offset < m_write_stream->m_static_data_offset);

#ifdef HYP_DEBUG_MODE
    const auto it = m_write_stream->m_static_data.Find(static_data.GetUniqueID());
    AssertThrow(it != m_write_stream->m_static_data.End());

    // AssertThrow(it->second.IsWritten());

    AssertThrow(it->second.type == static_data.type);
    AssertThrow(it->second.GetHashCode() == static_data.GetHashCode());
#endif

    out->Write<uint32>(uint32(offset));

    return FBOMResult::FBOM_OK;
}

void FBOMWriter::AddObjectData(const FBOMObject &object, UniqueID id)
{
    AssertThrow(uint64(id) != 0);

    AssertThrow(!m_write_stream->IsObjectDataWritingLocked());
    
    m_write_stream->m_object_data.PushBack(object);

    auto it = m_write_stream->m_hash_use_count_map.Find(id);

    if (it == m_write_stream->m_hash_use_count_map.End()) {
        it = m_write_stream->m_hash_use_count_map.Insert(id, 0).first;
    }

    it->second++;
}

void FBOMWriter::AddObjectData(FBOMObject &&object, UniqueID id)
{
    AssertThrow(uint64(id) != 0);

    AssertThrow(!m_write_stream->IsObjectDataWritingLocked());

    m_write_stream->m_object_data.PushBack(std::move(object));

    auto it = m_write_stream->m_hash_use_count_map.Find(id);

    if (it == m_write_stream->m_hash_use_count_map.End()) {
        it = m_write_stream->m_hash_use_count_map.Insert(id, 0).first;
    }

    it->second++;
}

UniqueID FBOMWriter::AddStaticData(UniqueID id, FBOMStaticData &&static_data)
{
    AssertThrow(!m_write_stream->IsWritingStaticData());

    auto it = m_write_stream->m_static_data.Find(id);

    if (it == m_write_stream->m_static_data.End()) { 
        static_data.SetUniqueID(id);
        static_data.offset = m_write_stream->m_static_data_offset++;

        const auto insert_result = m_write_stream->m_static_data.Insert(id, std::move(static_data));
        AssertThrow(insert_result.second);
    } else {
#if 0
        // sanity check - ensure pointing to correct object
        const HashCode hash_code = static_data.GetHashCode();
        const HashCode other_hash_code = it->second.GetHashCode();

        AssertThrowMsg(hash_code == other_hash_code,
            "Hash codes do not match: %llu != %llu\n\tLeft: %s\n\tRight: %s",
            hash_code.Value(), other_hash_code.Value(),
            static_data.ToString().Data(), it->second.ToString().Data());
#endif
    }

    return id;
}

UniqueID FBOMWriter::AddStaticData(const FBOMType &type)
{
    if (type.extends != nullptr) {
        AddStaticData(*type.extends);
    }

    return AddStaticData(FBOMStaticData(type));
}

UniqueID FBOMWriter::AddStaticData(const FBOMObject &object)
{
    // for (SizeType index = 0; index < object.nodes->Size(); index++) {
    //     FBOMObject &subobject = object.nodes->Get(index);

    //     AddStaticData(subobject);
    // }

    // for (const auto &prop : object.properties) {
    //     AddStaticData(FBOMData::FromName(prop.first));
    //     AddStaticData(prop.second);
    // }

    AddStaticData(object.GetType());

    return AddStaticData(FBOMStaticData(object));
}

UniqueID FBOMWriter::AddStaticData(const FBOMArray &array)
{
    return AddStaticData(FBOMStaticData(array));
}

UniqueID FBOMWriter::AddStaticData(const FBOMData &data)
{
    UniqueID id = UniqueID::Invalid();

    AddStaticData(data.GetType());

    if (data.IsObject()) {
        FBOMObject object;
        AssertThrowMsg(data.ReadObject(object).value == FBOMResult::FBOM_OK, "Invalid object, cannot write to stream");

        AddStaticData(object);
    } else if (data.IsArray()) {
        FBOMArray array { FBOMUnset() };
        AssertThrowMsg(data.ReadArray(array).value == FBOMResult::FBOM_OK, "Invalid array, cannot write to stream");

        AddStaticData(array);
    } else {
        HYP_FAIL("Unhandled container type");
    }

    return AddStaticData(FBOMStaticData(data));
}

#pragma endregion FBOMWriter

} // namespace hyperion::fbom