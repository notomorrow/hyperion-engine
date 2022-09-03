#ifndef IMMUTABLE_STRING_HPP
#define IMMUTABLE_STRING_HPP

#include <Types.hpp>

#include <cstring>

namespace hyperion {
namespace vm {

class VMString {
public:
    static VMString Concat(const VMString &a, const VMString &b);

public:
    VMString(const char *str);
    VMString(const char *str, SizeType len);
    VMString(const VMString &other);
    VMString &operator=(const VMString &other);
    VMString(VMString &&other) noexcept;
    VMString &operator=(VMString &&other) noexcept;
    ~VMString();

    bool operator==(const VMString &other) const
        { return m_length == other.m_length && !std::strcmp(m_data, other.m_data); }

    const char *GetData() const
        { return m_data; }
    SizeType GetLength() const
        { return m_length; }

private:
    char *m_data;
    // cache length
    SizeType m_length;
};

} // namespace vm
} // namespace hyperion

#endif
