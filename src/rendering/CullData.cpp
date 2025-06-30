/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/CullData.hpp>
#include <rendering/SafeDeleter.hpp>

namespace hyperion {

CullData::CullData(const CullData& other)
    : depthPyramidImageView(other.depthPyramidImageView),
      depthPyramidDimensions(other.depthPyramidDimensions)
{
}

CullData& CullData::operator=(const CullData& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (depthPyramidImageView != other.depthPyramidImageView)
    {
        SafeRelease(std::move(depthPyramidImageView));

        depthPyramidImageView = other.depthPyramidImageView;
    }

    depthPyramidDimensions = other.depthPyramidDimensions;

    return *this;
}

CullData::CullData(CullData&& other) noexcept
    : depthPyramidImageView(std::move(other.depthPyramidImageView)),
      depthPyramidDimensions(other.depthPyramidDimensions)
{
    other.depthPyramidDimensions = Vec2u::One();
}

CullData& CullData::operator=(CullData&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (depthPyramidImageView != other.depthPyramidImageView)
    {
        SafeRelease(std::move(depthPyramidImageView));

        depthPyramidImageView = std::move(other.depthPyramidImageView);
    }

    depthPyramidDimensions = other.depthPyramidDimensions;
    other.depthPyramidDimensions = Vec2u::One();

    return *this;
}

CullData::~CullData()
{
    SafeRelease(std::move(depthPyramidImageView));
}

} // namespace hyperion