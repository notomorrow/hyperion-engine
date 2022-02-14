#ifndef BUFFERED_TEXT_READER_H
#define BUFFERED_TEXT_READER_H

#include "../util.h"
#include "../util/string_util.h"

#include <fstream>
#include <cstring>

namespace hyperion {
template <size_t BufferSize>
class BufferedTextReader {
public:
    BufferedTextReader(const std::string &filepath, std::streampos begin = 0)
    {
        file = new std::ifstream(filepath, std::ifstream::in |
            std::ifstream::binary);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();

        std::memset(buffer, 0, BufferSize);
    }

    ~BufferedTextReader()
    {
        delete file;
    }

    inline bool IsOpen() const { return file->good(); }

    inline std::streampos Position() const
        { return pos; }

    inline void Rewind(unsigned long amount)
    {
        ex_assert(amount <= pos);

        file->seekg(pos -= amount);
    }

    inline void Seek(unsigned long where_to)
    {
        ex_assert(where_to <= pos);

        file->seekg(pos = where_to);
    }

    size_t Read()
    {
        if (file->eof()) {
            return 0;
        }

        file->read(buffer, BufferSize);
        buffer[BufferSize] = '\0';

        size_t count = file->gcount();

        hard_assert(count <= BufferSize);

        pos += count;

        return count;
    }

    size_t Read(char *out, size_t sz)
    {
        ex_assert(sz <= BufferSize);

        if (file->eof()) {
            return 0;
        }

        file->read(buffer, BufferSize);

        size_t count = file->gcount();

        hard_assert(count <= BufferSize);

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


        std::stringstream accum;

        while (size_t count = Read()) {
            for (size_t i = 0; i < count; i++) {
                if (buffer[i] == '\n') {
                    func(accum.str());
                    accum.str(std::string());
                    
                    continue;
                }

                accum << buffer[i];
            }
        }

        auto str = accum.str();

        if (str.length() != 0) {
            func(str);
        }
    }

private:
    std::istream *file;
    std::streampos pos;
    std::streampos max_pos;
    char buffer[BufferSize + 1];

    void ReadBytes(char *ptr, unsigned size)
    {
        file->read(ptr, size);
        pos += size;
    }
};
}

#endif
