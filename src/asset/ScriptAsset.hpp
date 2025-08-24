/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetRegistry.hpp>

#include <scripting/Script.hpp>

namespace hyperion {

HYP_CLASS()
class ScriptAsset : public AssetObject
{
    HYP_OBJECT_BODY(ScriptAsset);

public:
    ScriptAsset() = default;

    ScriptAsset(Name name, const ScriptData& scriptData)
        : AssetObject(name, scriptData)
    {
    }

    HYP_FORCE_INLINE ScriptData* GetScriptData() const
    {
        return GetResourceData<ScriptData>();
    }
};

} // namespace hyperion
