#include <script/vm/VMMemoryBuffer.hpp>

#include <system/Debug.hpp>

#include <core/Core.hpp>
#include <sstream>

namespace hyperion {
namespace vm {

VMMemoryBuffer::VMMemoryBuffer(size_t size)
    : m_size(size),
      m_buffer(std::malloc(size))
{
}

VMMemoryBuffer::VMMemoryBuffer(const VMMemoryBuffer &other)
    : m_size(other.m_size),
      m_buffer(std::malloc(other.m_size))
{
    hyperion::Memory::Copy(m_buffer, other.m_buffer, m_size);
}

VMMemoryBuffer::~VMMemoryBuffer()
{
    std::free(m_buffer);
}

VMMemoryBuffer &VMMemoryBuffer::operator=(const VMMemoryBuffer &other)
{
    if (&other == this) {
        return *this;
    }

    if (other.m_size != m_size) {
        if (m_buffer) {
            std::free(m_buffer);
        }

        m_size = other.m_size;
        m_buffer = std::malloc(other.m_size);
    }

    // copy all objects
    hyperion::Memory::Copy(m_buffer, other.m_buffer, m_size);

    return *this;
}

void VMMemoryBuffer::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    if (depth == 0) {
        ss << "MemoryBuffer(" << m_buffer << ")\n";

        return;
    }

    // convert all array elements to string
    for (size_t i = 0; i < m_size; i++) {
        ss << "\\0x" << std::hex << static_cast<uint16_t>(reinterpret_cast<unsigned char *>(m_buffer)[i]) << std::dec;
    }
}

} // namespace vm
} // namespace hyperion
