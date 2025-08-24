#include <script/vm/VMMap.hpp>

#include <core/debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace hyperion {
namespace vm {

VMMap::VMMap()
{
}

VMMap::VMMap(const VMMap& other)
    : m_map(other.m_map)
{
}

VMMap& VMMap::operator=(const VMMap& other)
{
    if (&other == this)
    {
        return *this;
    }

    m_map = other.m_map;

    return *this;
}

VMMap::VMMap(VMMap&& other) noexcept
    : m_map(std::move(other.m_map))
{
}

VMMap& VMMap::operator=(VMMap&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    m_map = std::move(other.m_map);

    return *this;
}

VMMap::~VMMap()
{
}

void VMMap::SetElement(VMMapKey key, VMMapValue value)
{
    m_map[key] = value;
}

VMMap::VMMapValue* VMMap::GetElement(const VMMapKey& key)
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

const VMMap::VMMapValue* VMMap::GetElement(const VMMapKey& key) const
{
    auto it = m_map.Find(key);

    if (it == m_map.End())
    {
        return nullptr;
    }

    return &it->second;
}

void VMMap::GetRepresentation(
    std::stringstream& ss,
    bool addTypeName,
    int depth) const
{
    if (depth == 0)
    {
        ss << "{...}";

        return;
    }

    // convert hashmap to string
    const char sepStr[3] = ", ";

    ss << '{';

    // all elements
    for (auto it = m_map.Begin(); it != m_map.End(); ++it)
    {
        // convert key to string
        it->first.key.ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        ss << " => ";

        // convert value to string
        it->second.ToRepresentation(
            ss,
            addTypeName,
            depth - 1);

        auto next = it;
        ++next;

        if (next != m_map.end())
        {
            ss << sepStr;
        }
    }

    ss << '}';
}

} // namespace vm
} // namespace hyperion
