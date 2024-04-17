/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_JSON_LOADER_HPP
#define HYPERION_JSON_LOADER_HPP

#include <asset/Assets.hpp>
#include <util/json/JSON.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion {

using namespace json;

class JSONLoader : public AssetLoader
{
public:
    virtual ~JSONLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif