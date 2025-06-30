/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMConfig.hpp>

#include <core/json/JSON.hpp>

namespace hyperion::serialization {

#pragma region FBOMWriterConfig

void FBOMWriterConfig::SaveToJSON(json::JSONValue& outJson) const
{
    json::JSONObject object = {
        { "enable_static_data", enableStaticData },
        { "compress_static_data", compressStaticData }
    };

    outJson = object;
}

bool FBOMWriterConfig::LoadFromJSON(const json::JSONValue& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    json::JSONObject object = json.AsObject();

    enableStaticData = object["enable_static_data"].ToBool();
    compressStaticData = object["compress_static_data"].ToBool();

    return true;
}

#pragma endregion FBOMWriterConfig

#pragma region FBOMReaderConfig

void FBOMReaderConfig::SaveToJSON(json::JSONValue& outJson) const
{
    json::JSONObject object = {
        { "continue_on_external_load_error", continueOnExternalLoadError },
        { "base_path", basePath }
    };

    // Don't save externalDataCache

    outJson = object;
}

bool FBOMReaderConfig::LoadFromJSON(const json::JSONValue& json)
{
    if (!json.IsObject())
    {
        return false;
    }

    json::JSONObject object = json.AsObject();

    continueOnExternalLoadError = object["continue_on_external_load_error"].ToBool();
    basePath = object["base_path"].ToString();

    return true;
}

#pragma endregion FBOMReaderConfig

} // namespace hyperion::serialization
