/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Buffers.hpp>

#include <rendering/RenderBackend.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    SafeRelease(std::move(m_buffers));
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType type, SizeType initialCount, SizeType size, SizeType alignment)
{
    if (initialCount == 0)
    {
        initialCount = 1;
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_buffers[frameIndex] = g_renderBackend->MakeGpuBuffer(type, size * initialCount, alignment);

        DeferCreate(m_buffers[frameIndex]);
    }
}

#pragma endregion GpuBufferHolderBase

} // namespace hyperion