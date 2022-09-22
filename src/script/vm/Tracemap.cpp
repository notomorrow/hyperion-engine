#include <script/vm/Tracemap.hpp>
#include <system/Debug.hpp>

namespace hyperion {
namespace vm {

Tracemap::Tracemap()
    : m_stringmap(nullptr),
      m_linemap(nullptr)
{
}

Tracemap::~Tracemap()
{
    if (m_stringmap) {
        delete[] m_stringmap;
    }

    if (m_linemap) {
        delete[] m_linemap;
    }
}

void Tracemap::Set(StringmapEntry *stringmap, LinemapEntry *linemap)
{
    AssertThrow(m_stringmap == nullptr);
    AssertThrow(m_linemap == nullptr);

    m_stringmap = stringmap;
    m_linemap = linemap;
}

} // namespace vm
} // namespace hyperion
