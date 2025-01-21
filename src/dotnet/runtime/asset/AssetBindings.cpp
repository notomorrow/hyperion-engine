/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Asset_Destroy(LoadedAsset *loaded_asset)
{
    if (!loaded_asset) {
        return;
    }

    delete loaded_asset;
}

} // extern "C"