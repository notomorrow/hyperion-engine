/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBX_MODEL_LOADER_HPP
#define HYPERION_FBX_MODEL_LOADER_HPP

#include <asset/Assets.hpp>
#include <scene/Node.hpp>
#include <core/Containers.hpp>

#include <Types.hpp>

namespace hyperion {

class FBXModelLoader : public AssetLoader
{
public:
    virtual ~FBXModelLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion

#endif