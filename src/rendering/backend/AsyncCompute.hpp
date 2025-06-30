/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP
#define HYPERION_BACKEND_RENDERER_ASYNC_COMPUTE_HPP

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <Types.hpp>

namespace hyperion {

class AsyncComputeBase
{
public:
    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    HYP_FORCE_INLINE CmdList& GetCommandList()
    {
        return m_commandList;
    }

    HYP_FORCE_INLINE const CmdList& GetCommandList(uint32 frameIndex) const
    {
        return m_commandList;
    }

protected:
    CmdList m_commandList;
};

} // namespace hyperion

#endif