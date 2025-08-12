/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassAttribute.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {

#pragma region HypClassAttributeValue

const HypClassAttributeValue HypClassAttributeValue::empty = HypClassAttributeValue();

bool HypClassAttributeValue::IsString() const
{
    return value.Is<String>();
}

const String& HypClassAttributeValue::GetString() const
{
    if (!IsString())
    {
        return String::empty;
    }

    return value.Get<String>();
}

bool HypClassAttributeValue::IsBool() const
{
    return value.Is<bool>();
}

bool HypClassAttributeValue::GetBool() const
{
    if (!value.HasValue())
    {
        return false;
    }

    if (const bool* boolPtr = value.TryGet<bool>())
    {
        return *boolPtr;
    }

    if (const String* stringPtr = value.TryGet<String>())
    {
        return !stringPtr->Empty();
    }

    if (const int* intPtr = value.TryGet<int>())
    {
        return *intPtr != 0;
    }

    return true;
}

bool HypClassAttributeValue::IsInt() const
{
    return value.Is<int>();
}

int HypClassAttributeValue::GetInt() const
{
    if (!value.HasValue())
    {
        return 0;
    }

    if (const int* intPtr = value.TryGet<int>())
    {
        return *intPtr;
    }

    if (const String* stringPtr = value.TryGet<String>())
    {
        int intValue;

        if (StringUtil::Parse(*stringPtr, &intValue))
        {
            return intValue;
        }

        return 0;
    }

    if (const bool* boolPtr = value.TryGet<bool>())
    {
        return *boolPtr != false;
    }

    return 0;
}

String HypClassAttributeValue::ToString() const
{
    json::JSONValue jsonValue;

    if (value.HasValue())
    {
        Visit(value, [&jsonValue](auto&& v)
            {
                jsonValue = json::JSONValue(v);
            });
    }

    return jsonValue.ToString(true);
}

#pragma endregion HypClassAttributeValue

} // namespace hyperion
