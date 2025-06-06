/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP
#define HYPERION_BACKEND_RENDERER_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

enum CommandBufferType
{
    COMMAND_BUFFER_PRIMARY,
    COMMAND_BUFFER_SECONDARY
};

class CommandBufferBase : public RenderObject<CommandBufferBase>
{
public:
    virtual ~CommandBufferBase() override = default;

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void BindVertexBuffer(const GPUBufferBase* buffer) = 0;
    HYP_API virtual void BindIndexBuffer(const GPUBufferBase* buffer, DatumType datum_type = DatumType::UNSIGNED_INT) = 0;

    HYP_API virtual void DrawIndexed(
        uint32 num_indices,
        uint32 num_instances = 1,
        uint32 instance_index = 0) const = 0;

    HYP_API virtual void DrawIndexedIndirect(
        const GPUBufferBase* buffer,
        uint32 buffer_offset) const = 0;
};

} // namespace renderer
} // namespace hyperion

#endif