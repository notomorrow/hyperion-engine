#ifndef BUFFERED_BYTE_READER_H
#define BUFFERED_BYTE_READER_H

#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <Util.hpp>
#include <util/Defines.hpp>
#include <util/StringUtil.hpp>
#include <core/Core.hpp>
#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>

#include <array>
#include <fstream>
#include <type_traits>
#include <thread>
#include <mutex>

namespace hyperion {
template <SizeType BufferSize, class Byte = UByte>
class BufferedReader
{
public:
    static_assert(sizeof(Byte) == 1);

    BufferedReader(const std::string &filepath, std::streampos begin = 0)
        : filepath(filepath),
          file(nullptr)
    {
        file = new std::ifstream(filepath, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();
    }

    BufferedReader(const BufferedReader &other) = delete;
    BufferedReader &operator=(const BufferedReader &other) = delete;

    BufferedReader(BufferedReader &&other) noexcept
        : filepath(std::move(other.filepath)),
          max_pos(std::move(other.max_pos)),
          pos(std::move(other.pos)),
          file(std::move(other.file)),
          buffer(std::move(other.buffer))
    {
        other.file = nullptr;
    }

    BufferedReader &operator=(BufferedReader &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        Close();

        if (file != nullptr) {
            delete file;
        }

        filepath = std::move(other.filepath);
        max_pos = std::move(other.max_pos),
        pos = std::move(other.max_pos);
        file = std::move(other.file);
        buffer = std::move(other.buffer);

        other.file    = nullptr;
        other.pos     = 0;
        other.max_pos = 0;

        return *this;
    }

    ~BufferedReader()
    {
        Close();

        delete file;
    }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    explicit operator bool() const
        { return IsOpen(); }

    const std::string &GetFilepath() const
        { return filepath; }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    bool IsOpen() const
        { return file->good(); }

    SizeType Position() const
        { return pos; }

    SizeType Max() const
        { return max_pos; }

    bool Eof() const
        { return pos >= max_pos; }

    void Rewind(unsigned long amount)
    {
        if (amount > pos) {
            pos = 0;
        } else {
            pos -= amount;
        }

        if (!Eof()) {
            file->seekg(pos -= amount);
        }
    }

    void Skip(unsigned long amount)
    {
        pos += amount;

        if (!Eof()) {
            file->seekg(pos);
        }
    }

    void Seek(unsigned long where_to)
    {
        pos = where_to;

        if (!Eof()) {
            file->seekg(pos);
        }
    }

    void Close()
    {
        if (file == nullptr) {
            return;
        }

        file->close();

        pos = max_pos; // eof
    }

    /*! \brief Reads the next \ref{count} bytes from the file and returns a ByteBuffer.
        If position + count is greater than the number of remaining bytes, the ByteBuffer is truncated. */
    ByteBuffer ReadBytes(SizeType count)
    {
        if (Eof()) {
            return {};
        }

        const SizeType remaining = max_pos - pos;
        const SizeType to_read = MathUtil::Min(remaining, count);

        ByteBuffer byte_buffer(to_read);
        file->read(reinterpret_cast<char *>(byte_buffer.Data()), to_read);
        pos += to_read;

        return byte_buffer;
    }

    /*! \brief Reads the entirety of the remaining bytes in file and returns ByteBuffer.
     * Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader! */
    ByteBuffer ReadBytes()
    {
        if (Eof()) {
            return {};
        }

        const SizeType remaining = max_pos - pos;

        ByteBuffer byte_buffer(remaining);
        file->read(reinterpret_cast<char *>(byte_buffer.Data()), remaining);
        pos += remaining;

        return byte_buffer;
    }

    /*! \brief Attempts to read \ref{count} bytes from the file into
        \ref{ptr}. If size is greater than the number of remaining bytes,
        it is capped to (num remaining bytes).
        @returns The number of bytes read */
    SizeType ReadBytes(Byte *ptr, SizeType count)
    {
        if (Eof()) {
            return 0;
        }

        const SizeType remaining = max_pos - pos;
        const SizeType to_read = MathUtil::Min(remaining, count);

        file->read(reinterpret_cast<char *>(ptr), to_read);
        pos += to_read;

        return to_read;
    }

    /*! \brief Reads the entirety of the remaining bytes in file PER LINE and returns a vector of
     * string. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    std::vector<std::string> ReadAllLines()
    {
        if (Eof()) {
            return {};
        }
        
        std::vector<std::string> lines;

        ReadLines([&lines](const std::string &line) {
            lines.push_back(line);
        });

        return lines;
    }

    SizeType Read(void *ptr, SizeType count)
    {
        return Read(ptr, count, [](void *ptr, const Byte *buffer, SizeType chunk_size) {
           Memory::Copy(ptr, buffer, chunk_size);
        });
    }

    /*! @returns The total number of bytes read */
    template <class Lambda = std::add_pointer_t<void(void *, const Byte *, SizeType)>>
    SizeType Read(void *ptr, SizeType count, Lambda &&func)
    {
        if (Eof()) {
            return 0;
        }

        SizeType total_read = 0;

        while (count) {
            const SizeType chunk_requested = MathUtil::Min(count, BufferSize);
            const SizeType chunk_returned = Read(chunk_requested);
            const SizeType offset = total_read;

            func(reinterpret_cast<void *>(uintptr_t(ptr) + offset), &buffer[0], chunk_returned);

            total_read += chunk_returned;

            if (chunk_returned < chunk_requested) {
                /* File ended */
                break;
            }

            count -= chunk_returned;
        }

        return total_read;
    }

    /*! \brief Read one instance of the given template type, using sizeof(T).
     * @param ptr The pointer to T, where the read memory will be written.
     * @returns The number of bytes read
     */
    template <class T>
    SizeType Read(T *ptr)
    {
        return Read(static_cast<void *>(ptr), sizeof(T));
    }

    template <class LambdaFunction>
    void ReadLines(LambdaFunction func, bool buffered = true)
    {
        if (!buffered) { // not buffered, do it in one pass
            auto all_bytes = ReadBytes();

            String accum;
            accum.Reserve(BufferSize);
            
            for (SizeType i = 0; i < all_bytes.Size(); i++) {
                if (all_bytes[i] == '\n') {
                    func(accum);
                    accum.Clear();
                    
                    continue;
                }

                accum.Append(static_cast<char>(all_bytes[i]));
            }

            if (accum.Any()) {
                func(accum);
            }

            return;
        }

        // if there is no newline in buffer:
        // accumulate chars until a newline is found, then
        // call func with the accumulated line
        // if there is at least one newline in buffer:
        // call func for each line in the buffer,
        // holding onto the last one (assuming no newline at the end)
        // keep that last line and continue accumulating until a newline is found
        String accum;
        accum.Reserve(BufferSize);

        ByteBuffer byte_buffer;

        while ((byte_buffer = ReadBytes(BufferSize)).Any()) {
            for (SizeType i = 0; i < byte_buffer.Size(); i++) {
                if (byte_buffer[i] == '\n') {
                    func(accum);
                    accum.Clear();
                    
                    continue;
                }

                accum.Append(static_cast<char>(byte_buffer[i]));
            }
        }

        if (accum.Any()) {
            func(accum);
        }
    }

    template <class LambdaFunction>
    void ReadChars(LambdaFunction func)
    {
        while (SizeType count = Read()) {
            for (SizeType i = 0; i < count; i++) {
                func(static_cast<char>(buffer[i]));
            }
        }
    }

private:
    std::string filepath;
    std::ifstream *file;
    std::streampos pos;
    std::streampos max_pos;
    std::array<Byte, BufferSize> buffer{};

    SizeType Read()
    {
        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), BufferSize);

        const SizeType count = file->gcount();

        AssertThrow(count <= BufferSize);

        pos += count;

        return count;
    }

    SizeType Read(SizeType sz)
    {
        AssertThrow(sz <= BufferSize);

        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), sz);

        const SizeType count = file->gcount();
        pos += count;

        return count;
    }
};

using Reader = BufferedReader<2048>;

} // namespace hyperion

#endif
