#ifndef BUFFERED_BYTE_READER_H
#define BUFFERED_BYTE_READER_H

#include <math/math_util.h>

#include "../types.h"
#include "../util.h"
#include "../util/string_util.h"

#include <fstream>
#include <cstring>

namespace hyperion {
template <size_t BufferSize, class Byte = ubyte>
class BufferedReader {
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

    ~BufferedReader()
    {
        delete file;
    }

    inline const std::string &GetFilepath() const
        { return filepath; }

    inline bool IsOpen() const
        { return file->good(); }

    inline std::streampos Position() const
        { return pos; }

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

    /*! \brief Reads the entirety of the remaining bytes in file and returns a vector of
     * unsigned bytes. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    std::vector<Byte> ReadAll()
    {
        if (Eof()) {
            return {};
        }

        const size_t remaining = max_pos - pos;
        
        std::vector<Byte> bytes;
        bytes.resize(remaining);

        file->read(reinterpret_cast<char *>(&bytes[0]), bytes.size());
        pos += bytes.size();

        return bytes;
    }

    /*! \brief Reads the entirety of the remaining bytes in file PER LINE and returns a vector of
     * string. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    std::vector<std::string> ReadAllLines()
    {
        if (Eof()) {
            return {};
        }

        const size_t remaining = max_pos - pos;
        
        std::vector<std::string> lines;

        ReadLines([&lines](const std::string &line) {
            lines.push_back(line);
        });

        return lines;
    }

    /*! @returns The total number of bytes read */
    template <class LambdaFunction>
    size_t ReadChunked(size_t count, LambdaFunction func)
    {
        if (Eof()) {
            return 0;
        }

        size_t total_read = 0;

        while (count) {
            const size_t chunk_requested = MathUtil::Min(count, BufferSize);
            const size_t chunk_returned = Read(chunk_requested);

            func(&buffer[0], chunk_returned);

            total_read += chunk_returned;

            if (chunk_returned < chunk_requested) {
                /* File ended */
                break;
            }

            count -= chunk_returned;
        }

        return total_read;
    }

    template <class LambdaFunction>
    void ReadLines(LambdaFunction func)
    {
        // if there is no newline in buffer:
        // accumulate chars until a newline is found, then
        // call func with the accumulated line
        // if there is at least one newline in buffer:
        // call func for each line in the buffer,
        // holding onto the last one (assuming no newline at the end)
        // keep that last line and continue accumulating until a newline is found
        std::string accum;
        accum.reserve(BufferSize);

        while (size_t count = Read()) {
            for (size_t i = 0; i < count; i++) {
                if (buffer[i] == '\n') {
                    func(accum);
                    accum.clear();
                    
                    continue;
                }

                accum += buffer[i];
            }
        }

        if (accum.length() != 0) {
            func(accum);
        }
    }

    template <class LambdaFunction>
    void ReadChars(LambdaFunction func)
    {
        while (size_t count = Read()) {
            for (size_t i = 0; i < count; i++) {
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

    size_t Read()
    {
        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), BufferSize);

        const size_t count = file->gcount();

        AssertThrow(count <= BufferSize);

        pos += count;

        return count;
    }

    size_t Read(size_t sz)
    {
        AssertThrow(sz <= BufferSize);

        if (Eof()) {
            return 0;
        }

        file->read(reinterpret_cast<char *>(&buffer[0]), sz);

        const size_t count = file->gcount();
        pos += count;

        return count;
    }

    void ReadBytes(Byte *ptr, unsigned size)
    {
        file->read(reinterpret_cast<char *>(ptr), size);
        pos += size;
    }
};
}

#endif
