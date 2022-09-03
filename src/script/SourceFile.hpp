#ifndef SOURCE_FILE_HPP
#define SOURCE_FILE_HPP

#include <Types.hpp>

#include <string>
#include <cstddef>

namespace hyperion {

class SourceFile
{
public:
    SourceFile();
    SourceFile(const std::string &filepath, SizeType size);
    SourceFile(const SourceFile &other);
    SourceFile &operator=(const SourceFile &other);
    ~SourceFile();

    bool IsValid() const { return m_size != 0; }

    // input into buffer
    SourceFile &operator>>(const std::string &str);

    const std::string &GetFilePath() const { return m_filepath; }
    char *GetBuffer() const { return m_buffer; }
    SizeType GetSize() const { return m_size; }
    void SetSize(SizeType size) { m_size = size; }
    
    void ReadIntoBuffer(const char *data, SizeType size);

private:
    std::string m_filepath;
    char *m_buffer;
    SizeType m_position;
    SizeType m_size;
};

} // namespace hyperion

#endif
