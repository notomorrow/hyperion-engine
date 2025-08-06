/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeAttributes.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/math/MathUtil.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMType.hpp>
#include <core/serialization/fbom/FBOMStaticData.hpp>
#include <core/serialization/fbom/FBOMObjectLibrary.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>
#include <core/serialization/fbom/FBOMConfig.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

namespace serialization {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;
class FBOMData;
class FBOMLoadContext;

struct FBOMWriteStream
{
    Array<FBOMObject> m_objectData;
    Array<FBOMObjectLibrary> m_objectLibraries;
    HashMap<UniqueId, FBOMStaticData> m_staticData; // map hashcodes to static data to be stored.
    HashMap<UniqueId, int> m_hashUseCountMap;
    uint32 m_staticDataOffset = 0;
    bool m_isWritingStaticData : 1 = false;   // is writing to static data locked? (prevents iterator invalidation)
    bool m_objectDataWriteLocked : 1 = false; // is writing to object data locked? (prevents iterator invalidation)
    FBOMResult m_lastResult = FBOMResult::FBOM_OK;

    FBOMWriteStream();
    FBOMWriteStream(const FBOMWriteStream& other) = default;
    FBOMWriteStream& operator=(const FBOMWriteStream& other) = default;
    FBOMWriteStream(FBOMWriteStream&& other) noexcept = default;
    FBOMWriteStream& operator=(FBOMWriteStream&& other) noexcept = default;
    ~FBOMWriteStream() = default;

    FBOMDataLocation GetDataLocation(const UniqueId& uniqueId, const FBOMStaticData** outStaticData, const FBOMExternalObjectInfo** outExternalObjectInfo) const;

    HYP_FORCE_INLINE void BeginStaticDataWriting()
    {
        m_isWritingStaticData = true;
    }

    HYP_FORCE_INLINE void EndStaticDataWriting()
    {
        m_isWritingStaticData = false;
    }

    HYP_FORCE_INLINE bool IsWritingStaticData() const
    {
        return m_isWritingStaticData;
    }

    HYP_FORCE_INLINE void LockObjectDataWriting()
    {
        m_objectDataWriteLocked = true;
    }

    HYP_FORCE_INLINE void UnlockObjectDataWriting()
    {
        m_objectDataWriteLocked = false;
    }

    HYP_FORCE_INLINE bool IsObjectDataWritingLocked() const
    {
        return m_objectDataWriteLocked;
    }

    void AddToObjectLibrary(FBOMObject& object);
};

class FBOMWriter
{
public:
    FBOMWriter(const FBOMWriterConfig& config);
    FBOMWriter(const FBOMWriterConfig& config, const RC<FBOMWriteStream>& writeStream);

    FBOMWriter(const FBOMWriter& other) = delete;
    FBOMWriter& operator=(const FBOMWriter& other) = delete;

    FBOMWriter(FBOMWriter&& other) noexcept;
    FBOMWriter& operator=(FBOMWriter&& other) noexcept;

    ~FBOMWriter();

    HYP_FORCE_INLINE const FBOMWriterConfig& GetConfig() const
    {
        return m_config;
    }

    HYP_FORCE_INLINE FBOMWriteStream* GetWriteStream() const
    {
        return m_writeStream.Get();
    }

    template <class T>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T& in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject object;

        if (FBOMResult err = FBOMObject::Serialize(in, object, flags))
        {
            m_writeStream->m_lastResult = err;

            return err;
        }

        return Append(std::move(object));
    }

    FBOMResult Append(const FBOMObject& object);
    FBOMResult Append(FBOMObject&& object);

    FBOMResult Emit(ByteWriter* out, bool writeHeader = true);

    FBOMResult Write(ByteWriter* out, const FBOMObject& object, UniqueId id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter* out, const FBOMType& type, UniqueId id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter* out, const FBOMData& data, UniqueId id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter* out, const FBOMArray& array, UniqueId id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);

private:
    FBOMResult WriteArchive(ByteWriter* out, const Archive& archive) const;

    FBOMResult WriteDataAttributes(ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const;
    FBOMResult WriteDataAttributes(ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const;

    FBOMResult WriteExternalObjects(ByteWriter* out, const FilePath& basePath, const FilePath& externalPath);

    FBOMResult BuildStaticData(FBOMLoadContext& context);
    FBOMResult AddExternalObjects(FBOMLoadContext& context, FBOMObject& object);

    FBOMResult WriteHeader(ByteWriter* out);
    FBOMResult WriteStaticData(ByteWriter* out);

    FBOMResult WriteStaticDataUsage(ByteWriter* out, const FBOMStaticData&) const;

    void AddObjectData(const FBOMObject&, UniqueId id);
    void AddObjectData(FBOMObject&&, UniqueId id);

    UniqueId AddStaticData(FBOMLoadContext& context, const FBOMType&);
    UniqueId AddStaticData(FBOMLoadContext& context, const FBOMObject&);
    UniqueId AddStaticData(FBOMLoadContext& context, const FBOMData&);
    UniqueId AddStaticData(FBOMLoadContext& context, const FBOMArray&);

    UniqueId AddStaticData(UniqueId id, FBOMStaticData&& staticData);

    HYP_FORCE_INLINE UniqueId AddStaticData(FBOMStaticData&& staticData)
    {
        const UniqueId id = staticData.GetUniqueID();
        HYP_CORE_ASSERT(id != UniqueId::Invalid());

        return AddStaticData(id, std::move(staticData));
    }

    RC<FBOMWriteStream> m_writeStream;
    FBOMWriterConfig m_config;
};

} // namespace serialization
} // namespace hyperion
