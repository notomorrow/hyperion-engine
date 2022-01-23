#ifndef BYTE_READER_H
#define BYTE_READER_H
#include <fstream>

namespace hyperion {
class ByteReader {
public:
    virtual ~ByteReader() {}

    template <typename T>
    void Read(T *ptr, unsigned size = sizeof(T))
    {
        ReadBytes(reinterpret_cast<char*>(ptr), size);
    }

    virtual std::streampos Position() const = 0;
    virtual std::streampos Max() const = 0;
    virtual void Skip(unsigned amount) = 0;
    virtual void Seek(unsigned long where_to) = 0;
    virtual bool Eof() const = 0;

protected:
    virtual void ReadBytes(char *ptr, unsigned size) = 0;
};

class FileByteReader : public ByteReader {
public:
    FileByteReader(const std::string &filepath, std::streampos begin = 0)
    {
        file = new std::ifstream(filepath, std::ifstream::in |
            std::ifstream::binary |
            std::ifstream::ate);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();
    }

    ~FileByteReader()
    {
        delete file;
    }

    std::streampos Position() const
    {
        return pos;
    }

    std::streampos Max() const
    {
        return max_pos;
    }

    void Skip(unsigned amount)
    {
        file->seekg(pos += amount);
    }

    void Seek(unsigned long where_to)
    {
        file->seekg(pos = where_to);
    }

    bool Eof() const
    {
        return file->eof();
    }

private:
    std::istream *file;
    std::streampos pos;
    std::streampos max_pos;

    void ReadBytes(char *ptr, unsigned size)
    {
        file->read(ptr, size);
        pos += size;
    }
};
}

#endif
