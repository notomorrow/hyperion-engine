#ifndef BUFFERED_TEXT_READER_H
#define BUFFERED_TEXT_READER_H

#include "../types.h"
#include "../util.h"
#include "../util/string_util.h"

#include <fstream>
#include <cstring>

namespace hyperion {
template <size_t BufferSize>
class BufferedTextReader {
public:
    BufferedTextReader(const std::string &filepath, std::streampos begin = 0)
        : file(nullptr)
    {
        file = new std::ifstream(filepath, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();
    }

    BufferedTextReader(const BufferedTextReader &other) = delete;
    BufferedTextReader &operator=(const BufferedTextReader &other) = delete;

    ~BufferedTextReader()
    {
        delete file;
    }

    inline bool IsOpen() const
        { return file->good(); }

    inline std::streampos Position() const
        { return pos; }

    bool Eof() const
        { return file->eof(); }

    void Rewind(unsigned long amount)
    {
        AssertThrow(amount <= pos);

        file->seekg(pos -= amount);
    }

    void Seek(unsigned long where_to)
    {
        AssertThrow(where_to <= pos);

        file->seekg(pos = where_to);
    }

    size_t Read()
    {
        if (Eof()) {
            return 0;
        }

        file->read((char *)&buffer[0], BufferSize);

        size_t count = file->gcount();

        AssertThrow(count <= BufferSize);

        pos += count;

        return count;
    }

    size_t Read(char *out, size_t sz)
    {
        AssertThrow(sz <= BufferSize);

        if (Eof()) {
            return 0;
        }

        file->read((char *)&buffer[0], BufferSize);

        size_t count = file->gcount();

        AssertExit(count <= BufferSize);

        pos += count;

        memcpy(out, buffer, sz);

        return count;
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

private:
    std::ifstream *file;
    std::streampos pos;
    std::streampos max_pos;
    std::array<ubyte, BufferSize> buffer{};

    void ReadBytes(char *ptr, unsigned size)
    {
        file->read(ptr, size);
        pos += size;
    }
};
}

#endif
