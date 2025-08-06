/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/Assets.hpp>
#include <core/json/JSON.hpp>

#include <core/Types.hpp>

namespace hyperion {

using namespace json;

HYP_CLASS(NoScriptBindings)
class JSONLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(JSONLoader);

public:
    virtual ~JSONLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

