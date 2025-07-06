/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/math/Matrix4.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCamera

RenderCamera::RenderCamera(Camera* camera)
    : m_camera(camera),
      m_bufferData {}
{
}

RenderCamera::~RenderCamera()
{
}

void RenderCamera::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderCamera::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderCamera::Update_Internal()
{
    HYP_SCOPE;
}

GpuBufferHolderBase* RenderCamera::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_CAMERAS];
}

void RenderCamera::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<CameraShaderData*>(m_bufferAddress) = m_bufferData;

    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
}

void RenderCamera::SetBufferData(const CameraShaderData& bufferData)
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

void RenderCamera::ApplyJitter(const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    static const float jitterScale = 0.25f;

    Assert(m_bufferIndex != ~0u);

    const uint32 frameCounter = renderSetup.world->GetBufferData().frameCounter + 1;

    CameraShaderData& bufferData = *static_cast<CameraShaderData*>(m_bufferAddress);

    if (bufferData.projection[3][3] < MathUtil::epsilonF)
    {
        Vec4f jitter = Vec4f::Zero();

        Matrix4::Jitter(frameCounter, bufferData.dimensions.x, bufferData.dimensions.y, jitter);

        bufferData.jitter = jitter * jitterScale;

        g_renderGlobalState->gpuBuffers[GRB_CAMERAS]->MarkDirty(m_bufferIndex);
    }
}

#pragma endregion RenderCamera

} // namespace hyperion