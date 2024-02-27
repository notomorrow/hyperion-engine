#include <streaming/DataStore.hpp>
#include <asset/ByteWriter.hpp>

#include <asset/Assets.hpp>

namespace hyperion::v2 {

extern AssetManager *g_asset_manager;

DataStoreBase::DataStoreBase(const String &prefix, DataStoreFlags flags)
    : m_prefix(prefix),
      m_flags(flags)
{
    if (m_flags & DATA_STORE_FLAG_WRITE) {
        AssertThrowMsg(MakeDirectory(), "Failed to create directory for data store at path %s", GetDirectory().Data());
    }
}

bool DataStoreBase::MakeDirectory() const
{
    const FilePath path = GetDirectory();
    DebugLog(LogType::Debug, "Path: %s\n", path.Data());

    if (!path.Exists() || !path.IsDirectory()) {
        return path.MkDir() == 0;
    }

    return true;
}

void DataStoreBase::Write(const String &key, const ByteBuffer &byte_buffer)
{
    AssertThrowMsg(m_flags & DATA_STORE_FLAG_WRITE, "Data store is not writable");

    const FilePath filepath = GetDirectory() / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byte_buffer.Data(), byte_buffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String &key, ByteBuffer &out_byte_buffer) const
{
    AssertThrowMsg(m_flags & DATA_STORE_FLAG_READ, "Data store is not readable");

    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory()) {
        return false;
    }

    const FilePath filepath = directory / key;

    if (!filepath.Exists()) {
        return false;
    }
    
    BufferedByteReader reader(filepath);
    out_byte_buffer = reader.ReadBytes();

    return true;
}

FilePath DataStoreBase::GetDirectory() const
{
    return g_asset_manager->GetBasePath() / "data" / m_prefix;
}

} // namespace hyperion::v2