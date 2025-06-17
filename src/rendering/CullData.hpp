/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CULL_DATA_HPP
#define HYPERION_CULL_DATA_HPP

#include <core/math/Extent.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

struct CullData
{
    ImageViewRef depthPyramidImageView;
    Vec2u depthPyramidDimensions;

    CullData()
        : depthPyramidDimensions(Vec2u::One())
    {
    }

    CullData(const CullData& other);
    CullData& operator=(const CullData& other);

    CullData(CullData&& other) noexcept;
    CullData& operator=(CullData&& other) noexcept;

    ~CullData();

    HYP_FORCE_INLINE bool operator==(const CullData& other) const
    {
        return depthPyramidImageView == other.depthPyramidImageView
            && depthPyramidDimensions == other.depthPyramidDimensions;
    }

    HYP_FORCE_INLINE bool operator!=(const CullData& other) const
    {
        return depthPyramidImageView != other.depthPyramidImageView
            || depthPyramidDimensions != other.depthPyramidDimensions;
    }
};

} // namespace hyperion

#endif