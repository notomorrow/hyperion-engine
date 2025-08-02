/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderDevice.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class CommandBufferBase : public RenderObject<CommandBufferBase>
{
public:
    virtual ~CommandBufferBase() override = default;

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

    virtual void BindVertexBuffer(const GpuBufferBase* buffer) = 0;
    virtual void BindIndexBuffer(const GpuBufferBase* buffer, GpuElemType elemType = GET_UNSIGNED_INT) = 0;

    virtual void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const = 0;

    virtual void DrawIndexedIndirect(
        const GpuBufferBase* buffer,
        uint32 bufferOffset) const = 0;
};

} // namespace hyperion
