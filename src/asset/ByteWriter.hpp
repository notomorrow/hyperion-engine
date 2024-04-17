/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef BYTE_WRITER_HPP
#define BYTE_WRITER_HPP

#include <core/memory/Memory.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/filesystem/FilePath.hpp>
#include <Types.hpp>

#include <type_traits>
#include <fstream>
#include <string>

namespace hyperion {

using ByteWriterFlags = ubyte;

enum ByteWriterFlagBits : ByteWriterFlags
{
    BYTE_WRITER_FLAGS_NONE              = 0,
    BYTE_WRITER_FLAGS_WRITE_NULL_CHAR   = 1 << 0,
    BYTE_WRITER_FLAGS_WRITE_SIZE        = 1 << 1
};

class ByteWriter
{
public:
    ByteWriter() = default;
    ByteWriter(const ByteWriter &other) = delete;
    ByteWriter &operator=(const ByteWriter &other) = delete;
    virtual ~ByteWriter() = default;

    void Write(const void *ptr, SizeType size)
    {
        WriteBytes(reinterpret_cast<const char *>(ptr), size);
    }

    template <class T>
    void Write(const T &value)
    {
        static_assert(!std::is_pointer_v<T>, "Expected to choose other overload");

        WriteBytes(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    void WriteString(const String &str, ByteWriterFlags flags = BYTE_WRITER_FLAGS_NONE)
    {
        const uint32 len = uint32(str.Size());

        if (flags & BYTE_WRITER_FLAGS_WRITE_SIZE) {
            const uint32 len_nt = len + ((flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR) ? 1 : 0);

            WriteBytes(reinterpret_cast<const char *>(&len_nt), sizeof(uint32));
        }

        WriteBytes(str.Data(), len);

        if (flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR) {
            WriteBytes("\0", 1);
        }
    }

    void WriteString(const char *str)
    {
        const auto len = Memory::StrLen(str);

        WriteBytes(reinterpret_cast<const char *>(&len), sizeof(uint32_t));
        WriteBytes(str, len);
    }

    virtual std::streampos Position() const = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(const char *ptr, SizeType size) = 0;
};

class MemoryByteWriter : public ByteWriter
{
public:
    MemoryByteWriter() : m_pos(0) { }

    virtual ~MemoryByteWriter() override = default;

    virtual std::streampos Position() const override
    {
        return std::streampos(m_pos);
    }

    virtual void Close() override { }

    const ByteBuffer &GetBuffer() const
        { return m_buffer; }

private:
    ByteBuffer  m_buffer;
    SizeType    m_pos;

    virtual void WriteBytes(const char *ptr, SizeType size) override
    {
        m_buffer.SetSize(m_buffer.Size() + size);
        m_buffer.Write(size, m_pos, ptr);
        m_pos += size;
    }
};

class FileByteWriter : public ByteWriter
{
public:
    FileByteWriter(const FilePath &filepath, std::streampos begin = 0)
    {
        file = new std::ofstream(filepath.Data(), std::ofstream::out | std::ofstream::binary);
    }

    FileByteWriter(const FileByteWriter &other)             = delete;
    FileByteWriter &operator=(const FileByteWriter &other)  = delete;

    FileByteWriter(FileByteWriter &&other) noexcept
        : file(other.file)
    {
        other.file = nullptr;
    }

    FileByteWriter &operator=(FileByteWriter &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        delete file;

        file = other.file;
        other.file = nullptr;

        return *this;
    }

    virtual ~FileByteWriter() override
    {
        delete file;
    }

    virtual std::streampos Position() const override
    {
        AssertThrow(file != nullptr);

        return file->tellp();
    }

    virtual void Close() override
    {
        if (!file) {
            return;
        }

        file->close();
    }

    bool IsOpen() const
        { return file != nullptr && file->is_open(); }

private:
    std::ofstream *file;

    virtual void WriteBytes(const char *ptr, SizeType size) override
    {
        AssertThrow(file != nullptr);

        file->write(ptr, size);
    }
};
}

#endif
