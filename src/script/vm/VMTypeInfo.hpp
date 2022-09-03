#ifndef TYPE_INFO_HPP
#define TYPE_INFO_HPP

#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace vm {

class VMTypeInfo
{
public:
    VMTypeInfo(const char *name, SizeType size, char **names);
    VMTypeInfo(const VMTypeInfo &other);
    VMTypeInfo &operator=(const VMTypeInfo &other);
    ~VMTypeInfo();

    bool operator==(const VMTypeInfo &other) const;

    char *const GetName() const
        { return m_name; }

    SizeType GetSize() const
        { return m_size; }

    char **const GetNames() const
        { return m_names; }

    const char *GetMemberName(SizeType index) const
        { AssertThrow(index < m_size); return m_names[index]; }

private:
    char *m_name;
    SizeType m_size;
    char **m_names;
};

} // namespace vm
} // namespace hyperion

#endif
