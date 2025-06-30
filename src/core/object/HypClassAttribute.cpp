/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

#pragma region HypClassAttributeValue

const HypClassAttributeValue HypClassAttributeValue::empty = HypClassAttributeValue();

bool HypClassAttributeValue::IsString() const
{
    return m_value.Is<String>();
}

const String& HypClassAttributeValue::GetString() const
{
    if (!IsString())
    {
        return String::empty;
    }

    return m_value.Get<String>();
}

bool HypClassAttributeValue::IsBool() const
{
    return m_value.Is<bool>();
}

bool HypClassAttributeValue::GetBool() const
{
    if (!m_value.HasValue())
    {
        return false;
    }

    if (const bool* boolPtr = m_value.TryGet<bool>())
    {
        return *boolPtr;
    }

    if (const String* stringPtr = m_value.TryGet<String>())
    {
        return !stringPtr->Empty();
    }

    if (const int* intPtr = m_value.TryGet<int>())
    {
        return *intPtr != 0;
    }

    return true;
}

bool HypClassAttributeValue::IsInt() const
{
    return m_value.Is<int>();
}

int HypClassAttributeValue::GetInt() const
{
    if (!m_value.HasValue())
    {
        return 0;
    }

    if (const int* intPtr = m_value.TryGet<int>())
    {
        return *intPtr;
    }

    if (const String* stringPtr = m_value.TryGet<String>())
    {
        int intValue;

        if (StringUtil::Parse(*stringPtr, &intValue))
        {
            return intValue;
        }

        return 0;
    }

    if (const bool* boolPtr = m_value.TryGet<bool>())
    {
        return *boolPtr != false;
    }

    return 0;
}

String HypClassAttributeValue::ToString() const
{
    json::JSONValue jsonValue;

    if (m_value.HasValue())
    {
        Visit(m_value, [&jsonValue](auto&& value)
            {
                jsonValue = json::JSONValue(value);
            });
    }

    return jsonValue.ToString(true);
}

#pragma endregion HypClassAttributeValue

} // namespace hyperion