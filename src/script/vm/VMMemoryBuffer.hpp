#ifndef MEMORY_BUFFER_HPP
#define  MEMORY_BUFFER_HPP

#include <system/Debug.hpp>
#include <Types.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class VMMemoryBuffer {
public:
    VMMemoryBuffer(SizeType size = 0);
    VMMemoryBuffer(const VMMemoryBuffer &other);
    ~VMMemoryBuffer();

    VMMemoryBuffer &operator=(const VMMemoryBuffer &other);
    bool operator==(const VMMemoryBuffer &other) const { return this == &other; }

    SizeType GetSize() const { return m_size; }
    void *GetBuffer() const { return m_buffer; }

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    SizeType m_size;
    void *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
