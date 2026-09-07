/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/Baker.hpp>

#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Rendering/EnvProbeCaptureState.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/World.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<EnvProbe>::BakeJob(BakeJobParams&& params, const Handle<EnvProbe>& envProbe, BakeData<EnvProbe>* bakeData)
    : BakeJobBase(std::move(params)),
      m_envProbe(envProbe),
      m_bakeData(bakeData)
{
}

BakeJob<EnvProbe>::~BakeJob()
{
    if (m_wasStarted && m_envProbe.IsValid() && IsRaster() && m_envProbeRasterCaptureState != nullptr)
    {
        m_envProbeRasterCaptureState->End(/* commitResult */ false);
        m_envProbeRasterCaptureState.Reset();
    }
}

bool BakeJob<EnvProbe>::IsRaster() const
{
    Assert(m_envProbe.IsValid());

    return !m_envProbe->IsPathTraced();
}

bool BakeJob<EnvProbe>::IsCompleted() const
{
    if (IsRaster() && m_envProbeRasterCaptureState != nullptr)
    {
        return false;
    }

    return BakeJobBase::IsCompleted();
}

void BakeJob<EnvProbe>::Start_Internal()
{
    Assert(m_envProbe.IsValid());

    m_envProbeRasterCaptureState.Reset();

    if (IsRaster())
    {
        m_envProbeRasterCaptureState = MakeUniqueWithAllocator<EnvProbeCaptureState, BakerAllocator>(m_envProbe, m_baker->GetBakeLayerName());
        m_envProbeRasterCaptureState->Begin();
    }
}

void BakeJob<EnvProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    Assert(m_envProbe.IsValid());

    if (IsRaster())
    {
        if (World* world = m_params.scene->GetWorld())
        {
            for (uint8 viewIndex = 0; viewIndex < 6; viewIndex++)
            {
                world->ProcessViewAsync(m_envProbe->GetView(viewIndex).Get());
            }
        }

        const bool isDone = m_envProbe->GetWorld() == nullptr
            || (!m_envProbe->needsRender.Load() && m_envProbe->IsCaptureReadbackComplete());

        if (isDone && m_envProbeRasterCaptureState)
        {
            m_envProbeRasterCaptureState->End(/* commitResult */ true);
            m_envProbeRasterCaptureState.Reset();
        }

        if (outIsReadyToProcess)
        {
            *outIsReadyToProcess = isDone;
        }

        return;
    }

    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion
