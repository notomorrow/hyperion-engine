/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderSkeleton.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region RenderSkeleton

RenderSkeleton::RenderSkeleton(Skeleton* skeleton)
    : m_skeleton(skeleton),
      m_bufferData {}
{
}

RenderSkeleton::RenderSkeleton(RenderSkeleton&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_skeleton(other.m_skeleton),
      m_bufferData(std::move(other.m_bufferData))
{
    other.m_skeleton = nullptr;
}

RenderSkeleton::~RenderSkeleton() = default;

void RenderSkeleton::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_skeleton != nullptr);

    UpdateBufferData();
}

void RenderSkeleton::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderSkeleton::Update_Internal()
{
    HYP_SCOPE;
}

GpuBufferHolderBase* RenderSkeleton::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_SKELETONS];
}

void RenderSkeleton::UpdateBufferData()
{
    HYP_SCOPE;

    AssertThrow(m_bufferIndex != ~0u);

    *static_cast<SkeletonShaderData*>(m_bufferAddress) = m_bufferData;
    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
}

void RenderSkeleton::SetBufferData(const SkeletonShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            m_bufferData = bufferData;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

#pragma endregion RenderSkeleton

HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

} // namespace hyperion