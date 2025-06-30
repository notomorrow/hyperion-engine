/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HYP_DATA_JSON_HELPERS_HPP
#define HYPERION_HYP_DATA_JSON_HELPERS_HPP

#include <core/utilities/TypeId.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Defines.hpp>

namespace hyperion {

struct HypData;
class HypClass;

namespace json {

class JSONValue;

using JSONString = String;
class JSONObject;

} // namespace json

bool JSONToHypData(const json::JSONValue& jsonValue, TypeId typeId, HypData& outHypData);
bool HypDataToJSON(const HypData& value, json::JSONValue& outJson);
bool ObjectToJSON(const HypClass* hypClass, const HypData& target, json::JSONObject& outJson);
bool JSONToObject(const json::JSONObject& jsonObject, const HypClass* hypClass, HypData& target);

} // namespace hyperion

#endif