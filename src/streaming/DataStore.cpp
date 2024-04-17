/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <streaming/DataStore.hpp>
#include <asset/ByteWriter.hpp>

#include <asset/Assets.hpp>

namespace hyperion {

extern AssetManager *g_asset_manager;

DataStoreBase::DataStoreBase(const String &prefix, DataStoreOptions options)
    : m_prefix(prefix),
      m_options(options)
{
    if (m_options.flags & DATA_STORE_FLAG_WRITE) {
        AssertThrowMsg(MakeDirectory(), "Failed to create directory for data store at path %s", GetDirectory().Data());

        // Clean up the directory if it's too large
        DiscardOldFiles();
    }
}

void DataStoreBase::DiscardOldFiles() const
{
    if (m_options.max_size == 0) {
        return; // No limit
    }

    const FilePath path = GetDirectory();

    SizeType directory_size = path.DirectorySize();

    if (directory_size <= m_options.max_size) {
        return;
    }

    FlatMap<uint64, FilePath> files_by_time;

    for (const FilePath &file : path.GetAllFilesInDirectory()) {
        files_by_time.Insert(file.LastModifiedTimestamp(), file);
    }

    while (directory_size > m_options.max_size) {
        const auto it = files_by_time.Begin();

        if (it == files_by_time.End()) {
            break;
        }

        const FilePath &file = it->second;

        directory_size -= file.FileSize();

        if (!file.Remove()) {
            DebugLog(
                LogType::Warn,
                "Failed to remove file %s\n",
                file.Data()
            );
        }

        files_by_time.Erase(it);
    }
}

bool DataStoreBase::MakeDirectory() const
{
    const FilePath path = GetDirectory();

    if (!path.Exists() || !path.IsDirectory()) {
        return path.MkDir() == 0;
    }

    return true;
}

void DataStoreBase::Write(const String &key, const ByteBuffer &byte_buffer)
{
    AssertThrowMsg(m_options.flags & DATA_STORE_FLAG_WRITE, "Data store is not writable");

    const FilePath filepath = GetDirectory() / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byte_buffer.Data(), byte_buffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String &key, ByteBuffer &out_byte_buffer) const
{
    AssertThrowMsg(m_options.flags & DATA_STORE_FLAG_READ, "Data store is not readable");

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

} // namespace hyperion