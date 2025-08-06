/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/Assets.hpp>

#include <scene/Node.hpp>

#include <rendering/font/FontFace.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class FontFaceLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(FontFaceLoader);

public:
    virtual ~FontFaceLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

