/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <Types.hpp>

namespace hyperion {

class AsyncComputeBase
{
public:
    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    RenderQueue renderQueue;
};

} // namespace hyperion
