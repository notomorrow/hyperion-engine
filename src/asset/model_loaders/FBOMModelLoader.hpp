/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_MODEL_LOADER_HPP
#define HYPERION_FBOM_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <asset/serialization/Serialization.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion {

class FBOMModelLoader : public AssetLoader
{
public:
    virtual ~FBOMModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif