#pragma once
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */


#include <asset/AssetBatch.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{
    struct ManagedAssetMap
    {
        AssetMap* map;
    };
} // extern "C"

