#include <script/vm/VMString.hpp>
#include <iostream>
namespace hyperion {
namespace vm {
  
VMString::VMString(const char *str)
{
    const SizeType len = std::strlen(str);
    m_length = len;

    m_data = new char[len + 1];
    std::strcpy(m_data, str);
    m_data[len] = '\0';
}
  
VMString::VMString(const char *str, SizeType len)
{
    m_length = len;
    m_data = new char[len + 1];
    std::strcpy(m_data, str);
    m_data[len] = '\0';
}

VMString::VMString(const VMString &other)
{
    m_length = other.m_length;
    m_data = new char[m_length + 1];
    std::strcpy(m_data, other.m_data);
    m_data[m_length] = '\0';
}


VMString &VMString::operator=(const VMString &other)
{
    if (std::addressof(other) == this) {
        return *this;
    }

    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
        m_length = 0;
    }

    if (other.m_data) {
        m_length = other.m_length;
        m_data = new char[m_length + 1];
        std::strcpy(m_data, other.m_data);
        m_data[m_length] = '\0';
    }

    return *this;
}

VMString::VMString(VMString &&other) noexcept
{
    m_data = other.m_data;
    m_length = other.m_length;

    other.m_data = nullptr;
    other.m_length = 0;
}

VMString &VMString::operator=(VMString &&other) noexcept
{
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
        m_length = 0;
    }

    m_data = other.m_data;
    m_length = other.m_length;

    other.m_data = nullptr;
    other.m_length = 0;

    return *this;
}

VMString::~VMString()
{
    if (m_data) {
        delete[] m_data;
    }
}

VMString VMString::Concat(const VMString &a, const VMString &b)
{
    const SizeType a_len = a.GetLength();
    const SizeType b_len = b.GetLength();

    const SizeType len = a_len + b_len;
    char *str = new char[len + 1];

    std::memcpy(str, a.GetData(), a_len);
    std::memcpy(&str[a_len], b.GetData(), b_len);

    VMString res(str, len);

    delete[] str;

    return res;
}

} // namespace vm
} // namespace hyperion
